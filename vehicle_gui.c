#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "vehicle.h"
#include "window.h"
#include "engine.h"
#include "gui.h"
#include "command.h"
#include "gfx.h"

VehicleSortListingTypeFunctions * const _vehicle_sorter[] = {
	&VehicleUnsortedSorter,
	&VehicleNumberSorter,
	&VehicleNameSorter,
	&VehicleAgeSorter,
	&VehicleProfitThisYearSorter,
	&VehicleProfitLastYearSorter,
	&VehicleCargoSorter,
	&VehicleReliabilitySorter,
	&VehicleMaxSpeedSorter
};

const StringID _vehicle_sort_listing[] = {
	STR_SORT_BY_UNSORTED,
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_AGE,
	STR_SORT_BY_PROFIT_THIS_YEAR,
	STR_SORT_BY_PROFIT_LAST_YEAR,
	STR_SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_MAX_SPEED,
	INVALID_STRING_ID
};

const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
	STR_MONORAIL_VEHICLES,
	STR_MAGLEV_VEHICLES,
	INVALID_STRING_ID
};

void RebuildVehicleLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w)
		switch (w->window_class) {
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
				WP(w, vehiclelist_d).flags |= VL_REBUILD;
				SetWindowDirty(w);
				break;

			default:
				break;
		}
}

void ResortVehicleLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w)
		switch (w->window_class) {
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
				WP(w, vehiclelist_d).flags |= VL_RESORT;
				SetWindowDirty(w);
				break;

			default:
				break;
		}
}

void BuildVehicleList(vehiclelist_d *vl, int type, int owner, int station)
{
	int subtype = (type != VEH_Aircraft) ? 0 : 2;
	int n = 0;
	int i;

	if (!(vl->flags & VL_REBUILD)) return;

	/* Create array for sorting */
	_vehicle_sort = realloc(_vehicle_sort, _vehicles_size * sizeof(_vehicle_sort[0]));
	if (_vehicle_sort == NULL)
		error("Could not allocate memory for the vehicle-sorting-list");

	DEBUG(misc, 1) ("Building vehicle list for player %d station %d...",
		owner, station);

	if (station != -1) {
		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->subtype <= subtype) {
				const Order *ord;
				for (ord = v->schedule_ptr; ord->type != OT_NOTHING; ++ord)
					if (ord->type == OT_GOTO_STATION && ord->station == station) {
						_vehicle_sort[n].index = v->index;
						_vehicle_sort[n].owner = v->owner;
						++n;
						break;
					}
			}
		}
	} else {
		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->subtype <= subtype && v->owner == owner) {
				_vehicle_sort[n].index = v->index;
				_vehicle_sort[n].owner = v->owner;
				++n;
			}
		}
	}

	vl->sort_list = realloc(vl->sort_list, n * sizeof(vl->sort_list[0]));
	if (n!=0 && vl->sort_list == NULL)
		error("Could not allocate memory for the vehicle-sorting-list");
	vl->list_length = n;

	for (i = 0; i < n; ++i)
		vl->sort_list[i] = _vehicle_sort[i];

	vl->flags &= ~VL_REBUILD;
	vl->flags |= VL_RESORT;
}

void SortVehicleList(vehiclelist_d *vl)
{
	if (!(vl->flags & VL_RESORT)) return;

	_internal_sort_order = vl->flags & VL_DESC;
	_internal_name_sorter_id = STR_SV_TRAIN_NAME;
	_last_vehicle_idx = 0; // used for "cache" in namesorting
	qsort(vl->sort_list, vl->list_length, sizeof(vl->sort_list[0]),
		_vehicle_sorter[vl->sort_type]);

	vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	vl->flags &= ~VL_RESORT;
}


/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList()
{
}

// draw the vehicle profit button in the vehicle list window.
void DrawVehicleProfitButton(Vehicle *v, int x, int y)
{
	uint32 ormod;

	// draw profit-based colored icons
	if(v->age <= 365 * 2)
		ormod = 0x3158000; // grey
	else if(v->profit_last_year < 0)
		ormod = 0x30b8000; //red
	else if(v->profit_last_year < 10000)
		ormod = 0x30a8000; // yellow
	else
		ormod = 0x30d8000; // green
	DrawSprite((SPR_BLOT) | ormod, x, y);
}

/************ Sorter functions *****************/
int CDECL GeneralOwnerSorter(const void *a, const void *b)
{
	return (*(const SortStruct*)a).owner - (*(const SortStruct*)b).owner;
}

