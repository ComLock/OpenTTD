/* $Id$ */

/** @file vehicle.cpp Base implementations of all vehicles. */

#include "stdafx.h"
#include "openttd.h"
#include "road_map.h"
#include "roadveh.h"
#include "ship.h"
#include "spritecache.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "timetable.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "news_func.h"
#include "command_func.h"
#include "saveload.h"
#include "player_func.h"
#include "debug.h"
#include "vehicle_gui.h"
#include "rail_type.h"
#include "train.h"
#include "aircraft.h"
#include "industry_map.h"
#include "station_map.h"
#include "water_map.h"
#include "yapf/yapf.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_station.h"
#include "group.h"
#include "order_func.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "functions.h"
#include "date_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "signal_func.h"
#include "sound_func.h"
#include "variables.h"
#include "autoreplace_func.h"
#include "autoreplace_gui.h"
#include "string_func.h"
#include "settings_type.h"
#include "oldpool_func.h"
#include "depot_map.h"
#include "animated_tile_func.h"
#include "effectvehicle_base.h"
#include "core/alloc_func.hpp"
#include "vehiclelist.h"

#include "table/sprites.h"
#include "table/strings.h"

#define INVALID_COORD (0x7fffffff)
#define GEN_HASH(x, y) ((GB((y), 6, 6) << 6) + GB((x), 7, 6))

VehicleID _vehicle_id_ctr_day;
const Vehicle *_place_clicked_vehicle;
VehicleID _new_vehicle_id;
uint16 _returned_refit_capacity;


/* Tables used in vehicle.h to find the right command for a certain vehicle type */
const uint32 _veh_build_proc_table[] = {
	CMD_BUILD_RAIL_VEHICLE,
	CMD_BUILD_ROAD_VEH,
	CMD_BUILD_SHIP,
	CMD_BUILD_AIRCRAFT,
};
const uint32 _veh_sell_proc_table[] = {
	CMD_SELL_RAIL_WAGON,
	CMD_SELL_ROAD_VEH,
	CMD_SELL_SHIP,
	CMD_SELL_AIRCRAFT,
};

const uint32 _veh_refit_proc_table[] = {
	CMD_REFIT_RAIL_VEHICLE,
	CMD_REFIT_ROAD_VEH,
	CMD_REFIT_SHIP,
	CMD_REFIT_AIRCRAFT,
};

const uint32 _send_to_depot_proc_table[] = {
	CMD_SEND_TRAIN_TO_DEPOT,
	CMD_SEND_ROADVEH_TO_DEPOT,
	CMD_SEND_SHIP_TO_DEPOT,
	CMD_SEND_AIRCRAFT_TO_HANGAR,
};


/* Initialize the vehicle-pool */
DEFINE_OLD_POOL_GENERIC(Vehicle, Vehicle)

/** Function to tell if a vehicle needs to be autorenewed
 * @param *p The vehicle owner
 * @return true if the vehicle is old enough for replacement
 */
bool Vehicle::NeedsAutorenewing(const Player *p) const
{
	/* We can always generate the Player pointer when we have the vehicle.
	 * However this takes time and since the Player pointer is often present
	 * when this function is called then it's faster to pass the pointer as an
	 * argument rather than finding it again. */
	assert(p == GetPlayer(this->owner));

	if (!p->engine_renew) return false;
	if (this->age - this->max_age < (p->engine_renew_months * 30)) return false;
	if (this->age == 0) return false; // rail cars don't age and lacks a max age

	return true;
}

void VehicleServiceInDepot(Vehicle *v)
{
	v->date_of_last_service = _date;
	v->breakdowns_since_last_service = 0;
	v->reliability = GetEngine(v->engine_type)->reliability;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index); // ensure that last service date and reliability are updated
}

bool Vehicle::NeedsServicing() const
{
	if (this->vehstatus & (VS_STOPPED | VS_CRASHED)) return false;

	if (_settings_game.order.no_servicing_if_no_breakdowns && _settings_game.difficulty.vehicle_breakdowns == 0) {
		/* Vehicles set for autoreplacing needs to go to a depot even if breakdowns are turned off.
		 * Note: If servicing is enabled, we postpone replacement till next service. */
		return EngineHasReplacementForPlayer(GetPlayer(this->owner), this->engine_type, this->group_id);
	}

	return _settings_game.vehicle.servint_ispercent ?
		(this->reliability < GetEngine(this->engine_type)->reliability * (100 - this->service_interval) / 100) :
		(this->date_of_last_service + this->service_interval < _date);
}

bool Vehicle::NeedsAutomaticServicing() const
{
	if (_settings_game.order.gotodepot && VehicleHasDepotOrders(this)) return false;
	if (this->current_order.IsType(OT_LOADING))            return false;
	if (this->current_order.IsType(OT_GOTO_DEPOT) && this->current_order.GetDepotOrderType() != ODTFB_SERVICE) return false;
	return NeedsServicing();
}

StringID VehicleInTheWayErrMsg(const Vehicle* v)
{
	switch (v->type) {
		case VEH_TRAIN:    return STR_8803_TRAIN_IN_THE_WAY;
		case VEH_ROAD:     return STR_9000_ROAD_VEHICLE_IN_THE_WAY;
		case VEH_AIRCRAFT: return STR_A015_AIRCRAFT_IN_THE_WAY;
		default:           return STR_980E_SHIP_IN_THE_WAY;
	}
}

static void *EnsureNoVehicleProcZ(Vehicle *v, void *data)
{
	byte z = *(byte*)data;

	if (v->type == VEH_DISASTER || (v->type == VEH_AIRCRAFT && v->subtype == AIR_SHADOW)) return NULL;
	if (v->z_pos > z) return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}

Vehicle *FindVehicleOnTileZ(TileIndex tile, byte z)
{
	return (Vehicle*)VehicleFromPos(tile, &z, &EnsureNoVehicleProcZ);
}

bool EnsureNoVehicleOnGround(TileIndex tile)
{
	return FindVehicleOnTileZ(tile, GetTileMaxZ(tile)) == NULL;
}

Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z, bool without_crashed)
{
	int x1 = TileX(from);
	int y1 = TileY(from);
	int x2 = TileX(to);
	int y2 = TileY(to);
	Vehicle *veh;

	/* Make sure x1 < x2 or y1 < y2 */
	if (x1 > x2 || y1 > y2) {
		Swap(x1, x2);
		Swap(y1, y2);
	}
	FOR_ALL_VEHICLES(veh) {
		if (without_crashed && (veh->vehstatus & VS_CRASHED) != 0) continue;
		if ((veh->type == VEH_TRAIN || veh->type == VEH_ROAD) && (z == 0xFF || veh->z_pos == z)) {
			if ((veh->x_pos >> 4) >= x1 && (veh->x_pos >> 4) <= x2 &&
					(veh->y_pos >> 4) >= y1 && (veh->y_pos >> 4) <= y2) {
				return veh;
			}
		}
	}
	return NULL;
}


/** Procedure called for every vehicle found in tunnel/bridge in the hash map */
static void *GetVehicleTunnelBridgeProc(Vehicle *v, void *data)
{
	if (v->type != VEH_TRAIN && v->type != VEH_ROAD && v->type != VEH_SHIP) return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}

/**
 * Finds vehicle in tunnel / bridge
 * @param tile first end
 * @param endtile second end
 * @return pointer to vehicle found
 */
Vehicle *GetVehicleTunnelBridge(TileIndex tile, TileIndex endtile)
{
	Vehicle *v = (Vehicle*)VehicleFromPos(tile, NULL, &GetVehicleTunnelBridgeProc);
	if (v != NULL) return v;

	return (Vehicle*)VehicleFromPos(endtile, NULL, &GetVehicleTunnelBridgeProc);
}


static void UpdateVehiclePosHash(Vehicle *v, int x, int y);

void VehiclePositionChanged(Vehicle *v)
{
	int img = v->cur_image;
	Point pt = RemapCoords(v->x_pos + v->x_offs, v->y_pos + v->y_offs, v->z_pos);
	const Sprite *spr = GetSprite(img);

	pt.x += spr->x_offs;
	pt.y += spr->y_offs;

	UpdateVehiclePosHash(v, pt.x, pt.y);

	v->left_coord = pt.x;
	v->top_coord = pt.y;
	v->right_coord = pt.x + spr->width + 2;
	v->bottom_coord = pt.y + spr->height + 2;
}

/** Called after load to update coordinates */
void AfterLoadVehicles(bool clear_te_id)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		/* Reinstate the previous pointer */
		if (v->Next() != NULL) v->Next()->previous = v;

		v->UpdateDeltaXY(v->direction);

		if (clear_te_id) v->fill_percent_te_id = INVALID_TE_ID;
		v->first = NULL;
		if (v->type == VEH_TRAIN) v->u.rail.first_engine = INVALID_ENGINE;
		if (v->type == VEH_ROAD)  v->u.road.first_engine = INVALID_ENGINE;

		v->cargo.InvalidateCache();
	}

	FOR_ALL_VEHICLES(v) {
		/* Fill the first pointers */
		if (v->Previous() == NULL) {
			for (Vehicle *u = v; u != NULL; u = u->Next()) {
				u->first = v;
			}
		}
	}

	FOR_ALL_VEHICLES(v) {
		assert(v->first != NULL);

		if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v))) {
			if (IsFrontEngine(v)) v->u.rail.last_speed = v->cur_speed; // update displayed train speed
			TrainConsistChanged(v, false);
		} else if (v->type == VEH_ROAD && IsRoadVehFront(v)) {
			RoadVehUpdateCache(v);
		}
	}

	FOR_ALL_VEHICLES(v) {
		switch (v->type) {
			case VEH_ROAD:
				v->u.road.roadtype = HasBit(EngInfo(v->engine_type)->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
				v->u.road.compatible_roadtypes = RoadTypeToRoadTypes(v->u.road.roadtype);
				/* FALL THROUGH */
			case VEH_TRAIN:
			case VEH_SHIP:
				v->cur_image = v->GetImage(v->direction);
				break;

			case VEH_AIRCRAFT:
				if (IsNormalAircraft(v)) {
					v->cur_image = v->GetImage(v->direction);

					/* The plane's shadow will have the same image as the plane */
					Vehicle *shadow = v->Next();
					shadow->cur_image = v->cur_image;

					/* In the case of a helicopter we will update the rotor sprites */
					if (v->subtype == AIR_HELICOPTER) {
						Vehicle *rotor = shadow->Next();
						rotor->cur_image = GetRotorImage(v);
					}

					UpdateAircraftCache(v);
				}
				break;
			default: break;
		}

		v->left_coord = INVALID_COORD;
		VehiclePositionChanged(v);
	}
}

Vehicle::Vehicle()
{
	this->type               = VEH_INVALID;
	this->left_coord         = INVALID_COORD;
	this->group_id           = DEFAULT_GROUP;
	this->fill_percent_te_id = INVALID_TE_ID;
	this->first              = this;
	this->colormap           = PAL_NONE;
}

/**
 * Get a value for a vehicle's random_bits.
 * @return A random value from 0 to 255.
 */
byte VehicleRandomBits()
{
	return GB(Random(), 0, 8);
}


/* static */ bool Vehicle::AllocateList(Vehicle **vl, int num)
{
	uint counter = _Vehicle_pool.first_free_index;

	for (int i = 0; i != num; i++) {
		Vehicle *v = AllocateRaw(counter);

		if (v == NULL) return false;
		v = new (v) InvalidVehicle();

		if (vl != NULL) {
			vl[i] = v;
		}
		counter++;
	}

	return true;
}

