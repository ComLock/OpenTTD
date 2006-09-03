/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "order.h"
#include "airport.h"
#include "depot.h"
#include "functions.h"
#include "table/strings.h"
#include "vehicle.h"
#include "waypoint.h"
#include "command.h"
#include "station.h"
#include "player.h"
#include "news.h"
#include "saveload.h"
#include "vehicle_gui.h"

enum {
	/* Max orders: 64000 (64 * 1000) */
	ORDER_POOL_BLOCK_SIZE_BITS = 6,       /* In bits, so (1 << 6) == 64 */
	ORDER_POOL_MAX_BLOCKS      = 1000,
};

/**
 * Called if a new block is added to the order-pool
 */
static void OrderPoolNewBlock(uint start_item)
{
	Order *order;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (order = GetOrder(start_item); order != NULL; order = (order->index + 1 < GetOrderPoolSize()) ? GetOrder(order->index + 1) : NULL) order->index = start_item++;
}

/* Initialize the order-pool */
MemoryPool _order_pool = { "Orders", ORDER_POOL_MAX_BLOCKS, ORDER_POOL_BLOCK_SIZE_BITS, sizeof(Order), &OrderPoolNewBlock, NULL, 0, 0, NULL };

/**
 *
 * Unpacks a order from savegames made with TTD(Patch)
 *
 */
Order UnpackOldOrder(uint16 packed)
{
	Order order;
	order.type    = GB(packed, 0, 4);
	order.flags   = GB(packed, 4, 4);
	order.dest    = GB(packed, 8, 8);
	order.next    = NULL;

	// Sanity check
	// TTD stores invalid orders as OT_NOTHING with non-zero flags/station
	if (order.type == OT_NOTHING && (order.flags != 0 || order.dest != 0)) {
		order.type = OT_DUMMY;
		order.flags = 0;
	}

	return order;
}

/**
 *
 * Unpacks a order from savegames with version 4 and lower
 *
 */
static Order UnpackVersion4Order(uint16 packed)
{
	Order order;
	order.type  = GB(packed, 0, 4);
	order.flags = GB(packed, 4, 4);
	order.dest  = GB(packed, 8, 8);
	order.next  = NULL;
	order.index = 0; // avoid compiler warning
	return order;
}

/**
 *
 * Updates the widgets of a vehicle which contains the order-data
 *
 */
void InvalidateVehicleOrder(const Vehicle *v)
{
	InvalidateWindow(WC_VEHICLE_VIEW,   v->index);
	InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
}

/**
 *
 * Swap two orders
 *
 */
static void SwapOrders(Order *order1, Order *order2)
{
	Order temp_order;

	temp_order = *order1;
	AssignOrder(order1, *order2);
	order1->next = order2->next;
	AssignOrder(order2, temp_order);
	order2->next = temp_order.next;
}

/**
 *
 * Allocate a new order
 *
 * @return Order* if a free space is found, else NULL.
 *
 */
static Order *AllocateOrder(void)
{
	Order *order;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (order = GetOrder(0); order != NULL; order = (order->index + 1 < GetOrderPoolSize()) ? GetOrder(order->index + 1) : NULL) {
		if (!IsValidOrder(order)) {
			OrderID index = order->index;

			memset(order, 0, sizeof(*order));
			order->index = index;
			order->next = NULL;

			return order;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_order_pool)) return AllocateOrder();

	return NULL;
}

/**
 *
 * Assign data to an order (from an other order)
 *   This function makes sure that the index is maintained correctly
 *
 */
void AssignOrder(Order *order, Order data)
{
	order->type  = data.type;
	order->flags = data.flags;
	order->dest  = data.dest;
}


/**
 * Delete all news items regarding defective orders about a vehicle
 * This could kill still valid warnings (for example about void order when just
 * another order gets added), but assume the player will notice the problems,
 * when (s)he's changing the orders.
 */
static void DeleteOrderWarnings(const Vehicle* v)
{
	DeleteVehicleNews(v->index, STR_TRAIN_HAS_TOO_FEW_ORDERS  + (v->type - VEH_Train) * 4);
	DeleteVehicleNews(v->index, STR_TRAIN_HAS_VOID_ORDER      + (v->type - VEH_Train) * 4);
	DeleteVehicleNews(v->index, STR_TRAIN_HAS_DUPLICATE_ENTRY + (v->type - VEH_Train) * 4);
	DeleteVehicleNews(v->index, STR_TRAIN_HAS_INVALID_ENTRY   + (v->type - VEH_Train) * 4);
}


