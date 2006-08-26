/* $Id$ */

#ifndef STATION_H
#define STATION_H

#include "player.h"
#include "pool.h"
#include "sprite.h"
#include "tile.h"
#include "newgrf_station.h"
#include "window.h"

typedef struct GoodsEntry {
	uint16 waiting_acceptance;
	byte days_since_pickup;
	byte rating;
	StationID enroute_from;
	byte enroute_time;
	byte last_speed;
	byte last_age;
	int32 feeder_profit;
} GoodsEntry;

typedef enum RoadStopType {
	RS_BUS,
	RS_TRUCK
} RoadStopType;

enum {
	INVALID_STATION = 0xFFFF,
	ROAD_STOP_LIMIT = 16,
};

typedef struct RoadStop {
	TileIndex xy;
	bool used;
	byte status;
	RoadStopID index;
	byte num_vehicles;
	StationID station;
	struct RoadStop *next;
	struct RoadStop *prev;
} RoadStop;

typedef struct StationSpecList {
	const StationSpec *spec;
	uint32 grfid;      /// GRF ID of this custom station
	uint8  localidx;   /// Station ID within GRF of station
} StationSpecList;

struct Station {
	TileIndex xy;
	RoadStop *bus_stops;
	RoadStop *truck_stops;
	TileIndex train_tile;
	TileIndex airport_tile;
	TileIndex dock_tile;
	Town *town;
	uint16 string_id;

	ViewportSign sign;

	uint16 had_vehicle_of_type;

	byte time_since_load;
	byte time_since_unload;
	byte delete_ctr;
	PlayerID owner;
	byte facilities;
	byte airport_type;

	// trainstation width/height
	byte trainst_w, trainst_h;

	/** List of custom stations (StationSpecs) allocated to the station */
	uint8 num_specs;
	StationSpecList *speclist;

	Date build_date;

	//uint16 airport_flags;
	uint32 airport_flags;
	StationID index;

	byte last_vehicle_type;
	GoodsEntry goods[NUM_CARGO];

	uint16 random_bits;
	byte waiting_triggers;

	/* Stuff that is no longer used, but needed for conversion */
	TileIndex bus_tile_obsolete;
	TileIndex lorry_tile_obsolete;

	byte truck_stop_status_obsolete;
	byte bus_stop_status_obsolete;
	byte blocked_months_obsolete;
};

enum {
	FACIL_TRAIN      = 0x01,
	FACIL_TRUCK_STOP = 0x02,
	FACIL_BUS_STOP   = 0x04,
	FACIL_AIRPORT    = 0x08,
	FACIL_DOCK       = 0x10,
};

enum {
//	HVOT_PENDING_DELETE = 1 << 0, // not needed anymore
	HVOT_TRAIN    = 1 << 1,
	HVOT_BUS      = 1 << 2,
	HVOT_TRUCK    = 1 << 3,
	HVOT_AIRCRAFT = 1 << 4,
	HVOT_SHIP     = 1 << 5,
	/* This bit is used to mark stations. No, it does not belong here, but what
	 * can we do? ;-) */
	HVOT_BUOY     = 1 << 6
};

enum {
	CA_BUS             =  3,
	CA_TRUCK           =  3,
	CA_AIR_OILPAD      =  3,
	CA_TRAIN           =  4,
	CA_AIR_HELIPORT    =  4,
	CA_AIR_SMALL       =  4,
	CA_AIR_LARGE       =  5,
	CA_DOCK            =  5,
	CA_AIR_METRO       =  6,
	CA_AIR_INTER       =  8,
	CA_AIR_COMMUTER    =  4,
	CA_AIR_HELIDEPOT   =  4,
	CA_AIR_INTERCON    = 10,
	CA_AIR_HELISTATION =  4,
};

void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius);

void ShowStationViewWindow(StationID station);
void UpdateAllStationVirtCoord(void);

/* sorter stuff */
void RebuildStationLists(void);
void ResortStationLists(void);

extern MemoryPool _station_pool;

/**
 * Get the pointer to the station with index 'index'
 */
static inline Station *GetStation(StationID index)
{
	return (Station*)GetItemFromPool(&_station_pool, index);
}

/**
 * Get the current size of the StationPool
 */
static inline uint16 GetStationPoolSize(void)
{
	return _station_pool.total_items;
}

static inline StationID GetStationArraySize(void)
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index + 1. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetStationPoolSize();
}

/**
 * Check if a station really exists.
 */
static inline bool IsValidStation(const Station *st)
{
	return st->xy != 0;
}

static inline bool IsValidStationID(StationID index)
{
	return index < GetStationPoolSize() && IsValidStation(GetStation(index));
}

void DestroyStation(Station *st);

static inline void DeleteStation(Station *st)
{
	DestroyStation(st);
	st->xy = 0;
}

#define FOR_ALL_STATIONS_FROM(st, start) for (st = GetStation(start); st != NULL; st = (st->index + 1 < GetStationPoolSize()) ? GetStation(st->index + 1) : NULL) if (IsValidStation(st))
#define FOR_ALL_STATIONS(st) FOR_ALL_STATIONS_FROM(st, 0)


/* Stuff for ROADSTOPS */

extern MemoryPool _roadstop_pool;

/**
 * Get the pointer to the roadstop with index 'index'
 */
static inline RoadStop *GetRoadStop(uint index)
{
	return (RoadStop*)GetItemFromPool(&_roadstop_pool, index);
}

/**
 * Get the current size of the RoadStoptPool
 */
static inline uint16 GetRoadStopPoolSize(void)
{
	return _roadstop_pool.total_items;
}

/**
 * Check if a RaodStop really exists.
 */
static inline bool IsValidRoadStop(const RoadStop *rs)
{
	return rs->used;
}

void DestroyRoadStop(RoadStop* rs);

static inline void DeleteRoadStop(RoadStop *rs)
{
	DestroyRoadStop(rs);
	rs->used = false;
}

#define FOR_ALL_ROADSTOPS_FROM(rs, start) for (rs = GetRoadStop(start); rs != NULL; rs = (rs->index + 1 < GetRoadStopPoolSize()) ? GetRoadStop(rs->index + 1) : NULL) if (IsValidRoadStop(rs))
#define FOR_ALL_ROADSTOPS(rs) FOR_ALL_ROADSTOPS_FROM(rs, 0)

/* End of stuff for ROADSTOPS */


void AfterLoadStations(void);
void GetProductionAroundTiles(AcceptedCargo produced, TileIndex tile, int w, int h, int rad);
void GetAcceptanceAroundTiles(AcceptedCargo accepts, TileIndex tile, int w, int h, int rad);
uint GetStationPlatforms(const Station *st, TileIndex tile);
uint GetPlatformLength(TileIndex tile, DiagDirection dir);


const DrawTileSprites *GetStationTileLayout(byte gfx);
void StationPickerDrawSprite(int x, int y, RailType railtype, int image);

RoadStop * GetRoadStopByTile(TileIndex tile, RoadStopType type);
RoadStop * GetPrimaryRoadStop(const Station *st, RoadStopType type);
uint GetNumRoadStops(const Station* st, RoadStopType type);
RoadStop * AllocateRoadStop( void );
void ClearSlot(Vehicle *v);

static inline bool IsBuoy(const Station* st)
{
	return (st->had_vehicle_of_type & HVOT_BUOY) != 0; /* XXX: We should really ditch this ugly coding and switch to something sane... */
}

#endif /* STATION_H */