/* Size of the hash, 6 = 64 x 64, 7 = 128 x 128. Larger sizes will (in theory) reduce hash
 * lookup times at the expense of memory usage. */
const int HASH_BITS = 7;
const int HASH_SIZE = 1 << HASH_BITS;
const int HASH_MASK = HASH_SIZE - 1;
const int TOTAL_HASH_SIZE = 1 << (HASH_BITS * 2);
const int TOTAL_HASH_MASK = TOTAL_HASH_SIZE - 1;

/* Resolution of the hash, 0 = 1*1 tile, 1 = 2*2 tiles, 2 = 4*4 tiles, etc.
 * Profiling results show that 0 is fastest. */
const int HASH_RES = 0;

static Vehicle *_new_vehicle_position_hash[TOTAL_HASH_SIZE];

static void *VehicleFromHash(int xl, int yl, int xu, int yu, void *data, VehicleFromPosProc *proc)
{
	for (int y = yl; ; y = (y + (1 << HASH_BITS)) & (HASH_MASK << HASH_BITS)) {
		for (int x = xl; ; x = (x + 1) & HASH_MASK) {
			Vehicle *v = _new_vehicle_position_hash[(x + y) & TOTAL_HASH_MASK];
			for (; v != NULL; v = v->next_new_hash) {
				void *a = proc(v, data);
				if (a != NULL) return a;
			}
			if (x == xu) break;
		}
		if (y == yu) break;
	}

	return NULL;
}


void *VehicleFromPosXY(int x, int y, void *data, VehicleFromPosProc *proc)
{
	const int COLL_DIST = 6;

	/* Hash area to scan is from xl,yl to xu,yu */
	int xl = GB((x - COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS);
	int xu = GB((x + COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS);
	int yl = GB((y - COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS) << HASH_BITS;
	int yu = GB((y + COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS) << HASH_BITS;

	return VehicleFromHash(xl, yl, xu, yu, data, proc);
}


void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	int x = GB(TileX(tile), HASH_RES, HASH_BITS);
	int y = GB(TileY(tile), HASH_RES, HASH_BITS) << HASH_BITS;

	Vehicle *v = _new_vehicle_position_hash[(x + y) & TOTAL_HASH_MASK];
	for (; v != NULL; v = v->next_new_hash) {
		if (v->tile != tile) continue;

		void *a = proc(v, data);
		if (a != NULL) return a;
	}

	return NULL;
}

static void UpdateNewVehiclePosHash(Vehicle *v, bool remove)
{
	Vehicle **old_hash = v->old_new_hash;
	Vehicle **new_hash;

	if (remove) {
		new_hash = NULL;
	} else {
		int x = GB(TileX(v->tile), HASH_RES, HASH_BITS);
		int y = GB(TileY(v->tile), HASH_RES, HASH_BITS) << HASH_BITS;
		new_hash = &_new_vehicle_position_hash[(x + y) & TOTAL_HASH_MASK];
	}

	if (old_hash == new_hash) return;

	/* Remove from the old position in the hash table */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		Vehicle *u = *old_hash;
		while (u != v) {
			last = u;
			u = u->next_new_hash;
			assert(u != NULL);
		}

		if (last == NULL) {
			*old_hash = v->next_new_hash;
		} else {
			last->next_new_hash = v->next_new_hash;
		}
	}

	/* Insert vehicle at beginning of the new position in the hash table */
	if (new_hash != NULL) {
		v->next_new_hash = *new_hash;
		*new_hash = v;
		assert(v != v->next_new_hash);
	}

	/* Remember current hash position */
	v->old_new_hash = new_hash;
}

static Vehicle *_vehicle_position_hash[0x1000];

static void UpdateVehiclePosHash(Vehicle *v, int x, int y)
{
	UpdateNewVehiclePosHash(v, x == INVALID_COORD);

	Vehicle **old_hash, **new_hash;
	int old_x = v->left_coord;
	int old_y = v->top_coord;

	new_hash = (x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(x, y)];
	old_hash = (old_x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(old_x, old_y)];

	if (old_hash == new_hash) return;

	/* remove from hash table? */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		Vehicle *u = *old_hash;
		while (u != v) {
			last = u;
			u = u->next_hash;
			assert(u != NULL);
		}

		if (last == NULL) {
			*old_hash = v->next_hash;
		} else {
			last->next_hash = v->next_hash;
		}
	}

	/* insert into hash table? */
	if (new_hash != NULL) {
		v->next_hash = *new_hash;
		*new_hash = v;
	}
}

void ResetVehiclePosHash()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) { v->old_new_hash = NULL; }
	memset(_vehicle_position_hash, 0, sizeof(_vehicle_position_hash));
	memset(_new_vehicle_position_hash, 0, sizeof(_new_vehicle_position_hash));
}

void ResetVehicleColorMap()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) { v->colormap = PAL_NONE; }
}

void InitializeVehicles()
{
	_Vehicle_pool.CleanPool();
	_Vehicle_pool.AddBlockToPool();

	ResetVehiclePosHash();
}

Vehicle *GetLastVehicleInChain(Vehicle *v)
{
	while (v->Next() != NULL) v = v->Next();
	return v;
}

const Vehicle *GetLastVehicleInChain(const Vehicle *v)
{
	while (v->Next() != NULL) v = v->Next();
	return v;
}

uint CountVehiclesInChain(const Vehicle* v)
{
	uint count = 0;
	do count++; while ((v = v->Next()) != NULL);
	return count;
}

/** Check if a vehicle is counted in num_engines in each player struct
 * @param *v Vehicle to test
 * @return true if the vehicle is counted in num_engines
 */
bool IsEngineCountable(const Vehicle *v)
{
	switch (v->type) {
		case VEH_AIRCRAFT: return IsNormalAircraft(v); // don't count plane shadows and helicopter rotors
		case VEH_TRAIN:
			return !IsArticulatedPart(v) && // tenders and other articulated parts
			!IsRearDualheaded(v); // rear parts of multiheaded engines
		case VEH_ROAD: return IsRoadVehFront(v);
		case VEH_SHIP: return true;
		default: return false; // Only count player buildable vehicles
	}
}

void Vehicle::PreDestructor()
{
	if (CleaningPool()) return;

	if (IsValidStationID(this->last_station_visited)) {
		GetStation(this->last_station_visited)->loading_vehicles.remove(this);

		HideFillingPercent(this->fill_percent_te_id);
		this->fill_percent_te_id = INVALID_TE_ID;
	}

	if (IsEngineCountable(this)) {
		GetPlayer(this->owner)->num_engines[this->engine_type]--;
		if (this->owner == _local_player) InvalidateAutoreplaceWindow(this->engine_type, this->group_id);

		if (IsValidGroupID(this->group_id)) GetGroup(this->group_id)->num_engines[this->engine_type]--;
		if (this->IsPrimaryVehicle()) DecreaseGroupNumVehicle(this->group_id);
	}

	if (this->type == VEH_ROAD) ClearSlot(this);

	if (this->type != VEH_TRAIN || (this->type == VEH_TRAIN && (IsFrontEngine(this) || IsFreeWagon(this)))) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, this->tile);
	}

	this->cargo.Truncate(0);
	DeleteVehicleOrders(this);

	/* Now remove any artic part. This will trigger an other
	 *  destroy vehicle, which on his turn can remove any
	 *  other artic parts. */
	if ((this->type == VEH_TRAIN && EngineHasArticPart(this)) || (this->type == VEH_ROAD && RoadVehHasArticPart(this))) {
		delete this->Next();
	}

	extern void StopGlobalFollowVehicle(const Vehicle *v);
	StopGlobalFollowVehicle(this);
}

Vehicle::~Vehicle()
{
	free(this->name);

	if (CleaningPool()) return;

	this->SetNext(NULL);
	UpdateVehiclePosHash(this, INVALID_COORD, 0);
	this->next_hash = NULL;
	this->next_new_hash = NULL;

	DeleteVehicleNews(this->index, INVALID_STRING_ID);

	new (this) InvalidVehicle();
}

/**
 * Deletes all vehicles in a chain.
 * @param v The first vehicle in the chain.
 */
void DeleteVehicleChain(Vehicle *v)
{
	assert(v->First() == v);

	do {
		Vehicle *u = v;
		/* sometimes, eg. for disaster vehicles, when company bankrupts, when removing crashed/flooded vehicles,
		 * it may happen that vehicle chain is deleted when visible */
		if (!(v->vehstatus & VS_HIDDEN)) MarkSingleVehicleDirty(v);
		v = v->Next();
		delete u;
	} while (v != NULL);
}

/** head of the linked list to tell what vehicles that visited a depot in a tick */
static Vehicle* _first_veh_in_depot_list;

/** Adds a vehicle to the list of vehicles, that visited a depot this tick
 * @param *v vehicle to add
 */
void VehicleEnteredDepotThisTick(Vehicle *v)
{
	/* We need to set v->leave_depot_instantly as we have no control of it's contents at this time.
	 * Vehicle should stop in the depot if it was in 'stopping' state - train intered depot while slowing down. */
	if (((v->current_order.GetDepotActionType() & ODATFB_HALT) && !(v->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) && v->current_order.IsType(OT_GOTO_DEPOT)) ||
			(v->vehstatus & VS_STOPPED)) {
		/* we keep the vehicle in the depot since the user ordered it to stay */
		v->leave_depot_instantly = false;
	} else {
		/* the vehicle do not plan on stopping in the depot, so we stop it to ensure that it will not reserve the path
		 * out of the depot before we might autoreplace it to a different engine. The new engine would not own the reserved path
		 * we store that we stopped the vehicle, so autoreplace can start it again */
		v->vehstatus |= VS_STOPPED;
		v->leave_depot_instantly = true;
	}

	if (_first_veh_in_depot_list == NULL) {
		_first_veh_in_depot_list = v;
	} else {
		Vehicle *w = _first_veh_in_depot_list;
		while (w->depot_list != NULL) w = w->depot_list;
		w->depot_list = v;
	}
}

void CallVehicleTicks()
{
	_first_veh_in_depot_list = NULL; // now we are sure it's initialized at the start of each tick

	Station *st;
	FOR_ALL_STATIONS(st) LoadUnloadStation(st);

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		v->Tick();

		switch (v->type) {
			default: break;

			case VEH_TRAIN:
			case VEH_ROAD:
			case VEH_AIRCRAFT:
			case VEH_SHIP:
				if (v->type == VEH_TRAIN && IsTrainWagon(v)) continue;
				if (v->type == VEH_AIRCRAFT && v->subtype != AIR_HELICOPTER) continue;
				if (v->type == VEH_ROAD && !IsRoadVehFront(v)) continue;

				v->motion_counter += (v->direction & 1) ? (v->cur_speed * 3) / 4 : v->cur_speed;
				/* Play a running sound if the motion counter passes 256 (Do we not skip sounds?) */
				if (GB(v->motion_counter, 0, 8) < v->cur_speed) PlayVehicleSound(v, VSE_RUNNING);

				/* Play an alterate running sound every 16 ticks */
				if (GB(v->tick_counter, 0, 4) == 0) PlayVehicleSound(v, v->cur_speed > 0 ? VSE_RUNNING_16 : VSE_STOPPED_16);
		}
	}

	/* now we handle all the vehicles that entered a depot this tick */
	v = _first_veh_in_depot_list;
	if (v != NULL) {
		while (v != NULL) {
			/* Autoreplace needs the current player set as the vehicle owner */
			_current_player = v->owner;

			/* Buffer v->depot_list and clear it.
			 * Autoreplace might clear this so it has to be buffered. */
			Vehicle *w = v->depot_list;
			v->depot_list = NULL; // it should always be NULL at the end of each tick

			/* Start vehicle if we stopped them in VehicleEnteredDepotThisTick()
			 * We need to stop them between VehicleEnteredDepotThisTick() and here or we risk that
			 * they are already leaving the depot again before being replaced. */
			if (v->leave_depot_instantly) {
				v->leave_depot_instantly = false;
				v->vehstatus &= ~VS_STOPPED;
			}
			MaybeReplaceVehicle(v, DC_EXEC, true);
			v = w;
		}
		_current_player = OWNER_NONE;
	}
}

