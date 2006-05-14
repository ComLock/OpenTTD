/* $Id$ */

#ifndef NEWGRF_ENGINE_H
#define NEWGRF_ENGINE_H

#include "direction.h"

/** @file newgrf_engine.h
 */

extern int _traininfo_vehicle_pitch;
extern int _traininfo_vehicle_width;

VARDEF const uint32 _default_refitmasks[NUM_VEHICLE_TYPES];
VARDEF const CargoID _global_cargo_id[NUM_LANDSCAPE][NUM_CARGO];
VARDEF const uint32 _landscape_global_cargo_mask[NUM_LANDSCAPE];
VARDEF const CargoID _local_cargo_id_ctype[NUM_GLOBAL_CID];
VARDEF const uint32 cargo_classes[16];

void SetWagonOverrideSprites(EngineID engine, const struct SpriteGroup *group, byte *train_id, int trains);
void SetCustomEngineSprites(EngineID engine, byte cargo, const struct SpriteGroup *group);
void SetRotorOverrideSprites(EngineID engine, const struct SpriteGroup *group);
SpriteID GetCustomEngineSprite(EngineID engine, const Vehicle* v, Direction direction);
SpriteID GetRotorOverrideSprite(EngineID engine, const Vehicle* v);
#define GetCustomRotorSprite(v) GetRotorOverrideSprite(v->engine_type, v)
#define GetCustomRotorIcon(et) GetRotorOverrideSprite(et, NULL)

void SetEngineGRF(EngineID engine, uint32 grfid);
uint32 GetEngineGRFID(EngineID engine);

uint16 GetVehicleCallback(uint16 callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v);
uint16 GetVehicleCallbackParent(uint16 callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v, const Vehicle *parent);
bool UsesWagonOverride(const Vehicle *v);
#define GetCustomVehicleSprite(v, direction) GetCustomEngineSprite(v->engine_type, v, direction)
#define GetCustomVehicleIcon(et, direction) GetCustomEngineSprite(et, NULL, direction)

typedef enum VehicleTrigger {
	VEHICLE_TRIGGER_NEW_CARGO = 1,
	// Externally triggered only for the first vehicle in chain
	VEHICLE_TRIGGER_DEPOT = 2,
	// Externally triggered only for the first vehicle in chain, only if whole chain is empty
	VEHICLE_TRIGGER_EMPTY = 4,
	// Not triggered externally (called for the whole chain if we got NEW_CARGO)
	VEHICLE_TRIGGER_ANY_NEW_CARGO = 8,
} VehicleTrigger;
void TriggerVehicle(Vehicle *veh, VehicleTrigger trigger);

void SetCustomEngineName(EngineID engine, StringID name);
StringID GetCustomEngineName(EngineID engine);

void UnloadWagonOverrides(void);
void UnloadRotorOverrideSprites(void);
void UnloadCustomEngineSprites(void);
void UnloadCustomEngineNames(void);

void ResetEngineListOrder(void);
EngineID GetRailVehAtPosition(EngineID pos);
void AlterRailVehListOrder(EngineID engine, EngineID target);

#endif /* NEWGRF_ENGINE_H */