/** Add an order to the orderlist of a vehicle.
 * @param tile unused
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 - 15) - ID of the vehicle
 * - p1 = (bit 16 - 31) - the selected order (if any). If the last order is given,
 *                        the order will be inserted before that one
 *                        only the first 8 bits used currently (bit 16 - 23) (max 255)
 * @param p2 packed order to insert
 */
int32 CmdInsertOrder(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	VehicleID veh   = GB(p1,  0, 16);
	VehicleOrderID sel_ord = GB(p1, 16, 16);
	Order new_order = UnpackOrder(p2);

	if (!IsValidVehicleID(veh)) return CMD_ERROR;

	v = GetVehicle(veh);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* Check if the inserted order is to the correct destination (owner, type),
	 * and has the correct flags if any */
	switch (new_order.type) {
		case OT_GOTO_STATION: {
			const Station *st;

			if (!IsValidStationID(new_order.dest)) return CMD_ERROR;
			st = GetStation(new_order.dest);

			if (st->airport_type != AT_OILRIG && !IsBuoy(st) && !CheckOwnership(st->owner)) {
				return CMD_ERROR;
			}

			switch (v->type) {
				case VEH_Train:
					if (!(st->facilities & FACIL_TRAIN)) return CMD_ERROR;
					break;

				case VEH_Road:
					if (v->cargo_type == CT_PASSENGERS) {
						if (!(st->facilities & FACIL_BUS_STOP)) return CMD_ERROR;
					} else {
						if (!(st->facilities & FACIL_TRUCK_STOP)) return CMD_ERROR;
					}
					break;

				case VEH_Ship:
					if (!(st->facilities & FACIL_DOCK)) return CMD_ERROR;
					break;

				case VEH_Aircraft:
					if (!(st->facilities & FACIL_AIRPORT)) return CMD_ERROR;
					break;

				default: return CMD_ERROR;
			}

			/* Order flags can be any of the following for stations:
			 * [full-load | unload] [+ transfer] [+ non-stop]
			 * non-stop orders (if any) are only valid for trains */
			switch (new_order.flags) {
				case 0:
				case OF_FULL_LOAD:
				case OF_FULL_LOAD | OF_TRANSFER:
				case OF_UNLOAD:
				case OF_UNLOAD | OF_TRANSFER:
				case OF_TRANSFER:
					break;

				case OF_NON_STOP:
				case OF_NON_STOP | OF_FULL_LOAD:
				case OF_NON_STOP | OF_FULL_LOAD | OF_TRANSFER:
				case OF_NON_STOP | OF_UNLOAD:
				case OF_NON_STOP | OF_UNLOAD | OF_TRANSFER:
				case OF_NON_STOP | OF_TRANSFER:
					if (v->type != VEH_Train) return CMD_ERROR;
					break;

				default: return CMD_ERROR;
			}
			break;
		}

		case OT_GOTO_DEPOT: {
			if (v->type == VEH_Aircraft) {
				const Station* st;

				if (!IsValidStationID(new_order.dest)) return CMD_ERROR;
				st = GetStation(new_order.dest);

				if ((st->airport_type != AT_OILRIG && !CheckOwnership(st->owner)) ||
						!(st->facilities & FACIL_AIRPORT) ||
						GetAirport(st->airport_type)->nof_depots == 0) {
					return CMD_ERROR;
				}
			} else {
				const Depot* dp;

				if (!IsValidDepotID(new_order.dest)) return CMD_ERROR;
				dp = GetDepot(new_order.dest);

				if (!CheckOwnership(GetTileOwner(dp->xy))) return CMD_ERROR;

				switch (v->type) {
					case VEH_Train:
						if (!IsTileDepotType(dp->xy, TRANSPORT_RAIL)) return CMD_ERROR;
						break;

					case VEH_Road:
						if (!IsTileDepotType(dp->xy, TRANSPORT_ROAD)) return CMD_ERROR;
						break;

					case VEH_Ship:
						if (!IsTileDepotType(dp->xy, TRANSPORT_WATER)) return CMD_ERROR;
						break;

					default: return CMD_ERROR;
				}
			}

			/* Order flags can be any of the following for depots:
			 * order [+ halt] [+ non-stop]
			 * non-stop orders (if any) are only valid for trains */
			switch (new_order.flags) {
				case OF_PART_OF_ORDERS:
				case OF_PART_OF_ORDERS | OF_HALT_IN_DEPOT:
					break;

				case OF_NON_STOP | OF_PART_OF_ORDERS:
				case OF_NON_STOP | OF_PART_OF_ORDERS | OF_HALT_IN_DEPOT:
					if (v->type != VEH_Train) return CMD_ERROR;
					break;

				default: return CMD_ERROR;
			}
			break;
		}

		case OT_GOTO_WAYPOINT: {
			const Waypoint* wp;

			if (v->type != VEH_Train) return CMD_ERROR;

			if (!IsValidWaypointID(new_order.dest)) return CMD_ERROR;
			wp = GetWaypoint(new_order.dest);

			if (!CheckOwnership(GetTileOwner(wp->xy))) return CMD_ERROR;

			/* Order flags can be any of the following for waypoints:
			 * [non-stop]
			 * non-stop orders (if any) are only valid for trains */
			switch (new_order.flags) {
				case 0: break;

				case OF_NON_STOP:
					if (v->type != VEH_Train) return CMD_ERROR;
					break;

				default: return CMD_ERROR;
			}
			break;
		}

		default: return CMD_ERROR;
	}

	if (sel_ord > v->num_orders) return CMD_ERROR;

	if (IsOrderPoolFull()) return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

	/* XXX - This limit is only here because the backuppedorders can't
	 * handle any more then this.. */
	if (v->num_orders >= MAX_BACKUP_ORDER_COUNT) return_cmd_error(STR_8832_TOO_MANY_ORDERS);

	/* For ships, make sure that the station is not too far away from the
	 * previous destination, for human players with new pathfinding disabled */
	if (v->type == VEH_Ship && IS_HUMAN_PLAYER(v->owner) &&
		sel_ord != 0 && GetVehicleOrder(v, sel_ord - 1)->type == OT_GOTO_STATION
		&& !_patches.new_pathfinding_all) {

		int dist = DistanceManhattan(
			GetStation(GetVehicleOrder(v, sel_ord - 1)->dest)->xy,
			GetStation(new_order.dest)->xy // XXX type != OT_GOTO_STATION?
		);
		if (dist >= 130)
			return_cmd_error(STR_0210_TOO_FAR_FROM_PREVIOUS_DESTINATIO);
	}

	if (flags & DC_EXEC) {
		Vehicle *u;
		Order *new = AllocateOrder();
		AssignOrder(new, new_order);

		/* Create new order and link in list */
		if (v->orders == NULL) {
			v->orders = new;
		} else {
			/* Try to get the previous item (we are inserting above the
			    selected) */
			Order *order = GetVehicleOrder(v, sel_ord - 1);

			if (order == NULL && GetVehicleOrder(v, sel_ord) != NULL) {
				/* There is no previous item, so we are altering v->orders itself
				    But because the orders can be shared, we copy the info over
				    the v->orders, so we don't have to change the pointers of
				    all vehicles */
				SwapOrders(v->orders, new);
				/* Now update the next pointers */
				v->orders->next = new;
			} else if (order == NULL) {
				/* 'sel' is a non-existing order, add him to the end */
				order = GetLastVehicleOrder(v);
				order->next = new;
			} else {
				/* Put the new order in between */
				new->next = order->next;
				order->next = new;
			}
		}

		u = GetFirstVehicleFromSharedList(v);
		DeleteOrderWarnings(u);
		for (; u != NULL; u = u->next_shared) {
			/* Increase amount of orders */
			u->num_orders++;

			/* If the orderlist was empty, assign it */
			if (u->orders == NULL) u->orders = v->orders;

			assert(v->orders == u->orders);

			/* If there is added an order before the current one, we need
			to update the selected order */
			if (sel_ord <= u->cur_order_index) {
				uint cur = u->cur_order_index + 1;
				/* Check if we don't go out of bound */
				if (cur < u->num_orders)
					u->cur_order_index = cur;
			}
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u);
		}

		/* Make sure to rebuild the whole list */
		RebuildVehicleLists();
	}

	return 0;
}