/* Variables you need to set before calling this function!
* 1. (byte)_internal_sort_type:					sorting criteria to sort on
* 2. (bool)_internal_sort_order:				sorting order, descending/ascending
* 3. (uint32)_internal_name_sorter_id:	default StringID of the vehicle when no name is set. eg
*    STR_SV_TRAIN_NAME for trains or STR_SV_AIRCRAFT_NAME for aircraft
*/
int CDECL VehicleUnsortedSorter(const void *a, const void *b)
{
	return GetVehicle((*(const SortStruct*)a).index)->index - GetVehicle((*(const SortStruct*)b).index)->index;
}

// if the sorting criteria had the same value, sort vehicle by unitnumber
#define VEHICLEUNITNUMBERSORTER(r, a, b) {if (r == 0) {r = a->unitnumber - b->unitnumber;}}

int CDECL VehicleNumberSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

static char _bufcache[64];	// used together with _last_vehicle_idx to hopefully speed up stringsorting
int CDECL VehicleNameSorter(const void *a, const void *b)
{
	const SortStruct *cmp1 = (const SortStruct*)a;
	const SortStruct *cmp2 = (const SortStruct*)b;
	const Vehicle *va = GetVehicle(cmp1->index);
	const Vehicle *vb = GetVehicle(cmp2->index);
	char buf1[64] = "\0";
	int r;

	if (va->string_id != _internal_name_sorter_id) {
		SetDParam(0, va->string_id);
		GetString(buf1, STR_0315);
	}

	if ( cmp2->index != _last_vehicle_idx) {
		_last_vehicle_idx = cmp2->index;
		_bufcache[0] = '\0';
		if (vb->string_id != _internal_name_sorter_id) {
			SetDParam(0, vb->string_id);
			GetString(_bufcache, STR_0315);
		}
	}

	r =  strcmp(buf1, _bufcache);	// sort by name

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleAgeSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->age - vb->age;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleProfitThisYearSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->profit_this_year - vb->profit_this_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->profit_last_year - vb->profit_last_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleCargoSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	const Vehicle *v;
	int r = 0;
	int i;
	uint _cargo_counta[NUM_CARGO];
	uint _cargo_countb[NUM_CARGO];
	memset(_cargo_counta, 0, sizeof(_cargo_counta));
	memset(_cargo_countb, 0, sizeof(_cargo_countb));

	for (v = va; v != NULL; v = v->next)
		_cargo_counta[v->cargo_type] += v->cargo_cap;

	for (v = vb; v != NULL; v = v->next)
		_cargo_countb[v->cargo_type] += v->cargo_cap;

	for (i = 0; i < NUM_CARGO; i++) {
		r = _cargo_counta[i] - _cargo_countb[i];
		if (r != 0)
			break;
	}

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleReliabilitySorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->reliability - vb->reliability;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleMaxSpeedSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->max_speed - vb->max_speed;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

// this define is to match engine.c, but engine.c keeps it to itself
// ENGINE_AVAILABLE is used in ReplaceVehicleWndProc
#define ENGINE_AVAILABLE ((e->flags & 1 && HASBIT(info->railtype_climates, _opt.landscape)) || HASBIT(e->player_avail, _local_player))

/*  if show_outdated is selected, it do not sort psudo engines properly but it draws all engines
 *	if used compined with show_cars set to false, it will work as intended. Replace window do it like that
 *  this was a big hack even before show_outdated was added. Stupid newgrf :p										*/
static void train_engine_drawing_loop(int *x, int *y, int *pos, int *sel, int *selected_id, byte railtype,
	uint8 lines_drawn, bool is_engine, bool show_cars, bool show_outdated)
{
	int i;
	byte colour;

	for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
		const Engine *e = DEREF_ENGINE(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const EngineInfo *info = &_engine_info[i];

		if ( rvi->power == 0 && !(show_cars) )   // disables display of cars (works since they do not have power)
			continue;

		if (*sel == 0) *selected_id = i;


		colour = *sel == 0 ? 0xC : 0x10;
		if (!(ENGINE_AVAILABLE && show_outdated && RailVehInfo(i)->power && e->railtype == railtype)) {
			if (e->railtype != railtype || !(rvi->flags & RVI_WAGON) != is_engine ||
				!HASBIT(e->player_avail, _local_player))
				continue;
		} /*else {
		// TODO find a nice red colour for vehicles being replaced
			if ( _autoreplace_array[i] != i )
				colour = *sel == 0 ? 0x44 : 0x45;
		} */

		if (IS_INT_INSIDE(--*pos, -lines_drawn, 0)) {
			DrawString(*x + 59, *y + 2, GetCustomEngineName(i),
				colour);
			DrawTrainEngine(*x + 29, *y + 6, i,
				SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
			*y += 14;
		}
		--*sel;
	}
}


static void SetupScrollStuffForReplaceWindow(Window *w)
{
	byte railtype;
	int selected_id[2] = {-1,-1};
	int sel[2];
	int count = 0;
	int count2 = 0;
	int engine_id;

	sel[0] = WP(w,replaceveh_d).sel_index[0];
	sel[1] = WP(w,replaceveh_d).sel_index[1];

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			railtype = WP(w,replaceveh_d).railtype;
			w->widget[13].color = _player_colors[_local_player];	// sets the colour of that art thing
			w->widget[16].color = _player_colors[_local_player];	// sets the colour of that art thing
			for (engine_id = 0; engine_id < NUM_TRAIN_ENGINES; engine_id++) {
				const Engine *e = DEREF_ENGINE(engine_id);
				const EngineInfo *info = &_engine_info[engine_id];

				if (ENGINE_AVAILABLE && RailVehInfo(engine_id)->power && e->railtype == railtype) {
					count++;
					if (sel[0]==0)  selected_id[0] = engine_id;
					sel[0]--;
					if (HASBIT(e->player_avail, _local_player)) {
						if (sel[1]==0)  selected_id[1] = engine_id;
							count2++;
							sel[1]--;
						}
					}
				}
			break;
			}
		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			Engine *e = DEREF_ENGINE(ROAD_ENGINES_INDEX);
			byte cargo;
			EngineInfo *info;
			engine_id = ROAD_ENGINES_INDEX;

			do {
				info = &_engine_info[engine_id];
				if (ENGINE_AVAILABLE) {
					if (sel[0]==0)  selected_id[0] = engine_id;
					count++;
					sel[0]--;
				}
			} while (++engine_id,++e,--num);

			if ( selected_id[0] != -1 ) {   // only draw right array if we have anything in the left one
				num = NUM_ROAD_ENGINES;
				engine_id = ROAD_ENGINES_INDEX;
				e = DEREF_ENGINE(ROAD_ENGINES_INDEX);
				cargo = RoadVehInfo(selected_id[0])->cargo_type;

				do {
					if ( cargo == RoadVehInfo(engine_id)->cargo_type && HASBIT(e->player_avail, _local_player)) {
						count2++;
						if (sel[1]==0)  selected_id[1] = engine_id;
						sel[1]--;
					}
				} while (++engine_id,++e,--num);
			}
			break;
		}

		case VEH_Ship: {
			int num = NUM_SHIP_ENGINES;
			Engine *e = DEREF_ENGINE(SHIP_ENGINES_INDEX);
			byte cargo, refittable;
			EngineInfo *info;
			engine_id = SHIP_ENGINES_INDEX;

			do {
				info = &_engine_info[engine_id];
				if (ENGINE_AVAILABLE) {
					if ( sel[0] == 0 )  selected_id[0] = engine_id;
					count++;
					sel[0]--;
				}
			} while (++engine_id,++e,--num);

			if ( selected_id[0] != -1 ) {
				num = NUM_SHIP_ENGINES;
				e = DEREF_ENGINE(SHIP_ENGINES_INDEX);
				engine_id = SHIP_ENGINES_INDEX;
				cargo = ShipVehInfo(selected_id[0])->cargo_type;
				refittable = ShipVehInfo(selected_id[0])->refittable;

				do {
					if (HASBIT(e->player_avail, _local_player)
					&& ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {

						if ( sel[1]==0)  selected_id[1] = engine_id;
						sel[1]--;
						count2++;
					}
				} while (++engine_id,++e,--num);
			}
			break;
		}   //end of ship

		case VEH_Aircraft:{
			int num = NUM_AIRCRAFT_ENGINES;
			byte subtype;
			Engine *e = DEREF_ENGINE(AIRCRAFT_ENGINES_INDEX);
			EngineInfo *info;
			engine_id = AIRCRAFT_ENGINES_INDEX;

			do {
				info = &_engine_info[engine_id];
				if (ENGINE_AVAILABLE) {
					count++;
					if (sel[0]==0)  selected_id[0] = engine_id;
					sel[0]--;
				}
			} while (++engine_id,++e,--num);

			if ( selected_id[0] != -1 ) {
				num = NUM_AIRCRAFT_ENGINES;
				e = DEREF_ENGINE(AIRCRAFT_ENGINES_INDEX);
				subtype = AircraftVehInfo(selected_id[0])->subtype;
				engine_id = AIRCRAFT_ENGINES_INDEX;
				do {
					if (HASBIT(e->player_avail, _local_player)) {
						if ( (subtype && AircraftVehInfo(engine_id)->subtype) || (!(subtype) && !AircraftVehInfo(engine_id)->subtype) ) {
							count2++;
							if (sel[1]==0)  selected_id[1] = engine_id;
							sel[1]--;
						}
					}
				} while (++engine_id,++e,--num);
			}
			break;
		}
	}
	// sets up the number of items in each list
	SetVScrollCount(w, count);
	SetVScroll2Count(w, count2);
	WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
	WP(w,replaceveh_d).sel_engine[1] = selected_id[1];

	WP(w,replaceveh_d).count[0] = count;
	WP(w,replaceveh_d).count[1] = count2;
	return;
}


