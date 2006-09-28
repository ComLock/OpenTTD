/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "ship.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "station.h"
#include "command.h"
#include "player.h"
#include "engine.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "date.h"

/**
 * Draw the purchase info details of a ship at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawShipPurchaseInfo(int x, int y, EngineID engine_number)
{
	YearMonthDay ymd;
	const ShipVehicleInfo *svi = ShipVehInfo(engine_number);
	const Engine *e;

	/* Purchase cost - Max speed */
	SetDParam(0, svi->base_cost * (_price.ship_base>>3)>>5);
	SetDParam(1, svi->max_speed / 2);
	DrawString(x,y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Cargo type + capacity */
	SetDParam(0, _cargoc.names_long[svi->cargo_type]);
	SetDParam(1, svi->capacity);
	SetDParam(2, svi->refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	DrawString(x,y, STR_PURCHASE_INFO_CAPACITY, 0);
	y += 10;

	/* Running cost */
	SetDParam(0, svi->running_cost * _price.ship_running >> 8);
	DrawString(x,y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Design date - Life length */
	e = GetEngine(engine_number);
	ConvertDateToYMD(e->intro_date, &ymd);
	SetDParam(0, ymd.year);
	SetDParam(1, e->lifelength);
	DrawString(x,y, STR_PURCHASE_INFO_DESIGNED_LIFE, 0);
	y += 10;

	/* Reliability */
	SetDParam(0, e->reliability * 100 >> 16);
	DrawString(x,y, STR_PURCHASE_INFO_RELIABILITY, 0);
	y += 10;

	/* Additional text from NewGRF */
	// XXX 227 will become a calculated width...
	y += ShowAdditionalText(x, y, 227, engine_number);
}

void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	DrawSprite(GetShipImage(v, DIR_W) | GetVehiclePalette(v), x + 32, y + 10);

	if (v->index == selection) {
		DrawFrameRect(x - 5, y - 1, x + 67, y + 21, 15, FR_BORDERONLY);
	}
}

static void ShipDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);
		StringID str;

		w->disabled_state = v->owner == _local_player ? 0 : (1 << 2);
		// disable service-scroller when interval is set to disabled
		if (!_patches.servint_ships) w->disabled_state |= (1 << 5) | (1 << 6);

		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;

			SetDParam(1, year);

			SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
			SetDParam(2, v->max_age / 366);
			SetDParam(3, ShipVehInfo(v->engine_type)->running_cost * _price.ship_running >> 8);
			DrawString(2, 15, STR_9812_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->max_speed / 2);
			DrawString(2, 25, STR_9813_MAX_SPEED, 0);
		}

		/* Draw profit */
		{
			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(2, 35, STR_9814_PROFIT_THIS_YEAR_LAST_YEAR, 0);
		}

		/* Draw breakdown & reliability */
		{
			SetDParam(0, v->reliability * 100 >> 16);
			SetDParam(1, v->breakdowns_since_last_service);
			DrawString(2, 45, STR_9815_RELIABILITY_BREAKDOWNS, 0);
		}

		/* Draw service interval text */
		{
			SetDParam(0, v->service_interval);
			SetDParam(1, v->date_of_last_service);
			DrawString(13, 90, _patches.servint_ispercent?STR_SERVICING_INTERVAL_PERCENT:STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}

		DrawShipImage(v, 3, 57, INVALID_VEHICLE);

		SetDParam(1, v->build_year);
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(2, v->value);
		DrawString(74, 57, STR_9816_BUILT_VALUE, 0);

		SetDParam(0, _cargoc.names_long[v->cargo_type]);
		SetDParam(1, v->cargo_cap);
		DrawString(74, 67, STR_9817_CAPACITY, 0);

		str = STR_8812_EMPTY;
		if (v->cargo_count != 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo_count);
			SetDParam(2, v->cargo_source);
			str = STR_8813_FROM;
		}
		DrawString(74, 78, str, 0);
	} break;

	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* rename */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_9831_NAME_SHIP, 31, 150, w->window_class, w->window_number, CS_ALPHANUMERAL);
			break;
		case 5: /* increase int */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;
		case 6: /* decrease int */
			mod = _ctrl_pressed?- 5 : -10;
do_change_service_int:
			v = GetVehicle(w->window_number);

			mod = GetServiceIntervalClamped(mod + v->service_interval);
			if (mod == v->service_interval) return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		}
	} break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_NAME_VEHICLE | CMD_MSG(STR_9832_CAN_T_NAME_SHIP));
		}
		break;
	}
}


static const Widget _ship_details_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   364,     0,    13, STR_9811_DETAILS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   365,   404,     0,    13, STR_01AA_NAME,    STR_982F_NAME_SHIP},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   404,    14,    55, 0x0,              STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   404,    56,    88, 0x0,              STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    89,    94, STR_0188,         STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    95,   100, STR_0189,         STR_884E_DECREASE_SERVICING_INTERVAL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,    11,   404,    89,   100, 0x0,              STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _ship_details_desc = {
	-1,-1, 405, 101,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_ship_details_widgets,
	ShipDetailsWndProc
};