/** Declone an order-list
 * @param *dst delete the orders of this vehicle
 * @param flags execution flags
 */
static int32 DecloneOrder(Vehicle *dst, uint32 flags)
{
	if (flags & DC_EXEC) {
		DeleteVehicleOrders(dst);
		InvalidateVehicleOrder(dst);
		RebuildVehicleLists();
	}
	return 0;
}

/** Delete an order from the orderlist of a vehicle.
 * @param tile unused
 * @param p1 the ID of the vehicle
 * @param p2 the order to delete (max 255)
 */
int32 CmdDeleteOrder(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v, *u;
	VehicleID veh_id = p1;
	VehicleOrderID sel_ord = p2;
	Order *order;

	if (!IsValidVehicleID(veh_id)) return CMD_ERROR;

	v = GetVehicle(veh_id);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* If we did not select an order, we maybe want to de-clone the orders */
	if (sel_ord >= v->num_orders)
		return DecloneOrder(v, flags);

	order = GetVehicleOrder(v, sel_ord);
	if (order == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (GetVehicleOrder(v, sel_ord - 1) == NULL) {
			if (GetVehicleOrder(v, sel_ord + 1) != NULL) {
				/* First item, but not the last, so we need to alter v->orders
				    Because we can have shared order, we copy the data
				    from the next item over the deleted */
				order = GetVehicleOrder(v, sel_ord + 1);
				SwapOrders(v->orders, order);
			} else {
				/* Last item, so clean the list */
				v->orders = NULL;
			}
		} else {
			GetVehicleOrder(v, sel_ord - 1)->next = order->next;
		}

		/* Give the item free */
		order->type = OT_NOTHING;
		order->next = NULL;

		u = GetFirstVehicleFromSharedList(v);
		DeleteOrderWarnings(u);
		for (; u != NULL; u = u->next_shared) {
			u->num_orders--;

			if (sel_ord < u->cur_order_index)
				u->cur_order_index--;

			/* If we removed the last order, make sure the shared vehicles
			 * also set their orders to NULL */
			if (v->orders == NULL) u->orders = NULL;

			assert(v->orders == u->orders);

			/* NON-stop flag is misused to see if a train is in a station that is
			 * on his order list or not */
			if (sel_ord == u->cur_order_index && u->current_order.type == OT_LOADING &&
					HASBIT(u->current_order.flags, OFB_NON_STOP)) {
				u->current_order.flags = 0;
			}

			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u);
		}

		RebuildVehicleLists();
	}

	return 0;
}

