#ifndef SLIPGATES_MAP_H
#define SLIPGATES_MAP_H

#include "slipgate.h"
#include <fixmath/fix16.h>

#define MAP_TILE_SHIFT 3
#define MAP_TILE_SIZE (1 << MAP_TILE_SHIFT)
#define MAP_TILE_WIDTH 40
#define MAP_TILE_HEIGHT 32

typedef enum tTile {
	TILE_BG_1 = 0,
	TILE_WALL_1 = 1,
	TILE_SLIPGATE_1 = 2,
	TILE_SLIPGATE_2 = 3,
} tTile;

typedef struct tLevel {
	fix16_t fStartX;
	fix16_t fStartY;
	tTile pTiles[MAP_TILE_WIDTH][MAP_TILE_HEIGHT]; // x,y
} tLevel;

extern tSlipgate g_pSlipgates[2];
extern tLevel g_sCurrentLevel;

void mapLoad(UBYTE ubIndex);

UBYTE mapIsTileSolid(UBYTE ubTileX, UBYTE ubTileY);

tTile mapGetTileAt(UBYTE ubTileX, UBYTE ubTileY);

void mapCloseSlipgates(void);

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY);

#endif // SLIPGATES_MAP_H