static void ShowShipDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);
	_alloc_wnd_parent_num = veh;
	w = AllocateWindowDesc(&_ship_details_desc);
	w->window_number = veh;
	w->caption_color = v->owner;
}

void CcBuildShip(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;
	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowShipViewWindow(v);
}

void CcCloneShip(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowShipViewWindow(GetVehicle(_new_vehicle_id));
}

static void NewShipWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			EngineID selected_id;
			EngineID eid;
			int count;
			int pos;
			int sel;
			int y;

			if (w->window_number == 0) w->disabled_state = 1 << 5;

			count = 0;
			for (eid = SHIP_ENGINES_INDEX; eid < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; eid++) {
				if (HASBIT(GetEngine(eid)->player_avail, _local_player)) count++;
			}
			SetVScrollCount(w, count);

			DrawWindowWidgets(w);

			y = 15;
			sel = WP(w,buildtrain_d).sel_index;
			pos = w->vscroll.pos;
			selected_id = INVALID_ENGINE;
			for (eid = SHIP_ENGINES_INDEX; eid < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; eid++) {
				if (!HASBIT(GetEngine(eid)->player_avail, _local_player)) continue;
				if (sel == 0) selected_id = eid;
				if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
					DrawString(77, y + 7, GetCustomEngineName(eid), sel == 0 ? 0xC : 0x10);
					DrawShipEngine(37, y + 10, eid, GetEnginePalette(eid, _local_player));
					y += 24;
				}
				sel--;
			}

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != INVALID_ENGINE) {
				DrawShipPurchaseInfo(2, w->widget[4].top + 1, selected_id);
			}
			break;
		}

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2: { /* listbox */
			uint i = (e->we.click.pt.y - 14) / 24;
			if (i < w->vscroll.cap) {
				WP(w,buildtrain_d).sel_index = i + w->vscroll.pos;
				SetWindowDirty(w);
			}
		} break;
		case 5: { /* build */
			EngineID sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != INVALID_ENGINE)
				DoCommandP(w->window_number, sel_eng, 0, CcBuildShip, CMD_BUILD_SHIP | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP));
		} break;

		case 6: { /* rename */
			EngineID sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				WP(w,buildtrain_d).rename_engine = sel_eng;
				ShowQueryString(GetCustomEngineName(sel_eng),
					STR_9838_RENAME_SHIP_TYPE, 31, 160, w->window_class, w->window_number, CS_ALPHANUMERAL);
			}
		}	break;
		}
		break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, WP(w, buildtrain_d).rename_engine, 0, NULL,
				CMD_RENAME_ENGINE | CMD_MSG(STR_9839_CAN_T_RENAME_SHIP_TYPE));
		}
		break;

	case WE_RESIZE:
		w->vscroll.cap += e->we.sizing.diff.y / 24;
		w->widget[2].data = (w->vscroll.cap << 8) + 1;
		break;

	}
}

static const Widget _new_ship_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   254,     0,    13, STR_9808_NEW_SHIPS,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   242,    14,   109, 0x401,               STR_9825_SHIP_SELECTION_LIST_CLICK},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   243,   254,    14,   109, 0x0,                 STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,     RESIZE_TB,    14,     0,   254,   110,   161, 0x0,                 STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   121,   162,   173, STR_9809_BUILD_SHIP, STR_9826_BUILD_THE_HIGHLIGHTED_SHIP},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   122,   242,   162,   173, STR_9836_RENAME,     STR_9837_RENAME_SHIP_TYPE},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   243,   254,   162,   173, 0x0,                 STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _new_ship_desc = {
	-1, -1, 255, 174,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_new_ship_widgets,
	NewShipWndProc
};


void ShowBuildShipWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_ship_desc);
	w->window_number = tile;
	w->vscroll.cap = 4;
	w->widget[2].data = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 24;

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
	} else {
		w->caption_color = _local_player;
	}

}