/** Goto next order of order-list.
 * @param tile unused
 * @param p1 The ID of the vehicle which order is skipped
 * @param p2 unused
 */
int32 CmdSkipOrder(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	VehicleID veh_id = p1;

	if (!IsValidVehicleID(veh_id)) return CMD_ERROR;

	v = GetVehicle(veh_id);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Goto next order */
		VehicleOrderID b = v->cur_order_index + 1;
		if (b >= v->num_orders) b = 0;

		v->cur_order_index = b;

		if (v->type == VEH_Train) v->u.rail.days_since_order_progr = 0;

		if (v->type == VEH_Road) ClearSlot(v);

		/* NON-stop flag is misused to see if a train is in a station that is
		 * on his order list or not */
		if (v->current_order.type == OT_LOADING && HASBIT(v->current_order.flags, OFB_NON_STOP))
			v->current_order.flags = 0;

		InvalidateVehicleOrder(v);
	}

	/* We have an aircraft/ship, they have a mini-schedule, so update them all */
	if (v->type == VEH_Aircraft) InvalidateWindowClasses(WC_AIRCRAFT_LIST);
	if (v->type == VEH_Ship) InvalidateWindowClasses(WC_SHIPS_LIST);

	return 0;
}


/** Modify an order in the orderlist of a vehicle.
 * @param tile unused
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 - 15) - ID of the vehicle
 * - p1 = (bit 16 - 31) - the selected order (if any). If the last order is given,
 *                        the order will be inserted before that one
 *                        only the first 8 bits used currently (bit 16 - 23) (max 255)
 * @param p2 mode to change the order to (always set)
 */
int32 CmdModifyOrder(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	Order *order;
	VehicleOrderID sel_ord = GB(p1, 16, 16); // XXX - automatically truncated to 8 bits.
	VehicleID veh   = GB(p1,  0, 16);

	if (!IsValidVehicleID(veh)) return CMD_ERROR;
	if (p2 != OFB_FULL_LOAD && p2 != OFB_UNLOAD && p2 != OFB_NON_STOP && p2 != OFB_TRANSFER) return CMD_ERROR;

	v = GetVehicle(veh);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	/* Is it a valid order? */
	if (sel_ord >= v->num_orders) return CMD_ERROR;

	order = GetVehicleOrder(v, sel_ord);
	if (order->type != OT_GOTO_STATION &&
			(order->type != OT_GOTO_DEPOT    || p2 == OFB_UNLOAD) &&
			(order->type != OT_GOTO_WAYPOINT || p2 != OFB_NON_STOP)) {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		switch (p2) {
		case OFB_FULL_LOAD:
			TOGGLEBIT(order->flags, OFB_FULL_LOAD);
			if (order->type != OT_GOTO_DEPOT) CLRBIT(order->flags, OFB_UNLOAD);
			break;
		case OFB_UNLOAD:
			TOGGLEBIT(order->flags, OFB_UNLOAD);
			CLRBIT(order->flags, OFB_FULL_LOAD);
			break;
		case OFB_NON_STOP:
			TOGGLEBIT(order->flags, OFB_NON_STOP);
			break;
		case OFB_TRANSFER:
			TOGGLEBIT(order->flags, OFB_TRANSFER);
			break;
		default: NOT_REACHED();
		}

		/* Update the windows and full load flags, also for vehicles that share the same order list */
		{
			Vehicle* u;

			u = GetFirstVehicleFromSharedList(v);
			DeleteOrderWarnings(u);
			for (; u != NULL; u = u->next_shared) {
				/* toggle u->current_order "Full load" flag if it changed */
				if (sel_ord == u->cur_order_index &&
						HASBIT(u->current_order.flags, OFB_FULL_LOAD) != HASBIT(order->flags, OFB_FULL_LOAD)) {
					TOGGLEBIT(u->current_order.flags, OFB_FULL_LOAD);
				}
				InvalidateVehicleOrder(u);
			}
		}
	}

	return 0;
}

