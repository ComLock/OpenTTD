/* $Id$ */

#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "pool.h"
#include "rail_map.h"

struct Waypoint {
	TileIndex xy;      ///< Tile of waypoint
	StationID index;   ///< Index of waypoint

	TownID town_index; ///< Town associated with the waypoint
	byte town_cn;      ///< The Nth waypoint for this town (consecutive number)
	StringID string;   ///< If this is zero (i.e. no custom name), town + town_cn is used for naming

	ViewportSign sign; ///< Dimensions of sign (not saved)
	Date build_date;   ///< Date of construction

	byte stat_id;      ///< ID of waypoint within the waypoint class (not saved)
	uint32 grfid;      ///< ID of GRF file
	byte localidx;     ///< Index of station within GRF file

	byte deleted;      ///< Delete counter. If greater than 0 then it is decremented until it reaches 0; the waypoint is then is deleted.
};

extern MemoryPool _waypoint_pool;

/**
 * Get the pointer to the waypoint with index 'index'
 */
static inline Waypoint *GetWaypoint(uint index)
{
	return (Waypoint*)GetItemFromPool(&_waypoint_pool, index);
}

/**
 * Get the current size of the WaypointPool
 */
static inline uint16 GetWaypointPoolSize(void)
{
	return _waypoint_pool.total_items;
}

/**
 * Check if a Waypoint really exists.
 */
static inline bool IsValidWaypoint(const Waypoint *wp)
{
	return wp->xy != 0;
}

static inline bool IsValidWaypointID(uint index)
{
	return index < GetWaypointPoolSize() && IsValidWaypoint(GetWaypoint(index));
}

#define FOR_ALL_WAYPOINTS_FROM(wp, start) for (wp = GetWaypoint(start); wp != NULL; wp = (wp->index + 1 < GetWaypointPoolSize()) ? GetWaypoint(wp->index + 1) : NULL) if (IsValidWaypoint(wp))
#define FOR_ALL_WAYPOINTS(wp) FOR_ALL_WAYPOINTS_FROM(wp, 0)


/**
 * Fetch a waypoint by tile
 * @param tile Tile of waypoint
 * @return Waypoint
 */
static inline Waypoint *GetWaypointByTile(TileIndex tile)
{
	assert(IsTileType(tile, MP_RAILWAY) && IsRailWaypoint(tile));
	return GetWaypoint(_m[tile].m2);
}

int32 RemoveTrainWaypoint(TileIndex tile, uint32 flags, bool justremove);
Station *ComposeWaypointStation(TileIndex tile);
void ShowRenameWaypointWindow(const Waypoint *cp);
void DrawWaypointSprite(int x, int y, int image, RailType railtype);
void FixOldWaypoints(void);
void UpdateAllWaypointSigns(void);
void UpdateAllWaypointCustomGraphics(void);

#endif /* WAYPOINT_H */