static void ShipViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			Vehicle *v = GetVehicle(w->window_number);
			uint32 disabled = 1<<8;
			StringID str;

			// Possible to refit?
			if (ShipVehInfo(v->engine_type)->refittable && IsShipInDepotStopped(v)) {
				disabled = 0;
			}

			if (v->owner != _local_player) disabled |= 1<<8 | 1<<7;
			w->disabled_state = disabled;

			/* draw widgets & caption */
			SetDParam(0, v->string_id);
			SetDParam(1, v->unitnumber);
			DrawWindowWidgets(w);

			if (v->breakdown_ctr == 1) {
				str = STR_885C_BROKEN_DOWN;
			} else if (v->vehstatus & VS_STOPPED) {
				str = STR_8861_STOPPED;
			} else {
				switch (v->current_order.type) {
					case OT_GOTO_STATION: {
						SetDParam(0, v->current_order.dest);
						SetDParam(1, v->cur_speed / 2);
						str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
					} break;

					case OT_GOTO_DEPOT: {
						Depot *depot = GetDepot(v->current_order.dest);
						SetDParam(0, depot->town_index);
						SetDParam(1, v->cur_speed / 2);
						if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
							str = STR_HEADING_FOR_SHIP_DEPOT + _patches.vehicle_speed;
						} else {
							str = STR_HEADING_FOR_SHIP_DEPOT_SERVICE + _patches.vehicle_speed;
						}
					} break;

					case OT_LOADING:
					case OT_LEAVESTATION:
						str = STR_882F_LOADING_UNLOADING;
						break;

					default:
						if (v->num_orders == 0) {
							str = STR_NO_ORDERS + _patches.vehicle_speed;
							SetDParam(0, v->cur_speed / 2);
						} else {
							str = STR_EMPTY;
						}
						break;
				}
			}

		/* draw the flag plus orders */
		DrawSprite(v->vehstatus & VS_STOPPED ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, 2, w->widget[5].top + 1);
		DrawStringCenteredTruncated(w->widget[5].left + 8, w->widget[5].right, w->widget[5].top + 1, str, 0);
		DrawWindowViewport(w);
	} break;

		case WE_CLICK: {
			const Vehicle *v = GetVehicle(w->window_number);

			switch (e->we.click.widget) {
				case 5: /* start stop */
					DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP));
					break;
				case 6: /* center main view */
					ScrollMainWindowTo(v->x_pos, v->y_pos);
					break;
				case 7: /* goto hangar */
					DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL, CMD_SEND_SHIP_TO_DEPOT | CMD_MSG(STR_9819_CAN_T_SEND_SHIP_TO_DEPOT));
					break;
				case 8: /* refit */
					ShowVehicleRefitWindow(v);
					break;
				case 9: /* show orders */
					ShowOrdersWindow(v);
					break;
				case 10: /* show details */
					ShowShipDetailsWindow(v);
					break;
				case 11: {
					/* clone vehicle */
					DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, CcCloneShip, CMD_CLONE_VEHICLE | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP));
				} break;
			}
		} break;

		case WE_RESIZE:
			w->viewport->width          += e->we.sizing.diff.x;
			w->viewport->height         += e->we.sizing.diff.y;
			w->viewport->virtual_width  += e->we.sizing.diff.x;
			w->viewport->virtual_height += e->we.sizing.diff.y;
			break;

		case WE_DESTROY:
			DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
			DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
			DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
			break;

		case WE_MOUSELOOP:
		{
			const Vehicle *v = GetVehicle(w->window_number);
			uint32 h = IsShipInDepotStopped(v) ? 1 << 7 : 1 << 11;

			if (h != w->hidden_state) {
				w->hidden_state = h;
				SetWindowDirty(w);
			}
		}
	}
}

static const Widget _ship_view_widgets[] = {
{ WWT_CLOSEBOX,   RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,       STR_018B_CLOSE_WINDOW},
{ WWT_CAPTION,    RESIZE_RIGHT, 14,  11, 237,   0,  13, STR_980F,       STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_STICKYBOX,  RESIZE_LR,    14, 238, 249,   0,  13, 0x0,            STR_STICKY_BUTTON},
{ WWT_IMGBTN,     RESIZE_RB,    14,   0, 231,  14, 103, 0x0,            STR_NULL},
{ WWT_6,          RESIZE_RB,    14,   2, 229,  16, 101, 0x0,            STR_NULL},
{ WWT_PUSHIMGBTN, RESIZE_RTB,   14,   0, 237, 104, 115, 0x0,            STR_9827_CURRENT_SHIP_ACTION_CLICK},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  14,  31, 0x2AB,          STR_9829_CENTER_MAIN_VIEW_ON_SHIP},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, 0x2B0,          STR_982A_SEND_SHIP_TO_DEPOT},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  50,  67, 0x2B4,          STR_983A_REFIT_CARGO_SHIP_TO_CARRY},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  68,  85, 0x2B2,          STR_9828_SHOW_SHIP_S_ORDERS},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  86, 103, 0x2B3,          STR_982B_SHOW_SHIP_DETAILS},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, SPR_CLONE_SHIP, STR_CLONE_SHIP_INFO},
{ WWT_PANEL,      RESIZE_LRB,   14, 232, 249, 104, 103, 0x0,            STR_NULL },
{ WWT_RESIZEBOX,  RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,            STR_NULL },
{ WIDGETS_END }
};

static const WindowDesc _ship_view_desc = {
	-1,-1, 250, 116,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_ship_view_widgets,
	ShipViewWndProc
};

void ShowShipViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_ship_view_desc, v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), 0);
	}
}

void DrawSmallOrderListShip(const Vehicle *v, int x, int y)
{
	const Order *order;
	int sel, i = 0;

	sel = v->cur_order_index;

	FOR_VEHICLE_ORDERS(v, order) {
		if (sel == 0) DrawString(x - 6, y, STR_SMALL_RIGHT_ARROW, 16);
		sel--;

		if (order->type == OT_GOTO_STATION) {
			if (!IsBuoy(GetStation(order->dest))) {
				SetDParam(0, order->dest);
				DrawString(x, y, STR_A036, 0);

				y += 6;
				if (++i == 4) break;
			}
		}
	}
}