/** Clone/share/copy an order-list of an other vehicle.
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0-15) - destination vehicle to clone orders to (p1 & 0xFFFF)
 * - p1 = (bit 16-31) - source vehicle to clone orders from, if any (none for CO_UNSHARE)
 * @param p2 mode of cloning: CO_SHARE, CO_COPY, or CO_UNSHARE
 */
int32 CmdCloneOrder(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *dst;
	VehicleID veh_src = GB(p1, 16, 16);
	VehicleID veh_dst = GB(p1,  0, 16);

	if (!IsValidVehicleID(veh_dst)) return CMD_ERROR;

	dst = GetVehicle(veh_dst);

	if (!CheckOwnership(dst->owner)) return CMD_ERROR;

	switch (p2) {
		case CO_SHARE: {
			Vehicle *src;

			if (!IsValidVehicleID(veh_src)) return CMD_ERROR;

			src = GetVehicle(veh_src);

			/* Sanity checks */
			if (!CheckOwnership(src->owner) || dst->type != src->type || dst == src)
				return CMD_ERROR;

			/* Trucks can't share orders with busses (and visa versa) */
			if (src->type == VEH_Road) {
				if (src->cargo_type != dst->cargo_type && (src->cargo_type == CT_PASSENGERS || dst->cargo_type == CT_PASSENGERS))
					return CMD_ERROR;
			}

			/* Is the vehicle already in the shared list? */
			{
				const Vehicle* u;

				for (u = GetFirstVehicleFromSharedList(src); u != NULL; u = u->next_shared) {
					if (u == dst) return CMD_ERROR;
				}
			}

			if (flags & DC_EXEC) {
				/* If the destination vehicle had a OrderList, destroy it */
				DeleteVehicleOrders(dst);

				dst->orders = src->orders;
				dst->num_orders = src->num_orders;

				/* Link this vehicle in the shared-list */
				dst->next_shared = src->next_shared;
				dst->prev_shared = src;
				if (src->next_shared != NULL) src->next_shared->prev_shared = dst;
				src->next_shared = dst;

				InvalidateVehicleOrder(dst);
				InvalidateVehicleOrder(src);

				RebuildVehicleLists();
			}
		} break;

		case CO_COPY: {
			Vehicle *src;
			int delta;

			if (!IsValidVehicleID(veh_src)) return CMD_ERROR;

			src = GetVehicle(veh_src);

			/* Sanity checks */
			if (!CheckOwnership(src->owner) || dst->type != src->type || dst == src)
				return CMD_ERROR;

			/* Trucks can't copy all the orders from busses (and visa versa) */
			if (src->type == VEH_Road) {
				const Order *order;
				TileIndex required_dst = INVALID_TILE;

				FOR_VEHICLE_ORDERS(src, order) {
					if (order->type == OT_GOTO_STATION) {
						const Station *st = GetStation(order->dest);
						if (dst->cargo_type == CT_PASSENGERS) {
							if (st->bus_stops != NULL) required_dst = st->bus_stops->xy;
						} else {
							if (st->truck_stops != NULL) required_dst = st->truck_stops->xy;
						}
						/* This station has not the correct road-bay, so we can't copy! */
						if (required_dst == INVALID_TILE)
							return CMD_ERROR;
					}
				}
			}

			/* make sure there are orders available */
			delta = IsOrderListShared(dst) ? src->num_orders + 1 : src->num_orders - dst->num_orders;
			if (!HasOrderPoolFree(delta))
				return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

			if (flags & DC_EXEC) {
				const Order *order;
				Order **order_dst;

				/* If the destination vehicle had a OrderList, destroy it */
				DeleteVehicleOrders(dst);

				order_dst = &dst->orders;
				FOR_VEHICLE_ORDERS(src, order) {
					*order_dst = AllocateOrder();
					AssignOrder(*order_dst, *order);
					order_dst = &(*order_dst)->next;
				}

				dst->num_orders = src->num_orders;

				InvalidateVehicleOrder(dst);

				RebuildVehicleLists();
			}
		} break;

		case CO_UNSHARE: return DecloneOrder(dst, flags);
		default: return CMD_ERROR;
	}

	return 0;
}

