/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "macros.h"
#include "pool.h"
#include "newgrf_spritegroup.h"

enum {
	SPRITEGROUP_POOL_BLOCK_SIZE_BITS = 4, /* (1 << 4) == 16 items */
	SPRITEGROUP_POOL_MAX_BLOCKS      = 8000,
};

static uint _spritegroup_count = 0;
static MemoryPool _spritegroup_pool;


static void SpriteGroupPoolCleanBlock(uint start_item, uint end_item)
{
	uint i;

	for (i = start_item; i <= end_item; i++) {
		SpriteGroup *group = (SpriteGroup*)GetItemFromPool(&_spritegroup_pool, i);

		/* Free dynamically allocated memory */
		switch (group->type) {
			case SGT_REAL:
				free(group->g.real.loaded);
				free(group->g.real.loading);
				break;

			case SGT_DETERMINISTIC:
				free(group->g.determ.adjusts);
				free(group->g.determ.ranges);
				break;

			case SGT_RANDOMIZED:
				free(group->g.random.groups);
				break;

			default:
				break;
		}
	}
}


/* Initialize the SpriteGroup pool */
static MemoryPool _spritegroup_pool = { "SpriteGr", SPRITEGROUP_POOL_MAX_BLOCKS, SPRITEGROUP_POOL_BLOCK_SIZE_BITS, sizeof(SpriteGroup), NULL, &SpriteGroupPoolCleanBlock, 0, 0, NULL };


/* Allocate a new SpriteGroup */
SpriteGroup *AllocateSpriteGroup(void)
{
	/* This is totally different to the other pool allocators, as we never remove an item from the pool. */
	if (_spritegroup_count == _spritegroup_pool.total_items) {
		if (!AddBlockToPool(&_spritegroup_pool)) return NULL;
	}

	return (SpriteGroup*)GetItemFromPool(&_spritegroup_pool, _spritegroup_count++);
}


void InitializeSpriteGroupPool(void)
{
	CleanPool(&_spritegroup_pool);
	AddBlockToPool(&_spritegroup_pool);

	_spritegroup_count = 0;
}


static inline uint32 GetVariable(const ResolverObject *object, byte variable, byte parameter)
{
	/* Return common variables */
	switch (variable) {
		case 0x00: return _date;
		case 0x01: return _cur_year;
		case 0x02: return _cur_month;
		case 0x03: return _opt.landscape;
		case 0x09: return _date_fract;
		case 0x0A: return _tick_counter;
		case 0x0C: return object->callback;
		case 0x10: return object->callback_param1;
		case 0x11: return 0;
		case 0x18: return object->callback_param2;
		case 0x1A: return -1;
		case 0x1B: return GB(_display_opt, 0, 6);
		case 0x1C: return object->last_value;
		case 0x20: return _opt.landscape == LT_HILLY ? _opt.snow_line : 0xFF;

		/* Not a common variable, so evalute the feature specific variables */
		default: return object->GetVariable(object, variable, parameter);
	}
}


/* Evaluate an adjustment for a variable of the given size. This is a bit of
 * an unwieldy macro, but it saves triplicating the code. */
#define BUILD_EVAL_ADJUST(size, usize) \
static inline usize EvalAdjust_ ## size(const DeterministicSpriteGroupAdjust *adjust, usize last_value, int32 value) \
{ \
	value >>= adjust->shift_num; \
	value  &= adjust->and_mask; \
\
	if (adjust->type != DSGA_TYPE_NONE) value += (size)adjust->add_val; \
\
	switch (adjust->type) { \
		case DSGA_TYPE_DIV:  value /= (size)adjust->divmod_val; break; \
		case DSGA_TYPE_MOD:  value %= (usize)adjust->divmod_val; break; \
		case DSGA_TYPE_NONE: break; \
	} \
\
	/* Get our value to the correct range */ \
	value = (usize)value; \
\
	switch (adjust->operation) { \
		case DSGA_OP_ADD:  return last_value + value; \
		case DSGA_OP_SUB:  return last_value - value; \
		case DSGA_OP_SMIN: return min(last_value, value); \
		case DSGA_OP_SMAX: return max(last_value, value); \
		case DSGA_OP_UMIN: return min((usize)last_value, (usize)value); \
		case DSGA_OP_UMAX: return max((usize)last_value, (usize)value); \
		case DSGA_OP_SDIV: return last_value / value; \
		case DSGA_OP_SMOD: return last_value % value; \
		case DSGA_OP_UDIV: return (usize)last_value / (usize)value; \
		case DSGA_OP_UMOD: return (usize)last_value % (usize)value; \
		case DSGA_OP_MUL:  return last_value * value; \
		case DSGA_OP_AND:  return last_value & value; \
		case DSGA_OP_OR:   return last_value | value; \
		case DSGA_OP_XOR:  return last_value ^ value; \
		default:           return value; \
	} \
}


