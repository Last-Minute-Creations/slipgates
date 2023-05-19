#ifndef SLIPGATES_MAP_H
#define SLIPGATES_MAP_H

#include "slipgate.h"

typedef enum tTile {
	TILE_BG_1 = 0,
	TILE_WALL_1 = 1,
	TILE_SLIPGATE_1 = 2,
	TILE_SLIPGATE_2 = 3,
} tTile;

extern tSlipgate g_pSlipgates[2];
extern UBYTE g_pTiles[20][16];

void mapInit(void);

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY);

#endif // SLIPGATES_MAP_H