/**
 *
 * Backup a vehicle order-list, so you can replace a vehicle
 *  without loosing the order-list
 *
 */
void BackupVehicleOrders(const Vehicle *v, BackuppedOrders *bak)
{
	/* Save general info */
	bak->orderindex       = v->cur_order_index;
	bak->service_interval = v->service_interval;

	/* Safe custom string, if any */
	if ((v->string_id & 0xF800) != 0x7800) {
		bak->name[0] = '\0';
	} else {
		GetName(v->string_id & 0x7FF, bak->name);
	}

	/* If we have shared orders, store it on a special way */
	if (IsOrderListShared(v)) {
		const Vehicle *u = (v->next_shared) ? v->next_shared : v->prev_shared;

		bak->clone = u->index;
	} else {
		/* Else copy the orders */
		Order *order, *dest;

		dest = bak->order;

		/* We do not have shared orders */
		bak->clone = INVALID_VEHICLE;

		/* Copy the orders */
		FOR_VEHICLE_ORDERS(v, order) {
			*dest = *order;
			dest++;
		}
		/* End the list with an OT_NOTHING */
		dest->type = OT_NOTHING;
		dest->next = NULL;
	}
}

/**
 *
 * Restore vehicle orders that are backupped via BackupVehicleOrders
 *
 */
void RestoreVehicleOrders(const Vehicle* v, const BackuppedOrders* bak)
{
	uint i;

	/* If we have a custom name, process that */
	if (bak->name[0] != 0) {
		_cmd_text = bak->name;
		DoCommandP(0, v->index, 0, NULL, CMD_NAME_VEHICLE);
	}

	/* If we had shared orders, recover that */
	if (bak->clone != INVALID_VEHICLE) {
		DoCommandP(0, v->index | (bak->clone << 16), 0, NULL, CMD_CLONE_ORDER);
		return;
	}

	/* CMD_NO_TEST_IF_IN_NETWORK is used here, because CMD_INSERT_ORDER checks if the
	    order number is one more than the current amount of orders, and because
	    in network the commands are queued before send, the second insert always
	    fails in test mode. By bypassing the test-mode, that no longer is a problem. */
	for (i = 0; bak->order[i].type != OT_NOTHING; i++) {
		if (!DoCommandP(0, v->index + (i << 16), PackOrder(&bak->order[i]), NULL, CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK))
			break;
	}

	/* Restore vehicle order-index and service interval */
	DoCommandP(0, v->index, bak->orderindex | (bak->service_interval << 16) , NULL, CMD_RESTORE_ORDER_INDEX);
}

/** Restore the current order-index of a vehicle and sets service-interval.
 * @param tile unused
 * @param p1 the ID of the vehicle
 * @param p2 various bistuffed elements
 * - p2 = (bit  0-15) - current order-index (p2 & 0xFFFF)
 * - p2 = (bit 16-31) - service interval (p2 >> 16)
 * @todo Unfortunately you cannot safely restore the unitnumber or the old vehicle
 * as far as I can see. We can store it in BackuppedOrders, and restore it, but
 * but we have no way of seeing it has been tampered with or not, as we have no
 * legit way of knowing what that ID was.@n
 * If we do want to backup/restore it, just add UnitID uid to BackuppedOrders, and
 * restore it as parameter 'y' (ugly hack I know) for example. "v->unitnumber = y;"
 */
int32 CmdRestoreOrderIndex(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	VehicleOrderID cur_ord = GB(p2,  0, 16);
	uint16 serv_int = GB(p2, 16, 16);

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	/* Check the vehicle type and ownership, and if the service interval and order are in range */
	if (!CheckOwnership(v->owner)) return CMD_ERROR;
	if (serv_int != GetServiceIntervalClamped(serv_int) || cur_ord >= v->num_orders) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->cur_order_index = cur_ord;
		v->service_interval = serv_int;
	}

	return 0;
}


static TileIndex GetStationTileForVehicle(const Vehicle* v, const Station* st)
{
	switch (v->type) {
		default: NOT_REACHED();
		case VEH_Train:     return st->train_tile;
		case VEH_Aircraft:  return st->airport_tile;
		case VEH_Ship:      return st->dock_tile;
		case VEH_Road:
			if (v->cargo_type == CT_PASSENGERS) {
				return (st->bus_stops != NULL) ? st->bus_stops->xy : 0;
			} else {
				return (st->truck_stops != NULL) ? st->truck_stops->xy : 0;
			}
	}
}


