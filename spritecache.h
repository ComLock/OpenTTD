#ifndef SPRITECACHE_H
#define SPRITECACHE_H

typedef struct Sprite {
	byte info;
	byte height;
	uint16 width;
	int16 x_offs;
	int16 y_offs;
	byte data[VARARRAY_SIZE];
} Sprite;
assert_compile(sizeof(Sprite) == 8);

typedef struct {
	int xoffs, yoffs;
	int xsize, ysize;
} SpriteDimension;

const SpriteDimension *GetSpriteDimension(SpriteID sprite);
Sprite *GetSprite(SpriteID sprite);
byte *GetNonSprite(SpriteID sprite);

void GfxLoadSprites(void);
void IncreaseSpriteLRU(void);

#endif