static void DrawEngineArrayInReplaceWindow(Window *w, int x, int y, int x2, int y2, int pos, int pos2,
	int sel1, int sel2, int selected_id1, int selected_id2)
{
	int sel[2];
	int selected_id[2];

	sel[0] = sel1;
	sel[1] = sel2;

	selected_id[0] = selected_id1;
	selected_id[1] = selected_id2;

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			byte railtype = WP(w,replaceveh_d).railtype;
			DrawString(157, 89 + (14 * w->vscroll.cap), _rail_types_list[railtype], 0x10);
			/* draw sorting criteria string */

			/* Ensure that custom engines which substituted wagons
			* are sorted correctly.
			* XXX - DO NOT EVER DO THIS EVER AGAIN! GRRR hacking in wagons as
			* engines to get more types.. Stays here until we have our own format
			* then it is exit!!! */
			train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, true, false, true); // True engines
			train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, true, false, false); // True engines
			train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, false, false); // Feeble wagons
			break;
		}

		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			Engine *e = DEREF_ENGINE(ROAD_ENGINES_INDEX);
			int engine_id = ROAD_ENGINES_INDEX;
			byte cargo;
			EngineInfo *info;

			if ( selected_id[0] >= ROAD_ENGINES_INDEX && selected_id[0] <= SHIP_ENGINES_INDEX ) {
				cargo = RoadVehInfo(selected_id[0])->cargo_type;

				do {
					info = &_engine_info[engine_id];
					if (ENGINE_AVAILABLE) {
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+59, y+2, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x+29, y+6, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
							y += 14;
						}

						if ( RoadVehInfo(engine_id)->cargo_type == cargo && HASBIT(e->player_avail, _local_player) ) {
							if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0) && RoadVehInfo(engine_id)->cargo_type == cargo) {
								DrawString(x2+59, y2+2, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
								DrawRoadVehEngine(x2+29, y2+6, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
								y2 += 14;
							}
							sel[1]--;
						}
					sel[0]--;
					}
				} while (++engine_id, ++e,--num);
			}
			break;
		}

		case VEH_Ship: {
			int num = NUM_SHIP_ENGINES;
			Engine *e = DEREF_ENGINE(SHIP_ENGINES_INDEX);
			int engine_id = SHIP_ENGINES_INDEX;
			byte cargo, refittable;
			EngineInfo *info;

			if ( selected_id[0] != -1 ) {
				cargo = ShipVehInfo(selected_id[0])->cargo_type;
				refittable = ShipVehInfo(selected_id[0])->refittable;

				do {
					info = &_engine_info[engine_id];
					if (ENGINE_AVAILABLE) {
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawShipEngine(x+35, y+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
							y += 24;
						}
						if ( selected_id[0] != -1 ) {
							if (HASBIT(e->player_avail, _local_player) && ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {
								if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
									DrawString(x2+75, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
									DrawShipEngine(x2+35, y2+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
									y2 += 24;
								}
								sel[1]--;
							}
						}
						sel[0]--;
					}
				} while (++engine_id, ++e,--num);
			}
			break;
		}   //end of ship

		case VEH_Aircraft: {
			if ( selected_id[0] != -1 ) {
				int num = NUM_AIRCRAFT_ENGINES;
				Engine *e = DEREF_ENGINE(AIRCRAFT_ENGINES_INDEX);
				int engine_id = AIRCRAFT_ENGINES_INDEX;
				byte subtype = AircraftVehInfo(selected_id[0])->subtype;
				EngineInfo *info;

				do {
					info = &_engine_info[engine_id];
					if (ENGINE_AVAILABLE) {
						if (sel[0]==0) selected_id[0] = engine_id;
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x+29, y+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
							y += 24;
						}
						if ( ((subtype && AircraftVehInfo(engine_id)->subtype) || (!(subtype) && !AircraftVehInfo(engine_id)->subtype))
							&& HASBIT(e->player_avail, _local_player) ) {
							if (sel[1]==0) selected_id[1] = engine_id;
							if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
								DrawString(x2+62, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
								DrawAircraftEngine(x2+29, y2+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
								y2 += 24;
							}
						sel[1]--;
						}
					sel[0]--;
					}
				} while (++engine_id, ++e,--num);
			}
			break;
		}   // end of aircraft
	}

}
static void ReplaceVehicleWndProc(Window *w, WindowEvent *e)
{
	// these 3 variables is used if any of the lists is clicked
	uint16 click_scroll_pos = w->vscroll2.pos;
	uint16 click_scroll_cap = w->vscroll2.cap;
	byte click_side = 1;

	switch(e->event) {
		case WE_PAINT:
			{
				int pos = w->vscroll.pos;
				int selected_id[2] = {-1,-1};
				int x = 1;
				int y = 15;
				int pos2 = w->vscroll2.pos;
				int x2 = 1 + 228;
				int y2 = 15;
				int sel[2];
				sel[0] = WP(w,replaceveh_d).sel_index[0];
				sel[1] = WP(w,replaceveh_d).sel_index[1];

				SetupScrollStuffForReplaceWindow(w);

				selected_id[0] = WP(w,replaceveh_d).sel_engine[0];
				selected_id[1] = WP(w,replaceveh_d).sel_engine[1];

			// sets the selected left item to the top one if it's greater than the number of vehicles in the left side

				if ( WP(w,replaceveh_d).count[0] <= sel[0] ) {
					if (WP(w,replaceveh_d).count[0]) {
						sel[0] = 0;
						WP(w,replaceveh_d).sel_index[0] = 0;
						w->vscroll.pos = 0;
						// now we go back to set selected_id[1] properly
						SetWindowDirty(w);
						return;
					} else { //there are no vehicles in the left window
						selected_id[1] = -1;
					}
				}

				if ( WP(w,replaceveh_d).count[1] <= sel[1] ) {
					if (WP(w,replaceveh_d).count[1]) {
						sel[1] = 0;
						WP(w,replaceveh_d).sel_index[1] = 0;
						w->vscroll2.pos = 0;
						// now we go back to set selected_id[1] properly
						SetWindowDirty(w);
						return;
					} else { //there are no vehicles in the right window
						selected_id[1] = -1;
					}
				}

				if ( selected_id[0] == selected_id[1] || _autoreplace_array[selected_id[0]] == selected_id[1]
					|| selected_id[0] == -1 || selected_id[1] == -1 )
					SETBIT(w->disabled_state, 4);
				else
					CLRBIT(w->disabled_state, 4);

				if ( _autoreplace_array[selected_id[0]] == selected_id[0] || selected_id[0] == -1 )
					SETBIT(w->disabled_state, 6);
				else
					CLRBIT(w->disabled_state, 6);

				// now the actual drawing of the window itself takes place
				DrawWindowWidgets(w);



				// sets up the string for the vehicle that is being replaced to
				if ( selected_id[0] != -1 ) {
					if ( selected_id[0] == _autoreplace_array[selected_id[0]] )
						SetDParam(0, STR_NOT_REPLACING);
					else
						SetDParam(0, GetCustomEngineName(_autoreplace_array[selected_id[0]]));
				} else {
					SetDParam(0, STR_NOT_REPLACING_VEHICLE_SELECTED);
				}


				DrawString(145, (w->resize.step_height == 24 ? 67 : 77 ) + ( w->resize.step_height * w->vscroll.cap), STR_02BD, 0x10);


				/*	now we draw the two arrays according to what we just counted */
				DrawEngineArrayInReplaceWindow(w, x, y, x2, y2, pos, pos2, sel[0], sel[1], selected_id[0], selected_id[1]);

				WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
				WP(w,replaceveh_d).sel_engine[1] = selected_id[1];
				/* now we draw the info about the vehicles we selected */
				switch (WP(w,replaceveh_d).vehicletype) {
					case VEH_Train: {
						byte i = 0;
						int offset = 0;

						for ( i = 0 ; i < 2 ; i++) {
							if ( i )
							offset = 228;
							if (selected_id[i] != -1) {
								if (!(RailVehInfo(selected_id[i])->flags & RVI_WAGON)) {
									/* it's an engine */
									Set_DPARAM_Train_Engine_Build_Window(selected_id[i]);
									DrawString(2 + offset, 15 + (14 * w->vscroll.cap), STR_8817_COST_WEIGHT_T_SPEED_POWER, 0);
								} else {
									/* it's a wagon. Train cars are not replaced with the current GUI, but this code is ready for newgrf if anybody adds that*/
									Set_DPARAM_Train_Car_Build_Window(w, selected_id[i]);
									DrawString(2 + offset, 15 + (14 * w->vscroll.cap), STR_8821_COST_WEIGHT_T_T_CAPACITY, 0);
								}
							}
						}
						break;
					}   //end if case  VEH_Train

					case VEH_Road: {
						if (selected_id[0] != -1) {
							Set_DPARAM_Road_Veh_Build_Window(selected_id[0]);
							DrawString(2, 15 + (14 * w->vscroll.cap), STR_9008_COST_SPEED_RUNNING_COST, 0);
							if (selected_id[1] != -1) {
								Set_DPARAM_Road_Veh_Build_Window(selected_id[1]);
								DrawString(2 + 228, 15 + (14 * w->vscroll.cap), STR_9008_COST_SPEED_RUNNING_COST, 0);
							}
						}
						break;
					}   // end of VEH_Road

					case VEH_Ship: {
						if (selected_id[0] != -1) {
							Set_DPARAM_Ship_Build_Window(selected_id[0]);
							DrawString(2, 15 + (24 * w->vscroll.cap), STR_980A_COST_SPEED_CAPACITY_RUNNING, 0);
							if (selected_id[1] != -1) {
								Set_DPARAM_Ship_Build_Window(selected_id[1]);
								DrawString(2 + 228, 15 + (24 * w->vscroll.cap), STR_980A_COST_SPEED_CAPACITY_RUNNING, 0);
							}
						}
						break;
					}   // end of VEH_Ship

					case VEH_Aircraft: {
						if (selected_id[0] != -1) {
							Set_DPARAM_Aircraft_Build_Window(selected_id[0]);
							DrawString(2, 15 + (24 * w->vscroll.cap), STR_A007_COST_SPEED_CAPACITY_PASSENGERS, 0);
							if (selected_id[1] != -1) {
								Set_DPARAM_Aircraft_Build_Window(selected_id[1]);
								DrawString(2 + 228, 15 + (24 * w->vscroll.cap), STR_A007_COST_SPEED_CAPACITY_PASSENGERS, 0);
							}
						}
						break;
					}   // end of VEH_Aircraft
				}
			}   // end of paint

		case WE_CLICK: {
			switch(e->click.widget) {
				case 14: case 15:/* Select sorting criteria dropdown menu */
				// finds mask for available engines
				{
					int engine_avail = 0;
					if ( !(HASBIT(DEREF_ENGINE(NUM_NORMAL_RAIL_ENGINES + NUM_MONORAIL_ENGINES)->player_avail, _local_player))) {
						engine_avail = 4;
						if ( !(HASBIT(DEREF_ENGINE(NUM_NORMAL_RAIL_ENGINES)->player_avail, _local_player)))
							engine_avail = 6;
					}
					ShowDropDownMenu(w, _rail_types_list, WP(w,replaceveh_d).railtype, 15, engine_avail, 1);
					break;
				}
				case 4: {
					_autoreplace_array[WP(w,replaceveh_d).sel_engine[0]] = WP(w,replaceveh_d).sel_engine[1];
					SetWindowDirty(w);
					break;
				}

				case 6: {
					_autoreplace_array[WP(w,replaceveh_d).sel_engine[0]] = WP(w,replaceveh_d).sel_engine[0];
					SetWindowDirty(w);
					break;
				}

				case 7:
					// sets up that the left one was clicked. The default values are for the right one (9)
					// this way, the code for 9 handles both sides
					click_scroll_pos = w->vscroll.pos;
					click_scroll_cap = w->vscroll.cap;
					click_side = 0;
				case 9: {
					uint i = (e->click.pt.y - 14) / w->resize.step_height;
					if (i < click_scroll_cap) {
						WP(w,replaceveh_d).sel_index[click_side] = i + click_scroll_pos;
						SetWindowDirty(w);
					}
				} break;
			}

		} break;

		case WE_DROPDOWN_SELECT: { /* we have selected a dropdown item in the list */
			//potiential bug: railtypes needs to be activated 0, 1, 2... If one is skipped, it messes up
			WP(w,replaceveh_d).railtype = e->dropdown.index;
			SetWindowDirty(w);
		} break;

		case WE_RESIZE: {
			w->vscroll.cap  += e->sizing.diff.y / (int)w->resize.step_height;
			w->vscroll2.cap += e->sizing.diff.y / (int)w->resize.step_height;

			w->widget[7].unkA = (w->vscroll.cap  << 8) + 1;
			w->widget[9].unkA = (w->vscroll2.cap << 8) + 1;
		} break;
	}
}