/**
 *
 * Check the orders of a vehicle, to see if there are invalid orders and stuff
 *
 */
void CheckOrders(const Vehicle* v)
{
	/* Does the user wants us to check things? */
	if (_patches.order_review_system == 0) return;

	/* Do nothing for crashed vehicles */
	if (v->vehstatus & VS_CRASHED) return;

	/* Do nothing for stopped vehicles if setting is '1' */
	if (_patches.order_review_system == 1 && v->vehstatus & VS_STOPPED)
		return;

	/* do nothing we we're not the first vehicle in a share-chain */
	if (v->next_shared != NULL) return;

	/* Only check every 20 days, so that we don't flood the message log */
	if (v->owner == _local_player && v->day_counter % 20 == 0) {
		int n_st, problem_type = -1;
		const Order *order;
		int message = 0;

		/* Check the order list */
		n_st = 0;

		FOR_VEHICLE_ORDERS(v, order) {
			/* Dummy order? */
			if (order->type == OT_DUMMY) {
				problem_type = 1;
				break;
			}
			/* Does station have a load-bay for this vehicle? */
			if (order->type == OT_GOTO_STATION) {
				const Station* st = GetStation(order->dest);
				TileIndex required_tile = GetStationTileForVehicle(v, st);

				n_st++;
				if (required_tile == 0) problem_type = 3;
			}
		}

		/* Check if the last and the first order are the same */
		if (v->num_orders > 1) {
			const Order* last = GetLastVehicleOrder(v);

			if (v->orders->type  == last->type &&
					v->orders->flags == last->flags &&
					v->orders->dest  == last->dest) {
				problem_type = 2;
			}
		}

		/* Do we only have 1 station in our order list? */
		if (n_st < 2 && problem_type == -1) problem_type = 0;

		/* We don't have a problem */
		if (problem_type < 0) return;

		message = STR_TRAIN_HAS_TOO_FEW_ORDERS + ((v->type - VEH_Train) << 2) + problem_type;
		//DEBUG(misc, 3) ("Triggered News Item for %d", v->index);

		SetDParam(0, v->unitnumber);
		AddNewsItem(
			message,
			NEWS_FLAGS(NM_SMALL, NF_VIEWPORT | NF_VEHICLE, NT_ADVICE, 0),
			v->index,
			0
		);
	}
}

/**
 * Removes an order from all vehicles. Triggers when, say, a station is removed.
 * @param type The type of the order (OT_GOTO_[STATION|DEPOT|WAYPOINT]).
 * @param destination The destination. Can be a StationID, DepotID or WaypointID.
 */
void RemoveOrderFromAllVehicles(OrderType type, DestinationID destination)
{
	Vehicle *v;

	/* Aircraft have StationIDs for depot orders and never use DepotIDs
	 * This fact is handled specially below
	 */

	/* Go through all vehicles */
	FOR_ALL_VEHICLES(v) {
		Order *order;
		bool invalidate;

		if (v->orders == NULL) continue;

		/* Forget about this station if this station is removed */
		if (v->last_station_visited == destination && type == OT_GOTO_STATION) {
			v->last_station_visited = INVALID_STATION;
		}

		order = &v->current_order;
		if ((v->type == VEH_Aircraft && order->type == OT_GOTO_DEPOT ? OT_GOTO_STATION : order->type) == type &&
				v->current_order.dest == destination) {
			order->type = OT_DUMMY;
			order->flags = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}

		/* Clear the order from the order-list */
		invalidate = false;
		FOR_VEHICLE_ORDERS(v, order) {
			if ((v->type == VEH_Aircraft && order->type == OT_GOTO_DEPOT ? OT_GOTO_STATION : order->type) == type &&
					order->dest == destination) {
				order->type = OT_DUMMY;
				order->flags = 0;
				invalidate = true;
			}
		}

		/* Only invalidate once, and if needed */
		if (invalidate) InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
	}
}

/**
 *
 * Checks if a vehicle has a GOTO_DEPOT in his order list
 *
 * @return True if this is true (lol ;))
 *
 */
bool VehicleHasDepotOrders(const Vehicle *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) {
		if (order->type == OT_GOTO_DEPOT)
			return true;
	}

	return false;
}

/**
 *
 * Delete all orders from a vehicle
 *
 */