/** Check if a given engine type can be refitted to a given cargo
 * @param engine_type Engine type to check
 * @param cid_to check refit to this cargo-type
 * @return true if it is possible, false otherwise
 */
bool CanRefitTo(EngineID engine_type, CargoID cid_to)
{
	return HasBit(EngInfo(engine_type)->refit_mask, cid_to);
}

/** Find the first cargo type that an engine can be refitted to.
 * @param engine_type Which engine to find cargo for.
 * @return A climate dependent cargo type. CT_INVALID is returned if not refittable.
 */
CargoID FindFirstRefittableCargo(EngineID engine_type)
{
	uint32 refit_mask = EngInfo(engine_type)->refit_mask;

	if (refit_mask != 0) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			if (HasBit(refit_mask, cid)) return cid;
		}
	}

	return CT_INVALID;
}

/** Learn the price of refitting a certain engine
* @param engine_type Which engine to refit
* @return Price for refitting
*/
CommandCost GetRefitCost(EngineID engine_type)
{
	Money base_cost;
	ExpensesType expense_type;
	switch (GetEngine(engine_type)->type) {
		case VEH_SHIP:
			base_cost = _price.ship_base;
			expense_type = EXPENSES_SHIP_RUN;
			break;

		case VEH_ROAD:
			base_cost = _price.roadveh_base;
			expense_type = EXPENSES_ROADVEH_RUN;
			break;

		case VEH_AIRCRAFT:
			base_cost = _price.aircraft_base;
			expense_type = EXPENSES_AIRCRAFT_RUN;
			break;

		case VEH_TRAIN:
			base_cost = 2 * ((RailVehInfo(engine_type)->railveh_type == RAILVEH_WAGON) ?
							 _price.build_railwagon : _price.build_railvehicle);
			expense_type = EXPENSES_TRAIN_RUN;
			break;

		default: NOT_REACHED();
	}
	return CommandCost(expense_type, (EngInfo(engine_type)->refit_cost * base_cost) >> 10);
}

static void DoDrawVehicle(const Vehicle *v)
{
	SpriteID image = v->cur_image;
	SpriteID pal = PAL_NONE;

	if (v->vehstatus & VS_DEFPAL) pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);

	AddSortableSpriteToDraw(image, pal, v->x_pos + v->x_offs, v->y_pos + v->y_offs,
		v->x_extent, v->y_extent, v->z_extent, v->z_pos, (v->vehstatus & VS_SHADOW) != 0);
}

void ViewportAddVehicles(DrawPixelInfo *dpi)
{
	/* The bounding rectangle */
	const int l = dpi->left;
	const int r = dpi->left + dpi->width;
	const int t = dpi->top;
	const int b = dpi->top + dpi->height;

	/* The hash area to scan */
	int xl, xu, yl, yu;

	if (dpi->width + 70 < (1 << (7 + 6))) {
		xl = GB(l - 70, 7, 6);
		xu = GB(r,      7, 6);
	} else {
		/* scan whole hash row */
		xl = 0;
		xu = 0x3F;
	}

	if (dpi->height + 70 < (1 << (6 + 6))) {
		yl = GB(t - 70, 6, 6) << 6;
		yu = GB(b,      6, 6) << 6;
	} else {
		/* scan whole column */
		yl = 0;
		yu = 0x3F << 6;
	}

	for (int y = yl;; y = (y + (1 << 6)) & (0x3F << 6)) {
		for (int x = xl;; x = (x + 1) & 0x3F) {
			const Vehicle *v = _vehicle_position_hash[x + y]; // already masked & 0xFFF

			while (v != NULL) {
				if (!(v->vehstatus & VS_HIDDEN) &&
						l <= v->right_coord &&
						t <= v->bottom_coord &&
						r >= v->left_coord &&
						b >= v->top_coord) {
					DoDrawVehicle(v);
				}
				v = v->next_hash;
			}

			if (x == xu) break;
		}

		if (y == yu) break;
	}
}

Vehicle *CheckClickOnVehicle(const ViewPort *vp, int x, int y)
{
	Vehicle *found = NULL, *v;
	uint dist, best_dist = (uint)-1;

	if ((uint)(x -= vp->left) >= (uint)vp->width || (uint)(y -= vp->top) >= (uint)vp->height) return NULL;

	x = ScaleByZoom(x, vp->zoom) + vp->virtual_left;
	y = ScaleByZoom(y, vp->zoom) + vp->virtual_top;

	FOR_ALL_VEHICLES(v) {
		if ((v->vehstatus & (VS_HIDDEN|VS_UNCLICKABLE)) == 0 &&
				x >= v->left_coord && x <= v->right_coord &&
				y >= v->top_coord && y <= v->bottom_coord) {

			dist = max(
				abs(((v->left_coord + v->right_coord) >> 1) - x),
				abs(((v->top_coord + v->bottom_coord) >> 1) - y)
			);

			if (dist < best_dist) {
				found = v;
				best_dist = dist;
			}
		}
	}

	return found;
}

void CheckVehicle32Day(Vehicle *v)
{
	if ((v->day_counter & 0x1F) != 0) return;

	uint16 callback = GetVehicleCallback(CBID_VEHICLE_32DAY_CALLBACK, 0, 0, v->engine_type, v);
	if (callback == CALLBACK_FAILED) return;
	if (HasBit(callback, 0)) TriggerVehicle(v, VEHICLE_TRIGGER_CALLBACK_32); // Trigger vehicle trigger 10
	if (HasBit(callback, 1)) v->colormap = PAL_NONE;                         // Update colormap via callback 2D
}

void DecreaseVehicleValue(Vehicle *v)
{
	v->value -= v->value >> 8;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
}

static const byte _breakdown_chance[64] = {
	  3,   3,   3,   3,   3,   3,   3,   3,
	  4,   4,   5,   5,   6,   6,   7,   7,
	  8,   8,   9,   9,  10,  10,  11,  11,
	 12,  13,  13,  13,  13,  14,  15,  16,
	 17,  19,  21,  25,  28,  31,  34,  37,
	 40,  44,  48,  52,  56,  60,  64,  68,
	 72,  80,  90, 100, 110, 120, 130, 140,
	150, 170, 190, 210, 230, 250, 250, 250,
};

void CheckVehicleBreakdown(Vehicle *v)
{
	int rel, rel_old;

	/* decrease reliability */
	v->reliability = rel = max((rel_old = v->reliability) - v->reliability_spd_dec, 0);
	if ((rel_old >> 8) != (rel >> 8)) InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (v->breakdown_ctr != 0 || v->vehstatus & VS_STOPPED ||
			_settings_game.difficulty.vehicle_breakdowns < 1 ||
			v->cur_speed < 5 || _game_mode == GM_MENU) {
		return;
	}

	uint32 r = Random();

	/* increase chance of failure */
	int chance = v->breakdown_chance + 1;
	if (Chance16I(1, 25, r)) chance += 25;
	v->breakdown_chance = min(255, chance);

	/* calculate reliability value to use in comparison */
	rel = v->reliability;
	if (v->type == VEH_SHIP) rel += 0x6666;

	/* reduced breakdowns? */
	if (_settings_game.difficulty.vehicle_breakdowns == 1) rel += 0x6666;

	/* check if to break down */
	if (_breakdown_chance[(uint)min(rel, 0xffff) >> 10] <= v->breakdown_chance) {
		v->breakdown_ctr    = GB(r, 16, 6) + 0x3F;
		v->breakdown_delay  = GB(r, 24, 7) + 0x80;
		v->breakdown_chance = 0;
	}
}

static const StringID _vehicle_type_names[4] = {
	STR_019F_TRAIN,
	STR_019C_ROAD_VEHICLE,
	STR_019E_SHIP,
	STR_019D_AIRCRAFT,
};

static void ShowVehicleGettingOld(Vehicle *v, StringID msg)
{
	if (v->owner != _local_player) return;

	/* Do not show getting-old message if autorenew is active (and it can replace the vehicle) */
	if (GetPlayer(v->owner)->engine_renew && GetEngine(v->engine_type)->player_avail != 0) return;

	SetDParam(0, _vehicle_type_names[v->type]);
	SetDParam(1, v->unitnumber);
	AddNewsItem(msg, NS_ADVICE, v->index, 0);
}

void AgeVehicle(Vehicle *v)
{
	if (v->age < 65535) v->age++;

	int age = v->age - v->max_age;
	if (age == 366*0 || age == 366*1 || age == 366*2 || age == 366*3 || age == 366*4) v->reliability_spd_dec <<= 1;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (age == -366) {
		ShowVehicleGettingOld(v, STR_01A0_IS_GETTING_OLD);
	} else if (age == 0) {
		ShowVehicleGettingOld(v, STR_01A1_IS_GETTING_VERY_OLD);
	} else if (age > 0 && (age % 366) == 0) {
		ShowVehicleGettingOld(v, STR_01A2_IS_GETTING_VERY_OLD_AND);
	}
}

/** Starts or stops a lot of vehicles
 * @param tile Tile of the depot where the vehicles are started/stopped (only used for depots)
 * @param flags type of operation
 * @param p1 Station/Order/Depot ID (only used for vehicle list windows)
 * @param p2 bitmask
 *   - bit 0-4 Vehicle type
 *   - bit 5 false = start vehicles, true = stop vehicles
 *   - bit 6 if set, then it's a vehicle list window, not a depot and Tile is ignored in this case
 *   - bit 8-11 Vehicle List Window type (ignored unless bit 1 is set)
 */
CommandCost CmdMassStartStopVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleList list;
	CommandCost return_value = CMD_ERROR;
	uint stop_command;
	VehicleType vehicle_type = (VehicleType)GB(p2, 0, 5);
	bool start_stop = HasBit(p2, 5);
	bool vehicle_list_window = HasBit(p2, 6);

	switch (vehicle_type) {
		case VEH_TRAIN:    stop_command = CMD_START_STOP_TRAIN;    break;
		case VEH_ROAD:     stop_command = CMD_START_STOP_ROADVEH;  break;
		case VEH_SHIP:     stop_command = CMD_START_STOP_SHIP;     break;
		case VEH_AIRCRAFT: stop_command = CMD_START_STOP_AIRCRAFT; break;
		default: return CMD_ERROR;
	}

	if (vehicle_list_window) {
		uint32 id = p1;
		uint16 window_type = p2 & VLW_MASK;

		GenerateVehicleSortList(&list, vehicle_type, _current_player, id, window_type);
	} else {
		/* Get the list of vehicles in the depot */
		BuildDepotVehicleList(vehicle_type, tile, &list, NULL);
	}

	for (uint i = 0; i < list.Length(); i++) {
		const Vehicle *v = list[i];

		if (!!(v->vehstatus & VS_STOPPED) != start_stop) continue;

		if (!vehicle_list_window) {
			if (vehicle_type == VEH_TRAIN) {
				if (CheckTrainInDepot(v, false) == -1) continue;
			} else {
				if (!(v->vehstatus & VS_HIDDEN)) continue;
			}
		}

		CommandCost ret = DoCommand(tile, v->index, 0, flags, stop_command);

		if (CmdSucceeded(ret)) {
			return_value = CommandCost();
			/* We know that the command is valid for at least one vehicle.
			 * If we haven't set DC_EXEC, then there is no point in continueing because it will be valid */
			if (!(flags & DC_EXEC)) break;
		}
	}

	return return_value;
}