static const Widget _replace_rail_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, 0x0,						STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   187, 0x0,						STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   200,   211, STR_REPLACE_VEHICLES_START,STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   316,   188,   199, 0x0,						STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   200,   211, STR_REPLACE_VEHICLES_STOP,	STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,						STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, 0x0,						STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,						STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, 0x0,						STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   187, 0x0,						STR_NULL},
// train specific stuff
{      WWT_PANEL,     RESIZE_TB,    14,     0,   138,   188,   199, 0x0,						STR_NULL},
{      WWT_PANEL,     RESIZE_TB,	  14,   139,   153,   200,   211, 0x0,						STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,   154,   289,   200,   211, 0x0,						STR_REPLACE_HELP_RAILTYPE},
{   WWT_CLOSEBOX,     RESIZE_TB,    14,   279,   287,   201,   210, STR_0225,					STR_REPLACE_HELP_RAILTYPE},
{      WWT_PANEL,     RESIZE_TB,	  14,   290,   305,   200,   211, 0x0,						STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,   317,   455,   188,   199, 0x0,						STR_NULL},
// end of train specific stuff
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   200,   211, 0x0,						STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_road_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, 0x0,						STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   187, 0x0,						STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   188,   199, STR_REPLACE_VEHICLES_START,	STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   188,   199, 0x0,						STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   188,   199, STR_REPLACE_VEHICLES_STOP,	STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,						STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, 0x0,						STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,						STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, 0x0,						STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   187, 0x0,						STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   188,   199, 0x0,						STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_ship_aircraft_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, 0x0,						STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   110,   161, 0x0,						STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   162,   173, STR_REPLACE_VEHICLES_START,	STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   162,   173, 0x0,						STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   162,   173, STR_REPLACE_VEHICLES_STOP,	STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   109, 0x401,						STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   109, 0x0,						STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   109, 0x401,						STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   109, 0x0,						STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   110,   161, 0x0,						STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   162,   173, 0x0,						STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _replace_rail_vehicle_desc = {
	-1, -1, 456, 212,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_rail_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_road_vehicle_desc = {
	-1, -1, 456, 200,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_road_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_ship_aircraft_vehicle_desc = {
	-1, -1, 456, 174,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_ship_aircraft_vehicle_widgets,
	ReplaceVehicleWndProc
};


void ShowReplaceVehicleWindow(byte vehicletype)
{
	Window *w;

	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype );

	switch (vehicletype) {
		case VEH_Train:
			w = AllocateWindowDescFront(&_replace_rail_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			break;
		case VEH_Road:
			w = AllocateWindowDescFront(&_replace_road_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			break;
		case VEH_Ship: case VEH_Aircraft:
			w = AllocateWindowDescFront(&_replace_ship_aircraft_vehicle_desc, vehicletype);
			w->vscroll.cap  = 4;
			w->resize.step_height = 24;
			break;
		default: return;
	}
	w->caption_color = _local_player;
	WP(w,replaceveh_d).vehicletype = vehicletype;
	w->vscroll2.cap = w->vscroll.cap;   // these two are always the same
}
