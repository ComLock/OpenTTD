/* $Id$ */

#ifndef DIRECTION_H
#define DIRECTION_H

/* Direction as commonly used in v->direction, 8 way. */
typedef enum Direction {
	DIR_N   = 0,
	DIR_NE  = 1,      /* Northeast, upper right on your monitor */
	DIR_E   = 2,
	DIR_SE  = 3,
	DIR_S   = 4,
	DIR_SW  = 5,
	DIR_W   = 6,
	DIR_NW  = 7,
	DIR_END,
	INVALID_DIR = 0xFF,
} Direction;

static inline Direction ReverseDir(Direction d)
{
	return (Direction)(4 ^ d);
}


typedef enum DirDiff {
	DIRDIFF_SAME    = 0,
	DIRDIFF_45RIGHT = 1,
	DIRDIFF_90RIGHT = 2,
	DIRDIFF_REVERSE = 4,
	DIRDIFF_90LEFT  = 6,
	DIRDIFF_45LEFT  = 7
} DirDiff;

static inline DirDiff DirDifference(Direction d0, Direction d1)
{
	return (DirDiff)((d0 + 8 - d1) % 8);
}

static inline DirDiff ChangeDirDiff(DirDiff d, DirDiff delta)
{
	return (DirDiff)((d + delta) % 8);
}


static inline Direction ChangeDir(Direction d, DirDiff delta)
{
	return (Direction)((d + delta) % 8);
}


/* Direction commonly used as the direction of entering and leaving tiles, 4-way */
typedef enum DiagDirection {
	DIAGDIR_NE  = 0,      /* Northeast, upper right on your monitor */
	DIAGDIR_SE  = 1,
	DIAGDIR_SW  = 2,
	DIAGDIR_NW  = 3,
	DIAGDIR_END,
	INVALID_DIAGDIR = 0xFF,
} DiagDirection;

static inline DiagDirection ReverseDiagDir(DiagDirection d)
{
	return (DiagDirection)(2 ^ d);
}


static inline DiagDirection DirToDiagDir(Direction dir)
{
	return (DiagDirection)(dir >> 1);
}


static inline Direction DiagDirToDir(DiagDirection dir)
{
	return (Direction)(dir * 2 + 1);
}


/* the 2 axis */
typedef enum Axis {
	AXIS_X = 0,
	AXIS_Y = 1
} Axis;


static inline Axis DiagDirToAxis(DiagDirection d)
{
	return (Axis)(d & 1);
}

#endif