/** Sells all vehicles in a depot
 * @param tile Tile of the depot where the depot is
 * @param flags type of operation
 * @param p1 Vehicle type
 * @param p2 unused
 */
CommandCost CmdDepotSellAllVehicles(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleList list;

	CommandCost cost(EXPENSES_NEW_VEHICLES);
	uint sell_command;
	VehicleType vehicle_type = (VehicleType)GB(p1, 0, 8);

	switch (vehicle_type) {
		case VEH_TRAIN:    sell_command = CMD_SELL_RAIL_WAGON; break;
		case VEH_ROAD:     sell_command = CMD_SELL_ROAD_VEH;   break;
		case VEH_SHIP:     sell_command = CMD_SELL_SHIP;       break;
		case VEH_AIRCRAFT: sell_command = CMD_SELL_AIRCRAFT;   break;
		default: return CMD_ERROR;
	}

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &list, &list);

	for (uint i = 0; i < list.Length(); i++) {
		CommandCost ret = DoCommand(tile, list[i]->index, 1, flags, sell_command);
		if (CmdSucceeded(ret)) cost.AddCost(ret);
	}

	if (cost.GetCost() == 0) return CMD_ERROR; // no vehicles to sell
	return cost;
}

/** Autoreplace all vehicles in the depot
 * Note: this command can make incorrect cost estimations
 * Luckily the final price can only drop, not increase. This is due to the fact that
 * estimation can't predict wagon removal so it presumes worst case which is no income from selling wagons.
 * @param tile Tile of the depot where the vehicles are
 * @param flags type of operation
 * @param p1 Type of vehicle
 * @param p2 If bit 0 is set, then either replace all or nothing (instead of replacing until money runs out)
 */
CommandCost CmdDepotMassAutoReplace(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleList list;
	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES);
	VehicleType vehicle_type = (VehicleType)GB(p1, 0, 8);
	bool all_or_nothing = HasBit(p2, 0);

	if (!IsDepotTile(tile) || !IsTileOwner(tile, _current_player)) return CMD_ERROR;

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &list, &list);

	for (uint i = 0; i < list.Length(); i++) {
		Vehicle *v = (Vehicle*)list[i];

		/* Ensure that the vehicle completely in the depot */
		if (!v->IsInDepot()) continue;

		CommandCost ret = MaybeReplaceVehicle(v, flags, false);

		if (CmdSucceeded(ret)) {
			cost.AddCost(ret);
		} else {
			if (all_or_nothing) {
				/* We failed to replace a vehicle even though we set all or nothing.
				 * We should never reach this if DC_EXEC is set since then it should
				 * have failed the estimation guess. */
				assert(!(flags & DC_EXEC));
				/* Now we will have to return an error. */
				return CMD_ERROR;
			}
		}
	}

	if (cost.GetCost() == 0) {
		/* Either we didn't replace anything or something went wrong.
		 * Either way we want to return an error and not execute this command. */
		cost = CMD_ERROR;
	}

	return cost;
}

/** Clone a vehicle. If it is a train, it will clone all the cars too
 * @param tile tile of the depot where the cloned vehicle is build
 * @param flags type of operation
 * @param p1 the original vehicle's index
 * @param p2 1 = shared orders, else copied orders
 */
CommandCost CmdCloneVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost total_cost(EXPENSES_NEW_VEHICLES);
	uint32 build_argument = 2;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);
	Vehicle *v_front = v;
	Vehicle *w = NULL;
	Vehicle *w_front = NULL;
	Vehicle *w_rear = NULL;

	/*
	 * v_front is the front engine in the original vehicle
	 * v is the car/vehicle of the original vehicle, that is currently being copied
	 * w_front is the front engine of the cloned vehicle
	 * w is the car/vehicle currently being cloned
	 * w_rear is the rear end of the cloned train. It's used to add more cars and is only used by trains
	 */

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->type == VEH_TRAIN && (!IsFrontEngine(v) || v->u.rail.crash_anim_pos >= 4400)) return CMD_ERROR;

	/* check that we can allocate enough vehicles */
	if (!(flags & DC_EXEC)) {
		int veh_counter = 0;
		do {
			veh_counter++;
		} while ((v = v->Next()) != NULL);

		if (!Vehicle::AllocateList(NULL, veh_counter)) {
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
		}
	}

	v = v_front;

	do {
		if (v->type == VEH_TRAIN && IsRearDualheaded(v)) {
			/* we build the rear ends of multiheaded trains with the front ones */
			continue;
		}

		CommandCost cost = DoCommand(tile, v->engine_type, build_argument, flags, GetCmdBuildVeh(v));
		build_argument = 3; // ensure that we only assign a number to the first engine

		if (CmdFailed(cost)) return cost;

		total_cost.AddCost(cost);

		if (flags & DC_EXEC) {
			w = GetVehicle(_new_vehicle_id);

			if (v->type == VEH_TRAIN && HasBit(v->u.rail.flags, VRF_REVERSE_DIRECTION)) {
				SetBit(w->u.rail.flags, VRF_REVERSE_DIRECTION);
			}

			if (v->type == VEH_TRAIN && !IsFrontEngine(v)) {
				/* this s a train car
				 * add this unit to the end of the train */
				CommandCost result = DoCommand(0, (w_rear->index << 16) | w->index, 1, flags, CMD_MOVE_RAIL_VEHICLE);
				if (CmdFailed(result)) {
					/* The train can't be joined to make the same consist as the original.
					 * Sell what we already made (clean up) and return an error.           */
					DoCommand(w_front->tile, w_front->index, 1, flags, GetCmdSellVeh(w_front));
					DoCommand(w_front->tile, w->index,       1, flags, GetCmdSellVeh(w));
					return result; // return error and the message returned from CMD_MOVE_RAIL_VEHICLE
				}
			} else {
				/* this is a front engine or not a train. */
				w_front = w;
				w->service_interval = v->service_interval;
			}
			w_rear = w; // trains needs to know the last car in the train, so they can add more in next loop
		}
	} while (v->type == VEH_TRAIN && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC && v_front->type == VEH_TRAIN) {
		/* for trains this needs to be the front engine due to the callback function */
		_new_vehicle_id = w_front->index;
	}

	if (flags & DC_EXEC) {
		/* Cloned vehicles belong to the same group */
		DoCommand(0, v_front->group_id, w_front->index, flags, CMD_ADD_VEHICLE_GROUP);
	}


	/* Take care of refitting. */
	w = w_front;
	v = v_front;

	/* Both building and refitting are influenced by newgrf callbacks, which
	 * makes it impossible to accurately estimate the cloning costs. In
	 * particular, it is possible for engines of the same type to be built with
	 * different numbers of articulated parts, so when refitting we have to
	 * loop over real vehicles first, and then the articulated parts of those
	 * vehicles in a different loop. */
	do {
		do {
			if (flags & DC_EXEC) {
				assert(w != NULL);

				if (w->cargo_type != v->cargo_type || w->cargo_subtype != v->cargo_subtype) {
					CommandCost cost = DoCommand(0, w->index, v->cargo_type | (v->cargo_subtype << 8) | 1U << 16 , flags, GetCmdRefitVeh(v));
					if (CmdSucceeded(cost)) total_cost.AddCost(cost);
				}

				if (w->type == VEH_TRAIN && EngineHasArticPart(w)) {
					w = GetNextArticPart(w);
				} else if (w->type == VEH_ROAD && RoadVehHasArticPart(w)) {
					w = w->Next();
				} else {
					break;
				}
			} else {
				CargoID initial_cargo = GetEngineCargoType(v->engine_type);

				if (v->cargo_type != initial_cargo && initial_cargo != CT_INVALID) {
					total_cost.AddCost(GetRefitCost(v->engine_type));
				}
			}

			if (v->type == VEH_TRAIN && EngineHasArticPart(v)) {
				v = GetNextArticPart(v);
			} else if (v->type == VEH_ROAD && RoadVehHasArticPart(v)) {
				v = v->Next();
			} else {
				break;
			}
		} while (v != NULL);

		if ((flags & DC_EXEC) && v->type == VEH_TRAIN) w = GetNextVehicle(w);
	} while (v->type == VEH_TRAIN && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC) {
		/*
		 * Set the orders of the vehicle. Cannot do it earlier as we need
		 * the vehicle refitted before doing this, otherwise the moved
		 * cargo types might not match (passenger vs non-passenger)
		 */
		DoCommand(0, (v_front->index << 16) | w_front->index, p2 & 1 ? CO_SHARE : CO_COPY, flags, CMD_CLONE_ORDER);
	}

	/* Since we can't estimate the cost of cloning a vehicle accurately we must
	 * check whether the player has enough money manually. */
	if (!CheckPlayerHasMoney(total_cost)) {
		if (flags & DC_EXEC) {
			/* The vehicle has already been bought, so now it must be sold again. */
			DoCommand(w_front->tile, w_front->index, 1, flags, GetCmdSellVeh(w_front));
		}
		return CMD_ERROR;
	}

	return total_cost;
}

/**
 * Send all vehicles of type to depots
 * @param type type of vehicle
 * @param flags the flags used for DoCommand()
 * @param service should the vehicles only get service in the depots
 * @param owner PlayerID of owner of the vehicles to send
 * @param vlw_flag tells what kind of list requested the goto depot
 * @return 0 for success and CMD_ERROR if no vehicle is able to go to depot
 */
CommandCost SendAllVehiclesToDepot(VehicleType type, uint32 flags, bool service, PlayerID owner, uint16 vlw_flag, uint32 id)
{
	VehicleList list;

	GenerateVehicleSortList(&list, type, owner, id, vlw_flag);

	/* Send all the vehicles to a depot */
	for (uint i = 0; i < list.Length(); i++) {
		const Vehicle *v = list[i];
		CommandCost ret = DoCommand(v->tile, v->index, (service ? 1 : 0) | DEPOT_DONT_CANCEL, flags, GetCmdSendToDepot(type));

		/* Return 0 if DC_EXEC is not set this is a valid goto depot command)
			* In this case we know that at least one vehicle can be sent to a depot
			* and we will issue the command. We can now safely quit the loop, knowing
			* it will succeed at least once. With DC_EXEC we really need to send them to the depot */
		if (CmdSucceeded(ret) && !(flags & DC_EXEC)) {
			return CommandCost();
		}
	}

	return (flags & DC_EXEC) ? CommandCost() : CMD_ERROR;
}

/**
 * Calculates how full a vehicle is.
 * @param v The Vehicle to check. For trains, use the first engine.
 * @param color The string to show depending on if we are unloading or loading
 * @return A percentage of how full the Vehicle is.
 */
uint8 CalcPercentVehicleFilled(const Vehicle *v, StringID *color)
{
	int count = 0;
	int max = 0;
	int cars = 0;
	int unloading = 0;
	bool loading = false;

	const Vehicle *u = v;
	const Station *st = v->last_station_visited != INVALID_STATION ? GetStation(v->last_station_visited) : NULL;

	/* Count up max and used */
	for (; v != NULL; v = v->Next()) {
		count += v->cargo.Count();
		max += v->cargo_cap;
		if (v->cargo_cap != 0 && color != NULL) {
			unloading += HasBit(v->vehicle_flags, VF_CARGO_UNLOADING) ? 1 : 0;
			loading |= !(u->current_order.GetUnloadType() & OUFB_UNLOAD) && st->goods[v->cargo_type].days_since_pickup != 255;
			cars++;
		}
	}

	if (color != NULL) {
		if (unloading == 0 && loading) {
			*color = STR_PERCENT_UP;
		} else if (cars == unloading || !loading) {
			*color = STR_PERCENT_DOWN;
		} else {
			*color = STR_PERCENT_UP_DOWN;
		}
	}

	/* Train without capacity */
	if (max == 0) return 100;

	/* Return the percentage */
	return (count * 100) / max;
}

