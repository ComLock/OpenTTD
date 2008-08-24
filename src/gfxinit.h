/* $Id$ */

/** @file gfxinit.h Functions related to the graphics initialization. */

#ifndef GFXINIT_H
#define GFXINIT_H

#include "gfx_type.h"

void CheckExternalFiles();
void GfxLoadSprites();
void LoadSpritesIndexed(int file_index, uint *sprite_id, const SpriteID *index_tbl);

void FindGraphicsSets();
bool SetGraphicsSet(const char *name);
char *GetGraphicsSetsList(char *p, const char *last);

#endif /* GFXINIT_H */
