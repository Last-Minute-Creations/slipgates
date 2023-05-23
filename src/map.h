/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_MAP_H
#define SLIPGATES_MAP_H

#include "slipgate.h"
#include <fixmath/fix16.h>
#include <ace/macros.h>

#define MAP_TILE_SHIFT 3
#define MAP_TILE_SIZE (1 << MAP_TILE_SHIFT)
#define MAP_TILE_WIDTH 40
#define MAP_TILE_HEIGHT 32
#define MAP_TILE_MASK_SOLID_FOR_BODIES BV(7)
#define MAP_TILE_MASK_SOLID_FOR_PROJECTILES BV(6)
#define MAP_TILE_MASK_SLIPGATABLE BV(5)
#define MAP_TILE_INDEX_MASK ~(MAP_TILE_MASK_SOLID_FOR_BODIES | MAP_TILE_MASK_SOLID_FOR_PROJECTILES | MAP_TILE_MASK_SLIPGATABLE)

typedef enum tTile {
	TILE_BG_1               = 0,
	TILE_SLIPGATE_1         = 1 | MAP_TILE_MASK_SOLID_FOR_PROJECTILES | MAP_TILE_MASK_SLIPGATABLE,
	TILE_SLIPGATE_2         = 2 | MAP_TILE_MASK_SOLID_FOR_PROJECTILES | MAP_TILE_MASK_SLIPGATABLE,
	TILE_WALL_NO_SLIPGATE_1 = 3 | MAP_TILE_MASK_SOLID_FOR_BODIES | MAP_TILE_MASK_SOLID_FOR_PROJECTILES,
	TILE_WALL_1             = 4 | MAP_TILE_MASK_SOLID_FOR_BODIES | MAP_TILE_MASK_SOLID_FOR_PROJECTILES | MAP_TILE_MASK_SLIPGATABLE,
	TILE_FORCEFIELD_1       = 5 | MAP_TILE_MASK_SOLID_FOR_BODIES | MAP_TILE_MASK_SLIPGATABLE,
} tTile;

typedef struct tLevel {
	fix16_t fStartX;
	fix16_t fStartY;
	tTile pTiles[MAP_TILE_WIDTH][MAP_TILE_HEIGHT]; // x,y
} tLevel;

extern tSlipgate g_pSlipgates[2];
extern tLevel g_sCurrentLevel;

void mapLoad(UBYTE ubIndex);

UBYTE mapIsTileEmpty(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsTileSolidForProjectiles(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsTileSolidForBodies(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsTileSlipgatable(UBYTE ubTileX, UBYTE ubTileY);

tTile mapGetTileAt(UBYTE ubTileX, UBYTE ubTileY);

void mapCloseSlipgates(void);

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY);

#endif // SLIPGATES_MAP_H