void VehicleEnterDepot(Vehicle *v)
{
	switch (v->type) {
		case VEH_TRAIN:
			InvalidateWindowClasses(WC_TRAINS_LIST);
			if (!IsFrontEngine(v)) v = v->First();
			UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
			v->load_unload_time_rem = 0;
			ClrBit(v->u.rail.flags, VRF_TOGGLE_REVERSE);
			TrainConsistChanged(v, true);
			break;

		case VEH_ROAD:
			InvalidateWindowClasses(WC_ROADVEH_LIST);
			if (!IsRoadVehFront(v)) v = v->First();
			break;

		case VEH_SHIP:
			InvalidateWindowClasses(WC_SHIPS_LIST);
			v->u.ship.state = TRACK_BIT_DEPOT;
			RecalcShipStuff(v);
			break;

		case VEH_AIRCRAFT:
			InvalidateWindowClasses(WC_AIRCRAFT_LIST);
			HandleAircraftEnterHangar(v);
			break;
		default: NOT_REACHED();
	}

	if (v->type != VEH_TRAIN) {
		/* Trains update the vehicle list when the first unit enters the depot and calls VehicleEnterDepot() when the last unit enters.
		 * We only increase the number of vehicles when the first one enters, so we will not need to search for more vehicles in the depot */
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}
	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

	v->vehstatus |= VS_HIDDEN;
	v->cur_speed = 0;

	VehicleServiceInDepot(v);

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.IsType(OT_GOTO_DEPOT)) {
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		Order t = v->current_order;
		v->current_order.MakeDummy();

		if (t.IsRefit()) {
			_current_player = v->owner;
			CommandCost cost = DoCommand(v->tile, v->index, t.GetRefitCargo() | t.GetRefitSubtype() << 8, DC_EXEC, GetCmdRefitVeh(v));

			if (CmdFailed(cost)) {
				v->leave_depot_instantly = false; // We ensure that the vehicle stays in the depot
				if (v->owner == _local_player) {
					/* Notify the user that we stopped the vehicle */
					SetDParam(0, _vehicle_type_names[v->type]);
					SetDParam(1, v->unitnumber);
					AddNewsItem(STR_ORDER_REFIT_FAILED, NS_ADVICE, v->index, 0);
				}
			} else if (v->owner == _local_player && cost.GetCost() != 0) {
				ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost.GetCost());
			}
		}

		if (t.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) {
			/* Part of orders */
			UpdateVehicleTimetable(v, true);
			v->cur_order_index++;
		} else if (t.GetDepotActionType() & ODATFB_HALT) {
			/* Force depot visit */
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				StringID string;

				switch (v->type) {
					case VEH_TRAIN:    string = STR_8814_TRAIN_IS_WAITING_IN_DEPOT; break;
					case VEH_ROAD:     string = STR_9016_ROAD_VEHICLE_IS_WAITING;   break;
					case VEH_SHIP:     string = STR_981C_SHIP_IS_WAITING_IN_DEPOT;  break;
					case VEH_AIRCRAFT: string = STR_A014_AIRCRAFT_IS_WAITING_IN;    break;
					default: NOT_REACHED(); string = STR_EMPTY; // Set the string to something to avoid a compiler warning
				}

				SetDParam(0, v->unitnumber);
				AddNewsItem(string, NS_ADVICE, v->index, 0);
			}
		}
	}
}

static bool IsUniqueVehicleName(const char *name)
{
	const Vehicle *v;
	char buf[512];

	FOR_ALL_VEHICLES(v) {
		switch (v->type) {
			case VEH_TRAIN:
				if (!IsTrainEngine(v)) continue;
				break;

			case VEH_ROAD:
				if (!IsRoadVehFront(v)) continue;
				break;

			case VEH_AIRCRAFT:
				if (!IsNormalAircraft(v)) continue;
				break;

			case VEH_SHIP:
				break;

			default:
				continue;
		}

		SetDParam(0, v->index);
		GetString(buf, STR_VEHICLE_NAME, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
	}

	return true;
}

/** Give a custom name to your vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to name
 * @param p2 unused
 */
CommandCost CmdNameVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidVehicleID(p1) || StrEmpty(_cmd_text)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (!IsUniqueVehicleName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

	if (flags & DC_EXEC) {
		free(v->name);
		v->name = strdup(_cmd_text);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 1);
		MarkWholeScreenDirty();
	}

	return CommandCost();
}


/** Change the service interval of a vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID that is being service-interval-changed
 * @param p2 new service interval
 */
CommandCost CmdChangeServiceInt(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	uint16 serv_int = GetServiceIntervalClamped(p2); /* Double check the service interval from the user-input */

	if (serv_int != p2 || !IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = serv_int;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}

	return CommandCost();
}


static Rect _old_vehicle_coords; ///< coords of vehicle before it has moved

/**
 * Stores the vehicle image coords for later call to EndVehicleMove()
 * @param v vehicle which image's coords to store
 * @see _old_vehicle_coords
 * @see EndVehicleMove()
 */
void BeginVehicleMove(const Vehicle *v)
{
	_old_vehicle_coords.left   = v->left_coord;
	_old_vehicle_coords.top    = v->top_coord;
	_old_vehicle_coords.right  = v->right_coord;
	_old_vehicle_coords.bottom = v->bottom_coord;
}

/**
 * Marks screen dirty after a vehicle has moved
 * @param v vehicle which is marked dirty
 * @see _old_vehicle_coords
 * @see BeginVehicleMove()
 */
void EndVehicleMove(const Vehicle *v)
{
	MarkAllViewportsDirty(
		min(_old_vehicle_coords.left,   v->left_coord),
		min(_old_vehicle_coords.top,    v->top_coord),
		max(_old_vehicle_coords.right,  v->right_coord) + 1,
		max(_old_vehicle_coords.bottom, v->bottom_coord) + 1
	);
}

/**
 * Marks viewports dirty where the vehicle's image is
 * In fact, it equals
 *   BeginVehicleMove(v); EndVehicleMove(v);
 * @param v vehicle to mark dirty
 * @see BeginVehicleMove()
 * @see EndVehicleMove()
 */
void MarkSingleVehicleDirty(const Vehicle *v)
{
	MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
}

/* returns true if staying in the same tile */
GetNewVehiclePosResult GetNewVehiclePos(const Vehicle *v)
{
	static const int8 _delta_coord[16] = {
		-1,-1,-1, 0, 1, 1, 1, 0, /* x */
		-1, 0, 1, 1, 1, 0,-1,-1, /* y */
	};

	int x = v->x_pos + _delta_coord[v->direction];
	int y = v->y_pos + _delta_coord[v->direction + 8];

	GetNewVehiclePosResult gp;
	gp.x = x;
	gp.y = y;
	gp.old_tile = v->tile;
	gp.new_tile = TileVirtXY(x, y);
	return gp;
}

static const Direction _new_direction_table[] = {
	DIR_N , DIR_NW, DIR_W ,
	DIR_NE, DIR_SE, DIR_SW,
	DIR_E , DIR_SE, DIR_S
};

Direction GetDirectionTowards(const Vehicle *v, int x, int y)
{
	int i = 0;

	if (y >= v->y_pos) {
		if (y != v->y_pos) i += 3;
		i += 3;
	}

	if (x >= v->x_pos) {
		if (x != v->x_pos) i++;
		i++;
	}

	Direction dir = v->direction;

	DirDiff dirdiff = DirDifference(_new_direction_table[i], dir);
	if (dirdiff == DIRDIFF_SAME) return dir;
	return ChangeDir(dir, dirdiff > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
}

Trackdir GetVehicleTrackdir(const Vehicle *v)
{
	if (v->vehstatus & VS_CRASHED) return INVALID_TRACKDIR;

	switch (v->type) {
		case VEH_TRAIN:
			if (v->u.rail.track == TRACK_BIT_DEPOT) // We'll assume the train is facing outwards
				return DiagDirToDiagTrackdir(GetRailDepotDirection(v->tile)); // Train in depot

			if (v->u.rail.track == TRACK_BIT_WORMHOLE) // train in tunnel or on bridge, so just use his direction and assume a diagonal track
				return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

			return TrackDirectionToTrackdir(FindFirstTrack(v->u.rail.track), v->direction);

		case VEH_SHIP:
			if (v->IsInDepot())
				// We'll assume the ship is facing outwards
				return DiagDirToDiagTrackdir(GetShipDepotDirection(v->tile));

			if (v->u.ship.state == TRACK_BIT_WORMHOLE) // ship on aqueduct, so just use his direction and assume a diagonal track
				return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

			return TrackDirectionToTrackdir(FindFirstTrack(v->u.ship.state), v->direction);

		case VEH_ROAD:
			if (v->IsInDepot()) // We'll assume the road vehicle is facing outwards
				return DiagDirToDiagTrackdir(GetRoadDepotDirection(v->tile));

			if (IsStandardRoadStopTile(v->tile)) // We'll assume the road vehicle is facing outwards
				return DiagDirToDiagTrackdir(GetRoadStopDir(v->tile)); // Road vehicle in a station

			if (IsDriveThroughStopTile(v->tile)) return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

			/* If vehicle's state is a valid track direction (vehicle is not turning around) return it */
			if (!IsReversingRoadTrackdir((Trackdir)v->u.road.state)) return (Trackdir)v->u.road.state;

			/* Vehicle is turning around, get the direction from vehicle's direction */
			return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

		/* case VEH_AIRCRAFT: case VEH_EFFECT: case VEH_DISASTER: */
		default: return INVALID_TRACKDIR;
	}
}

/**
 * Returns some meta-data over the to be entered tile.
 * @see VehicleEnterTileStatus to see what the bits in the return value mean.
 */
uint32 VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y)
{
	return _tile_type_procs[GetTileType(tile)]->vehicle_enter_tile_proc(v, tile, x, y);
}

UnitID GetFreeUnitNumber(VehicleType type)
{
	UnitID max = 0;
	const Vehicle *u;
	static bool *cache = NULL;
	static UnitID gmax = 0;

	switch (type) {
		case VEH_TRAIN:    max = _settings_game.vehicle.max_trains; break;
		case VEH_ROAD:     max = _settings_game.vehicle.max_roadveh; break;
		case VEH_SHIP:     max = _settings_game.vehicle.max_ships; break;
		case VEH_AIRCRAFT: max = _settings_game.vehicle.max_aircraft; break;
		default: NOT_REACHED();
	}

	if (max == 0) {
		/* we can't build any of this kind of vehicle, so we just return 1 instead of looking for a free number
		 * a max of 0 will cause the following code to write to a NULL pointer
		 * We know that 1 is bigger than the max allowed vehicle number, so it's the same as returning something, that is too big
		 */
		return 1;
	}

	if (max > gmax) {
		gmax = max;
		free(cache);
		cache = MallocT<bool>(max + 1);
	}

	/* Clear the cache */
	memset(cache, 0, (max + 1) * sizeof(*cache));

	/* Fill the cache */
	FOR_ALL_VEHICLES(u) {
		if (u->type == type && u->owner == _current_player && u->unitnumber != 0 && u->unitnumber <= max)
			cache[u->unitnumber] = true;
	}

	/* Find the first unused unit number */
	UnitID unit = 1;
	for (; unit <= max; unit++) {
		if (!cache[unit]) break;
	}

	return unit;
}