BUILD_EVAL_ADJUST(int8, uint8)
BUILD_EVAL_ADJUST(int16, uint16)
BUILD_EVAL_ADJUST(int32, uint32)


static inline const SpriteGroup *ResolveVariable(const SpriteGroup *group, ResolverObject *object)
{
	static SpriteGroup nvarzero;
	const SpriteGroup *target;
	int32 last_value = object->last_value;
	int32 value = -1;
	uint i;

	object->scope = group->g.determ.var_scope;

	for (i = 0; i < group->g.determ.num_adjusts; i++) {
		DeterministicSpriteGroupAdjust *adjust = &group->g.determ.adjusts[i];
		value = GetVariable(object, adjust->variable, adjust->parameter);
		switch (group->g.determ.size) {
			case DSG_SIZE_BYTE:  value = EvalAdjust_int8(adjust, last_value, value); break;
			case DSG_SIZE_WORD:  value = EvalAdjust_int16(adjust, last_value, value); break;
			case DSG_SIZE_DWORD: value = EvalAdjust_int32(adjust, last_value, value); break;
			default: NOT_REACHED(); break;
		}
		last_value = value;
	}

	if (value == -1) {
		/* Unsupported property */
		target = group->g.determ.num_ranges > 0 ? group->g.determ.ranges[0].group : group->g.determ.default_group;
	} else {
		if (group->g.determ.num_ranges == 0) {
			/* nvar == 0 is a special case -- we turn our value into a callback result */
			nvarzero.type = SGT_CALLBACK;
			nvarzero.g.callback.result = GB(value, 0, 15) | 0x8000;
			return &nvarzero;
		}

		target = group->g.determ.default_group;
		for (i = 0; i < group->g.determ.num_ranges; i++) {
			if (group->g.determ.ranges[i].low <= (uint32)value && (uint32)value <= group->g.determ.ranges[i].high) {
				target = group->g.determ.ranges[i].group;
				break;
			}
		}
	}

	return Resolve(target, object);
}


static inline const SpriteGroup *ResolveRandom(const SpriteGroup *group, ResolverObject *object)
{
	uint32 mask;
	byte index;

	object->scope = group->g.random.var_scope;

	if (object->trigger != 0) {
		/* Handle triggers */
		/* Magic code that may or may not do the right things... */
		byte waiting_triggers = object->GetTriggers(object);
		byte match = group->g.random.triggers & (waiting_triggers | object->trigger);
		bool res;

		res = (group->g.random.cmp_mode == RSG_CMP_ANY) ?
			(match != 0) : (match == group->g.random.triggers);

		if (res) {
			waiting_triggers &= ~match;
			object->reseed |= (group->g.random.num_groups - 1) << group->g.random.lowest_randbit;
		} else {
			waiting_triggers |= object->trigger;
		}

		object->SetTriggers(object, waiting_triggers);
	}

	mask  = (group->g.random.num_groups - 1) << group->g.random.lowest_randbit;
	index = (object->GetRandomBits(object) & mask) >> group->g.random.lowest_randbit;

	return Resolve(group->g.random.groups[index], object);
}


/* ResolverObject (re)entry point */
const SpriteGroup *Resolve(const SpriteGroup *group, ResolverObject *object)
{
	/* We're called even if there is no group, so quietly return nothing */
	if (group == NULL) return NULL;

	switch (group->type) {
		case SGT_REAL:          return object->ResolveReal(object, group);
		case SGT_DETERMINISTIC: return ResolveVariable(group, object);
		case SGT_RANDOMIZED:    return ResolveRandom(group, object);
		default:                return group;
	}
}