void DeleteVehicleOrders(Vehicle *v)
{
	Order *cur, *next;

	DeleteOrderWarnings(v);

	/* If we have a shared order-list, don't delete the list, but just
	    remove our pointer */
	if (IsOrderListShared(v)) {
		const Vehicle *u = v;

		v->orders = NULL;
		v->num_orders = 0;

		/* Unlink ourself */
		if (v->prev_shared != NULL) {
			v->prev_shared->next_shared = v->next_shared;
			u = v->prev_shared;
		}
		if (v->next_shared != NULL) {
			v->next_shared->prev_shared = v->prev_shared;
			u = v->next_shared;
		}
		v->prev_shared = NULL;
		v->next_shared = NULL;

		/* We only need to update this-one, because if there is a third
		    vehicle which shares the same order-list, nothing will change. If
		    this is the last vehicle, the last line of the order-window
		    will change from Shared order list, to Order list, so it needs
		    an update */
		InvalidateVehicleOrder(u);
		return;
	}

	/* Remove the orders */
	cur = v->orders;
	v->orders = NULL;
	v->num_orders = 0;

		while (cur != NULL) {
		next = cur->next;
		DeleteOrder(cur);
		cur = next;
	}
}

/**
 *
 * Check if we share our orders with an other vehicle
 *
 * @return Returns the vehicle who has the same order
 *
 */
bool IsOrderListShared(const Vehicle *v)
{
	return v->next_shared != NULL || v->prev_shared != NULL;
}

/**
 *
 * Check if a vehicle has any valid orders
 *
 * @return false if there are no valid orders
 *
 */
bool CheckForValidOrders(const Vehicle* v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) if (order->type != OT_DUMMY) return true;

	return false;
}

void InitializeOrders(void)
{
	CleanPool(&_order_pool);
	AddBlockToPool(&_order_pool);

	_backup_orders_tile = 0;
}

static const SaveLoad _order_desc[] = {
	SLE_VAR(Order, type,  SLE_UINT8),
	SLE_VAR(Order, flags, SLE_UINT8),
	SLE_VAR(Order, dest,  SLE_UINT16),
	SLE_REF(Order, next,  REF_ORDER),

	// reserve extra space in savegame here. (currently 10 bytes)
	SLE_CONDNULL(10, 5, SL_MAX_VERSION),
	SLE_END()
};

static void Save_ORDR(void)
{
	Order *order;

	FOR_ALL_ORDERS(order) {
		SlSetArrayIndex(order->index);
		SlObject(order, _order_desc);
	}
}

static void Load_ORDR(void)
{
	if (CheckSavegameVersionOldStyle(5, 2)) {
		/* Version older than 5.2 did not have a ->next pointer. Convert them
		    (in the old days, the orderlist was 5000 items big) */
		uint len = SlGetFieldLength();
		uint i;

		if (CheckSavegameVersion(5)) {
			/* Pre-version 5 had an other layout for orders
			    (uint16 instead of uint32) */
			uint16 orders[5000];

			len /= sizeof(uint16);
			assert (len <= lengthof(orders));

			SlArray(orders, len, SLE_UINT16);

			for (i = 0; i < len; ++i) {
				if (!AddBlockIfNeeded(&_order_pool, i))
					error("Orders: failed loading savegame: too many orders");

				AssignOrder(GetOrder(i), UnpackVersion4Order(orders[i]));
			}
		} else if (CheckSavegameVersionOldStyle(5, 2)) {
			uint32 orders[5000];

			len /= sizeof(uint32);
			assert (len <= lengthof(orders));

			SlArray(orders, len, SLE_UINT32);

			for (i = 0; i < len; ++i) {
				if (!AddBlockIfNeeded(&_order_pool, i))
					error("Orders: failed loading savegame: too many orders");

				AssignOrder(GetOrder(i), UnpackOrder(orders[i]));
			}
		}

		/* Update all the next pointer */
		for (i = 1; i < len; ++i) {
			/* The orders were built like this:
			     Vehicle one had order[0], and as long as order++.type was not
			     OT_NOTHING, it was part of the order-list of that vehicle */
			if (GetOrder(i)->type != OT_NOTHING)
				GetOrder(i - 1)->next = GetOrder(i);
		}
	} else {
		int index;

		while ((index = SlIterateArray()) != -1) {
			Order *order;

			if (!AddBlockIfNeeded(&_order_pool, index))
				error("Orders: failed loading savegame: too many orders");

			order = GetOrder(index);
			SlObject(order, _order_desc);
		}
	}
}

const ChunkHandler _order_chunk_handlers[] = {
	{ 'ORDR', Save_ORDR, Load_ORDR, CH_ARRAY | CH_LAST},
};