/**
 * Check whether we can build infrastructure for the given
 * vehicle type. This to disable building stations etc. when
 * you are not allowed/able to have the vehicle type yet.
 * @param type the vehicle type to check this for
 * @return true if there is any reason why you may build
 *         the infrastructure for the given vehicle type
 */
bool CanBuildVehicleInfrastructure(VehicleType type)
{
	assert(IsPlayerBuildableVehicleType(type));

	if (!IsValidPlayerID(_current_player)) return false;
	if (_settings_client.gui.always_build_infrastructure) return true;

	UnitID max;
	switch (type) {
		case VEH_TRAIN:    max = _settings_game.vehicle.max_trains; break;
		case VEH_ROAD:     max = _settings_game.vehicle.max_roadveh; break;
		case VEH_SHIP:     max = _settings_game.vehicle.max_ships; break;
		case VEH_AIRCRAFT: max = _settings_game.vehicle.max_aircraft; break;
		default: NOT_REACHED();
	}

	/* We can build vehicle infrastructure when we may build the vehicle type */
	if (max > 0) {
		/* Can we actually build the vehicle type? */
		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, type) {
			if (HasBit(e->player_avail, _local_player)) return true;
		}
		return false;
	}

	/* We should be able to build infrastructure when we have the actual vehicle type */
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _local_player && v->type == type) return true;
	}

	return false;
}


const Livery *GetEngineLivery(EngineID engine_type, PlayerID player, EngineID parent_engine_type, const Vehicle *v)
{
	const Player *p = GetPlayer(player);
	LiveryScheme scheme = LS_DEFAULT;
	CargoID cargo_type = v == NULL ? (CargoID)CT_INVALID : v->cargo_type;

	/* The default livery is always available for use, but its in_use flag determines
	 * whether any _other_ liveries are in use. */
	if (p->livery[LS_DEFAULT].in_use && (_settings_client.gui.liveries == 2 || (_settings_client.gui.liveries == 1 && player == _local_player))) {
		/* Determine the livery scheme to use */
		switch (GetEngine(engine_type)->type) {
			default: NOT_REACHED();
			case VEH_TRAIN: {
				const RailVehicleInfo *rvi = RailVehInfo(engine_type);

				if (cargo_type == CT_INVALID) cargo_type = rvi->cargo_type;
				if (rvi->railveh_type == RAILVEH_WAGON) {
					if (!GetCargo(cargo_type)->is_freight) {
						if (parent_engine_type == INVALID_ENGINE) {
							scheme = LS_PASSENGER_WAGON_STEAM;
						} else {
							switch (RailVehInfo(parent_engine_type)->engclass) {
								default: NOT_REACHED();
								case EC_STEAM:    scheme = LS_PASSENGER_WAGON_STEAM;    break;
								case EC_DIESEL:   scheme = LS_PASSENGER_WAGON_DIESEL;   break;
								case EC_ELECTRIC: scheme = LS_PASSENGER_WAGON_ELECTRIC; break;
								case EC_MONORAIL: scheme = LS_PASSENGER_WAGON_MONORAIL; break;
								case EC_MAGLEV:   scheme = LS_PASSENGER_WAGON_MAGLEV;   break;
							}
						}
					} else {
						scheme = LS_FREIGHT_WAGON;
					}
				} else {
					bool is_mu = HasBit(EngInfo(engine_type)->misc_flags, EF_RAIL_IS_MU);

					switch (rvi->engclass) {
						default: NOT_REACHED();
						case EC_STEAM:    scheme = LS_STEAM; break;
						case EC_DIESEL:   scheme = is_mu ? LS_DMU : LS_DIESEL;   break;
						case EC_ELECTRIC: scheme = is_mu ? LS_EMU : LS_ELECTRIC; break;
						case EC_MONORAIL: scheme = LS_MONORAIL; break;
						case EC_MAGLEV:   scheme = LS_MAGLEV; break;
					}
				}
				break;
			}

			case VEH_ROAD: {
				const RoadVehicleInfo *rvi = RoadVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = rvi->cargo_type;
				if (HasBit(EngInfo(engine_type)->misc_flags, EF_ROAD_TRAM)) {
					/* Tram */
					scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_PASSENGER_TRAM : LS_FREIGHT_TRAM;
				} else {
					/* Bus or truck */
					scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_BUS : LS_TRUCK;
				}
				break;
			}

			case VEH_SHIP: {
				const ShipVehicleInfo *svi = ShipVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = svi->cargo_type;
				scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_PASSENGER_SHIP : LS_FREIGHT_SHIP;
				break;
			}

			case VEH_AIRCRAFT: {
				const AircraftVehicleInfo *avi = AircraftVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = CT_PASSENGERS;
				switch (avi->subtype) {
					case AIR_HELI: scheme = LS_HELICOPTER; break;
					case AIR_CTOL: scheme = LS_SMALL_PLANE; break;
					case AIR_CTOL | AIR_FAST: scheme = LS_LARGE_PLANE; break;
				}
				break;
			}
		}

		/* Switch back to the default scheme if the resolved scheme is not in use */
		if (!p->livery[scheme].in_use) scheme = LS_DEFAULT;
	}

	return &p->livery[scheme];
}


static SpriteID GetEngineColourMap(EngineID engine_type, PlayerID player, EngineID parent_engine_type, const Vehicle *v)
{
	SpriteID map = (v != NULL) ? v->colormap : PAL_NONE;

	/* Return cached value if any */
	if (map != PAL_NONE) return map;

	/* Check if we should use the colour map callback */
	if (HasBit(EngInfo(engine_type)->callbackmask, CBM_VEHICLE_COLOUR_REMAP)) {
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_COLOUR_MAPPING, 0, 0, engine_type, v);
		/* A return value of 0xC000 is stated to "use the default two-color
		 * maps" which happens to be the failure action too... */
		if (callback != CALLBACK_FAILED && callback != 0xC000) {
			map = GB(callback, 0, 14);
			/* If bit 14 is set, then the company colours are applied to the
			 * map else it's returned as-is. */
			if (!HasBit(callback, 14)) {
				/* Update cache */
				if (v != NULL) ((Vehicle*)v)->colormap = map;
				return map;
			}
		}
	}

	bool twocc = HasBit(EngInfo(engine_type)->misc_flags, EF_USES_2CC);

	if (map == PAL_NONE) map = twocc ? (SpriteID)SPR_2CCMAP_BASE : (SpriteID)PALETTE_RECOLOR_START;

	const Livery *livery = GetEngineLivery(engine_type, player, parent_engine_type, v);

	map += livery->colour1;
	if (twocc) map += livery->colour2 * 16;

	/* Update cache */
	if (v != NULL) ((Vehicle*)v)->colormap = map;
	return map;
}

SpriteID GetEnginePalette(EngineID engine_type, PlayerID player)
{
	return GetEngineColourMap(engine_type, player, INVALID_ENGINE, NULL);
}

SpriteID GetVehiclePalette(const Vehicle *v)
{
	if (v->type == VEH_TRAIN) {
		return GetEngineColourMap(
			(v->u.rail.first_engine != INVALID_ENGINE && (UsesWagonOverride(v) || (IsArticulatedPart(v) && RailVehInfo(v->engine_type)->railveh_type != RAILVEH_WAGON))) ?
				v->u.rail.first_engine : v->engine_type,
			v->owner, v->u.rail.first_engine, v);
	}

	return GetEngineColourMap(v->engine_type, v->owner, INVALID_ENGINE, v);
}

static uint8  _cargo_days;
static uint16 _cargo_source;
static uint32 _cargo_source_xy;
static uint16 _cargo_count;
static uint16 _cargo_paid_for;
static Money  _cargo_feeder_share;
static uint32 _cargo_loaded_at_xy;

/**
 * Make it possible to make the saveload tables "friends" of other classes.
 * @param vt the vehicle type. Can be VEH_END for the common vehicle description data
 * @return the saveload description
 */
const SaveLoad *GetVehicleDescription(VehicleType vt)
{
/** Save and load of vehicles */
static const SaveLoad _common_veh_desc[] = {
	    SLE_VAR(Vehicle, subtype,              SLE_UINT8),

	    SLE_REF(Vehicle, next,                 REF_VEHICLE_OLD),
	SLE_CONDVAR(Vehicle, name,                 SLE_NAME,                    0, 83),
	SLE_CONDSTR(Vehicle, name,                 SLE_STR, 0,                 84, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, unitnumber,           SLE_FILE_U8  | SLE_VAR_U16,  0, 7),
	SLE_CONDVAR(Vehicle, unitnumber,           SLE_UINT16,                  8, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, owner,                SLE_UINT8),
	SLE_CONDVAR(Vehicle, tile,                 SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, tile,                 SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, dest_tile,            SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, dest_tile,            SLE_UINT32,                  6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,                SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, x_pos,                SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,                SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, y_pos,                SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,                SLE_UINT8),
	    SLE_VAR(Vehicle, direction,            SLE_UINT8),

	SLE_CONDNULL(2,                                                         0, 57),
	    SLE_VAR(Vehicle, spritenum,            SLE_UINT8),
	SLE_CONDNULL(5,                                                         0, 57),
	    SLE_VAR(Vehicle, engine_type,          SLE_UINT16),

	    SLE_VAR(Vehicle, max_speed,            SLE_UINT16),
	    SLE_VAR(Vehicle, cur_speed,            SLE_UINT16),
	    SLE_VAR(Vehicle, subspeed,             SLE_UINT8),
	    SLE_VAR(Vehicle, acceleration,         SLE_UINT8),
	    SLE_VAR(Vehicle, progress,             SLE_UINT8),

	    SLE_VAR(Vehicle, vehstatus,            SLE_UINT8),
	SLE_CONDVAR(Vehicle, last_station_visited, SLE_FILE_U8  | SLE_VAR_U16,  0, 4),
	SLE_CONDVAR(Vehicle, last_station_visited, SLE_UINT16,                  5, SL_MAX_VERSION),

	     SLE_VAR(Vehicle, cargo_type,           SLE_UINT8),
	 SLE_CONDVAR(Vehicle, cargo_subtype,        SLE_UINT8,                  35, SL_MAX_VERSION),
	SLEG_CONDVAR(         _cargo_days,          SLE_UINT8,                   0, 67),
	SLEG_CONDVAR(         _cargo_source,        SLE_FILE_U8  | SLE_VAR_U16,  0, 6),
	SLEG_CONDVAR(         _cargo_source,        SLE_UINT16,                  7, 67),
	SLEG_CONDVAR(         _cargo_source_xy,     SLE_UINT32,                 44, 67),
	     SLE_VAR(Vehicle, cargo_cap,            SLE_UINT16),
	SLEG_CONDVAR(         _cargo_count,         SLE_UINT16,                  0, 67),
	 SLE_CONDLST(Vehicle, cargo,                REF_CARGO_PACKET,           68, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, day_counter,          SLE_UINT8),
	    SLE_VAR(Vehicle, tick_counter,         SLE_UINT8),
	SLE_CONDVAR(Vehicle, running_ticks,        SLE_UINT8,                   88, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, cur_order_index,      SLE_UINT8),
	    SLE_VAR(Vehicle, num_orders,           SLE_UINT8),

	/* This next line is for version 4 and prior compatibility.. it temporarily reads
	    type and flags (which were both 4 bits) into type. Later on this is
	    converted correctly */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, type), SLE_UINT8,                 0, 4),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),

	/* Orders for version 5 and on */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, type),  SLE_UINT8,  5, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, flags), SLE_UINT8,  5, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest),  SLE_UINT16, 5, SL_MAX_VERSION),

	/* Refit in current order */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, refit_cargo),    SLE_UINT8, 36, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, refit_subtype),  SLE_UINT8, 36, SL_MAX_VERSION),

	/* Timetable in current order */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, wait_time),      SLE_UINT16, 67, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, travel_time),    SLE_UINT16, 67, SL_MAX_VERSION),

	    SLE_REF(Vehicle, orders,               REF_ORDER),

	SLE_CONDVAR(Vehicle, age,                  SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, age,                  SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, max_age,              SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, max_age,              SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, date_of_last_service, SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, date_of_last_service, SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, service_interval,     SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, service_interval,     SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, reliability,          SLE_UINT16),
	    SLE_VAR(Vehicle, reliability_spd_dec,  SLE_UINT16),
	    SLE_VAR(Vehicle, breakdown_ctr,        SLE_UINT8),
	    SLE_VAR(Vehicle, breakdown_delay,      SLE_UINT8),
	    SLE_VAR(Vehicle, breakdowns_since_last_service, SLE_UINT8),
	    SLE_VAR(Vehicle, breakdown_chance,     SLE_UINT8),
	SLE_CONDVAR(Vehicle, build_year,           SLE_FILE_U8 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, build_year,           SLE_INT32,                 31, SL_MAX_VERSION),

	     SLE_VAR(Vehicle, load_unload_time_rem, SLE_UINT16),
	SLEG_CONDVAR(         _cargo_paid_for,      SLE_UINT16,                45, SL_MAX_VERSION),
	 SLE_CONDVAR(Vehicle, vehicle_flags,        SLE_UINT8,                 40, SL_MAX_VERSION),

	 SLE_CONDVAR(Vehicle, profit_this_year,     SLE_FILE_I32 | SLE_VAR_I64, 0, 64),
	 SLE_CONDVAR(Vehicle, profit_this_year,     SLE_INT64,                 65, SL_MAX_VERSION),
	 SLE_CONDVAR(Vehicle, profit_last_year,     SLE_FILE_I32 | SLE_VAR_I64, 0, 64),
	 SLE_CONDVAR(Vehicle, profit_last_year,     SLE_INT64,                 65, SL_MAX_VERSION),
	SLEG_CONDVAR(         _cargo_feeder_share,  SLE_FILE_I32 | SLE_VAR_I64,51, 64),
	SLEG_CONDVAR(         _cargo_feeder_share,  SLE_INT64,                 65, 67),
	SLEG_CONDVAR(         _cargo_loaded_at_xy,  SLE_UINT32,                51, 67),
	 SLE_CONDVAR(Vehicle, value,                SLE_FILE_I32 | SLE_VAR_I64, 0, 64),
	 SLE_CONDVAR(Vehicle, value,                SLE_INT64,                 65, SL_MAX_VERSION),

	 SLE_CONDVAR(Vehicle, random_bits,          SLE_UINT8,                 2, SL_MAX_VERSION),
	 SLE_CONDVAR(Vehicle, waiting_triggers,     SLE_UINT8,                 2, SL_MAX_VERSION),

	 SLE_CONDREF(Vehicle, next_shared,        REF_VEHICLE,                 2, SL_MAX_VERSION),
	 SLE_CONDREF(Vehicle, prev_shared,        REF_VEHICLE,                 2, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, group_id,             SLE_UINT16,                60, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, current_order_time,   SLE_UINT32,                67, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, lateness_counter,     SLE_INT32,                 67, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 10 bytes) */
	SLE_CONDNULL(10,                                                       2, SL_MAX_VERSION),

	SLE_END()
};


static const SaveLoad _train_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_TRAIN),
	SLE_VEH_INCLUDEX(),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, crash_anim_pos),         SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, force_proceed),          SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, railtype),               SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, track),                  SLE_UINT8),

	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, flags),                  SLE_UINT8,  2, SL_MAX_VERSION),
	SLE_CONDNULL(2, 2, 59),

	SLE_CONDNULL(2, 2, 19),
	/* reserve extra space in savegame here. (currently 11 bytes) */
	SLE_CONDNULL(11, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _roadveh_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_ROAD),
	SLE_VEH_INCLUDEX(),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, state),          SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, frame),          SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, blocked_ctr),    SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, overtaking),     SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, overtaking_ctr), SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, crashed_ctr),    SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, reverse_ctr),    SLE_UINT8),

	SLE_CONDREFX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, slot),     REF_ROADSTOPS, 6, SL_MAX_VERSION),
	SLE_CONDNULL(1,                                                                     6, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, slot_age), SLE_UINT8,     6, SL_MAX_VERSION),
	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16,                                                                    2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _ship_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_SHIP),
	SLE_VEH_INCLUDEX(),
	SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleShip, state), SLE_UINT8),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _aircraft_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_AIRCRAFT),
	SLE_VEH_INCLUDEX(),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, crashed_counter), SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, pos),             SLE_UINT8),

	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, targetairport),   SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, targetairport),   SLE_UINT16,                5, SL_MAX_VERSION),

	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, state),           SLE_UINT8),

	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, previous_pos),    SLE_UINT8,                 2, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 15 bytes) */
	SLE_CONDNULL(15,                                                                                      2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _special_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_EFFECT),

	    SLE_VAR(Vehicle, subtype,       SLE_UINT8),

	SLE_CONDVAR(Vehicle, tile,          SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle, tile,          SLE_UINT32,                 6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,         SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle, x_pos,         SLE_INT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_INT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,         SLE_UINT8),

	    SLE_VAR(Vehicle, cur_image,     SLE_UINT16),
	SLE_CONDNULL(5,                                                 0, 57),
	    SLE_VAR(Vehicle, progress,      SLE_UINT8),
	    SLE_VAR(Vehicle, vehstatus,     SLE_UINT8),

	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleEffect, animation_state),    SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleEffect, animation_substate), SLE_UINT8),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _disaster_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_DISASTER),

	    SLE_REF(Vehicle, next,          REF_VEHICLE_OLD),

	    SLE_VAR(Vehicle, subtype,       SLE_UINT8),
	SLE_CONDVAR(Vehicle, tile,          SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, tile,          SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, dest_tile,     SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, dest_tile,     SLE_UINT32,                  6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,         SLE_FILE_I16 | SLE_VAR_I32,  0, 5),
	SLE_CONDVAR(Vehicle, x_pos,         SLE_INT32,                   6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_FILE_I16 | SLE_VAR_I32,  0, 5),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_INT32,                   6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,         SLE_UINT8),
	    SLE_VAR(Vehicle, direction,     SLE_UINT8),

	SLE_CONDNULL(5,                                                  0, 57),
	    SLE_VAR(Vehicle, owner,         SLE_UINT8),
	    SLE_VAR(Vehicle, vehstatus,     SLE_UINT8),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest), SLE_UINT16,                5, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, cur_image,     SLE_UINT16),
	SLE_CONDVAR(Vehicle, age,           SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, age,           SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, tick_counter,  SLE_UINT8),

	   SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleDisaster, image_override),           SLE_UINT16),
	   SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleDisaster, big_ufo_destroyer_target), SLE_UINT16),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16,                                                 2, SL_MAX_VERSION),

	SLE_END()
};


static const SaveLoad *_veh_descs[] = {
	_train_desc,
	_roadveh_desc,
	_ship_desc,
	_aircraft_desc,
	_special_desc,
	_disaster_desc,
	_common_veh_desc,
};

	return _veh_descs[vt];
}

/** Will be called when the vehicles need to be saved. */
static void Save_VEHS()
{
	Vehicle *v;
	/* Write the vehicles */
	FOR_ALL_VEHICLES(v) {
		SlSetArrayIndex(v->index);
		SlObject(v, GetVehicleDescription(v->type));
	}
}

/** Will be called when vehicles need to be loaded. */
void Load_VEHS()
{
	int index;
	Vehicle *v;

	_cargo_count = 0;

	while ((index = SlIterateArray()) != -1) {
		Vehicle *v;
		VehicleType vtype = (VehicleType)SlReadByte();

		switch (vtype) {
			case VEH_TRAIN:    v = new (index) Train();           break;
			case VEH_ROAD:     v = new (index) RoadVehicle();     break;
			case VEH_SHIP:     v = new (index) Ship();            break;
			case VEH_AIRCRAFT: v = new (index) Aircraft();        break;
			case VEH_EFFECT:   v = new (index) EffectVehicle();   break;
			case VEH_DISASTER: v = new (index) DisasterVehicle(); break;
			case VEH_INVALID:  v = new (index) InvalidVehicle();  break;
			default: NOT_REACHED();
		}

		SlObject(v, GetVehicleDescription(vtype));

		if (_cargo_count != 0 && IsPlayerBuildableVehicleType(v)) {
			/* Don't construct the packet with station here, because that'll fail with old savegames */
			CargoPacket *cp = new CargoPacket();
			cp->source          = _cargo_source;
			cp->source_xy       = _cargo_source_xy;
			cp->count           = _cargo_count;
			cp->days_in_transit = _cargo_days;
			cp->feeder_share    = _cargo_feeder_share;
			cp->loaded_at_xy    = _cargo_loaded_at_xy;
			v->cargo.Append(cp);
		}

		/* Old savegames used 'last_station_visited = 0xFF' */
		if (CheckSavegameVersion(5) && v->last_station_visited == 0xFF)
			v->last_station_visited = INVALID_STATION;

		if (CheckSavegameVersion(5)) {
			/* Convert the current_order.type (which is a mix of type and flags, because
			 *  in those versions, they both were 4 bits big) to type and flags */
			v->current_order.flags = GB(v->current_order.type, 4, 4);
			v->current_order.type &= 0x0F;
		}

		/* Advanced vehicle lists got added */
		if (CheckSavegameVersion(60)) v->group_id = DEFAULT_GROUP;
	}

	/* Check for shared order-lists (we now use pointers for that) */
	if (CheckSavegameVersionOldStyle(5, 2)) {
		FOR_ALL_VEHICLES(v) {
			Vehicle *u;

			FOR_ALL_VEHICLES_FROM(u, v->index + 1) {
				/* If a vehicle has the same orders, add the link to eachother
				 *  in both vehicles */
				if (v->orders == u->orders) {
					v->next_shared = u;
					u->prev_shared = v;
					break;
				}
			}
		}
	}
}

extern const ChunkHandler _veh_chunk_handlers[] = {
	{ 'VEHS', Save_VEHS, Load_VEHS, CH_SPARSE_ARRAY | CH_LAST},
};

void Vehicle::BeginLoading()
{
	assert(IsTileType(tile, MP_STATION) || type == VEH_SHIP);

	if (this->current_order.IsType(OT_GOTO_STATION) &&
			this->current_order.GetDestination() == this->last_station_visited) {
		current_order.MakeLoading(true);
		UpdateVehicleTimetable(this, true);

		/* Furthermore add the Non Stop flag to mark that this station
		 * is the actual destination of the vehicle, which is (for example)
		 * necessary to be known for HandleTrainLoading to determine
		 * whether the train is lost or not; not marking a train lost
		 * that arrives at random stations is bad. */
		this->current_order.SetNonStopType(ONSF_NO_STOP_AT_ANY_STATION);

	} else {
		current_order.MakeLoading(false);
	}

	GetStation(this->last_station_visited)->loading_vehicles.push_back(this);

	VehiclePayment(this);

	InvalidateWindow(GetWindowClassForVehicleType(this->type), this->owner);
	InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
	InvalidateWindow(WC_VEHICLE_DETAILS, this->index);
	InvalidateWindow(WC_STATION_VIEW, this->last_station_visited);

	GetStation(this->last_station_visited)->MarkTilesDirty(true);
	this->MarkDirty();
}

void Vehicle::LeaveStation()
{
	assert(current_order.IsType(OT_LOADING));

	/* Only update the timetable if the vehicle was supposed to stop here. */
	if (current_order.GetNonStopType() != ONSF_STOP_EVERYWHERE) UpdateVehicleTimetable(this, false);

	current_order.MakeLeaveStation();
	Station *st = GetStation(this->last_station_visited);
	st->loading_vehicles.remove(this);

	HideFillingPercent(this->fill_percent_te_id);
	this->fill_percent_te_id = INVALID_TE_ID;

	/* Trigger station animation for trains only */
	if (this->type == VEH_TRAIN && IsTileType(this->tile, MP_STATION)) StationAnimationTrigger(st, this->tile, STAT_ANIM_TRAIN_DEPARTS);
}


void Vehicle::HandleLoading(bool mode)
{
	switch (this->current_order.GetType()) {
		case OT_LOADING: {
			uint wait_time = max(this->current_order.wait_time - this->lateness_counter, 0);

			/* Not the first call for this tick, or still loading */
			if (mode || !HasBit(this->vehicle_flags, VF_LOADING_FINISHED) ||
					(_settings_game.order.timetabling && this->current_order_time < wait_time)) return;

			this->PlayLeaveStationSound();

			bool at_destination_station = this->current_order.GetNonStopType() != ONSF_STOP_EVERYWHERE;
			this->LeaveStation();

			/* If this was not the final order, don't remove it from the list. */
			if (!at_destination_station) return;
			break;
		}

		case OT_DUMMY: break;

		default: return;
	}

	this->cur_order_index++;
	InvalidateVehicleOrder(this);
}

CommandCost Vehicle::SendToDepot(uint32 flags, DepotCommand command)
{
	if (!CheckOwnership(this->owner)) return CMD_ERROR;
	if (this->vehstatus & VS_CRASHED) return CMD_ERROR;
	if (this->IsInDepot()) return CMD_ERROR;

	if (this->current_order.IsType(OT_GOTO_DEPOT)) {
		bool halt_in_depot = this->current_order.GetDepotActionType() & ODATFB_HALT;
		if (!!(command & DEPOT_SERVICE) == halt_in_depot) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same depot.
			 * Note: the if is (true for requesting service == true for ordered to stop in depot)          */
			if (flags & DC_EXEC) {
				this->current_order.SetDepotOrderType(ODTF_MANUAL);
				this->current_order.SetDepotActionType(halt_in_depot ? ODATF_SERVICE_ONLY : ODATFB_HALT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
			}
			return CommandCost();
		}

		if (command & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of depot orders
		if (flags & DC_EXEC) {
			/* If the orders to 'goto depot' are in the orders list (forced servicing),
			 * then skip to the next order; effectively cancelling this forced service */
			if (this->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) this->cur_order_index++;

			this->current_order.MakeDummy();
			InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
		}
		return CommandCost();
	}

	/* check if at a standstill (not stopped only) in a depot
	 * the check is down here to make it possible to alter stop/service for trains entering the depot */
	if (this->type == VEH_TRAIN && IsRailDepotTile(this->tile) && this->cur_speed == 0) return CMD_ERROR;

	TileIndex location;
	DestinationID destination;
	bool reverse;
	static const StringID no_depot[] = {STR_883A_UNABLE_TO_FIND_ROUTE_TO, STR_9019_UNABLE_TO_FIND_LOCAL_DEPOT, STR_981A_UNABLE_TO_FIND_LOCAL_DEPOT, STR_A012_CAN_T_SEND_AIRCRAFT_TO};
	if (!this->FindClosestDepot(&location, &destination, &reverse)) return_cmd_error(no_depot[this->type]);

	if (flags & DC_EXEC) {
		if (this->current_order.IsType(OT_LOADING)) this->LeaveStation();

		this->dest_tile = location;
		this->current_order.MakeGoToDepot(destination, ODTF_MANUAL);
		if (!(command & DEPOT_SERVICE)) this->current_order.SetDepotActionType(ODATFB_HALT);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);

		/* If there is no depot in front, reverse automatically (trains only) */
		if (this->type == VEH_TRAIN && reverse) DoCommand(this->tile, this->index, 0, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);

		if (this->type == VEH_AIRCRAFT && this->u.air.state == FLYING && this->u.air.targetairport != destination) {
			/* The aircraft is now heading for a different hangar than the next in the orders */
			extern void AircraftNextAirportPos_and_Order(Vehicle *v);
			AircraftNextAirportPos_and_Order(this);
		}
	}

	return CommandCost();

}

void Vehicle::SetNext(Vehicle *next)
{
	if (this->next != NULL) {
		/* We had an old next vehicle. Update the first and previous pointers */
		for (Vehicle *v = this->next; v != NULL; v = v->Next()) {
			v->first = this->next;
		}
		this->next->previous = NULL;
	}

	this->next = next;

	if (this->next != NULL) {
		/* A new next vehicle. Update the first and previous pointers */
		if (this->next->previous != NULL) this->next->previous->next = NULL;
		this->next->previous = this;
		for (Vehicle *v = this->next; v != NULL; v = v->Next()) {
			v->first = this->first;
		}
	}
}

/** Backs up a chain of vehicles
 * @param v The vehicle to back up
 */
void BackuppedVehicle::BackupVehicle(Vehicle *v)
{
	int length = CountVehiclesInChain(v);

	size_t cargo_packages_count = 1;
	for (const Vehicle *v_count = v; v_count != NULL; v_count=v_count->Next()) {
		/* Now we count how many cargo packets we need to store.
		 * We started with an offset by one because we also need an end of array marker. */
		cargo_packages_count += v_count->cargo.packets.size();
	}

	vehicles = MallocT<Vehicle>(length);
	cargo_packets = MallocT<CargoPacket>(cargo_packages_count);

	/* Now we make some pointers to iterate over the arrays. */
	Vehicle *copy = vehicles;
	CargoPacket *cargo = cargo_packets;

	Vehicle *original = v;

	for (; 0 < length; original = original->Next(), copy++, length--) {
		/* First we need to copy the vehicle itself.
		 * However there is an issue as the cargo list isn't copied.
		 * To avoid restoring invalid pointers we start by swapping the cargo list with an empty one. */
		CargoList::List empty_packets;
		original->cargo.packets.swap(empty_packets);
		memcpy(copy, original, sizeof(Vehicle));

		/* No need to do anything else if the cargo list is empty.
		 * It really doesn't matter if we swap an empty list with an empty list. */
		if (original->cargo.Empty()) continue;

		/* And now we swap the cargo lists back. The vehicle now has it's cargo again. */
		original->cargo.packets.swap(empty_packets);

		/* The vehicle contains some cargo so we will back up the cargo as well.
		 * We only need to store the packets and not which vehicle they came from.
		 * We will still be able to put them together with the right vehicle when restoring. */
		const CargoList::List *packets = original->cargo.Packets();
		for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
			memcpy(cargo, (*it), sizeof(CargoPacket));
			cargo++;
		}
	}
	/* We should end with a 0 packet so restoring can detect the end of the array. */
	memset(cargo, 0, sizeof(CargoPacket));
}

/** Restore a backed up row of vehicles
 * @param *v The array of vehicles to restore
 * @param *p The owner of the vehicle
 */
Vehicle* BackuppedVehicle::RestoreBackupVehicle(Vehicle *v, Player *p)
{
	Vehicle *backup = v;
	CargoPacket *cargo = cargo_packets;

	assert(v->owner == p->index);

	/* Cache the result of the vehicle type check since it will not change
	 * and we need this check once for every run though the loop. */
	bool is_road_veh = v->type == VEH_ROAD;

	while (true) {
		Vehicle *dest = GetVehicle(backup->index);
		/* The vehicle should be free since we are restoring something we just sold. */
		assert(!dest->IsValid());
		memcpy(dest, backup, sizeof(Vehicle));

		/* We decreased the engine count when we sold the engines so we will increase it again. */
		if (IsEngineCountable(backup)) {
			p->num_engines[backup->engine_type]++;
			if (IsValidGroupID(backup->group_id)) GetGroup(backup->group_id)->num_engines[backup->engine_type]++;
			if (backup->IsPrimaryVehicle()) IncreaseGroupNumVehicle(backup->group_id);
		}

		/* Update hash. */
		Vehicle *dummy = dest;
		dest->old_new_hash = &dummy;
		dest->left_coord = INVALID_COORD;
		UpdateVehiclePosHash(dest, INVALID_COORD, 0);

		if (is_road_veh) {
			/* Removed the slot in the road vehicles as the slot is gone.
			 * We don't want a pointer to a slot that's gone.              */
			dest->u.road.slot = NULL;
		}

		if (!dest->cargo.Empty()) {
			/* The vehicle in question contains some cargo.
			 * However we lost the list so we will have to recreate it.
			 * We know that the packets are stored in the same order as the vehicles so
			 * the one cargo_packets points to and maybe some following ones belongs to
			 * the current vehicle.
			 * Now all we have to do is to add the packets to a list and keep track of how
			 * much cargo we restore and once we reached the cached cargo hold we recovered
			 * everything for this vehicle. */
			uint cargo_count = 0;
			for(; cargo_count < dest->cargo.Count(); cargo++) {
				dest->cargo.packets.push_back(GetCargoPacket(cargo->index));
				cargo_count += cargo->count;
			}
			/* This design should always end up with the right amount of cargo. */
			assert(cargo_count == dest->cargo.Count());
		}

		if (backup->Next() == NULL) break;
		backup++;
	}
	return GetVehicle(v->index);
}

/** Restores a backed up vehicle
 * @param *v A vehicle we should sell and take the windows from (NULL for not using this)
 * @param *p The owner of the vehicle
 * @return The vehicle we restored (front for trains) or v if we didn't have anything to restore
 */
Vehicle *BackuppedVehicle::Restore(Vehicle *v, Player *p)
{
	if (!ContainsBackup()) return v;
	if (v != NULL) {
		ChangeVehicleViewWindow(v->index, INVALID_VEHICLE);
		DoCommand(0, v->index, 1, DC_EXEC, GetCmdSellVeh(v));
	}
	v = RestoreBackupVehicle(this->vehicles, p);
	ChangeVehicleViewWindow(INVALID_VEHICLE, v->index);
	if (orders != NULL) RestoreVehicleOrdersBruteForce(v, orders);
	if (economy != NULL) economy->Restore();
	/* If we stored cargo as well then we should restore it. */
	cargo_packets->RestoreBackup();
	return v;
}

/** Backs up a vehicle
 * This should never be called when the object already contains a backup
 * @param v the vehicle to backup
 * @param p If it's set to the vehicle's owner then economy is backed up. If NULL then economy backup will be skipped.
 */
void BackuppedVehicle::Backup(Vehicle *v, Player *p)
{
	assert(!ContainsBackup());
	if (p != NULL) {
		assert(p->index == v->owner);
		economy = new PlayerMoneyBackup(p);
	}
	BackupVehicle(v);
	if (orders != NULL) BackupVehicleOrders(v, orders);
}

void StopAllVehicles()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* Code ripped from CmdStartStopTrain. Can't call it, because of
		 * ownership problems, so we'll duplicate some code, for now */
		v->vehstatus |= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}
}
