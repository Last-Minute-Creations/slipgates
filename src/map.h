/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_MAP_H
#define SLIPGATES_MAP_H

#include <fixmath/fix16.h>
#include <ace/macros.h>
#include "slipgate.h"
#include "interaction.h"

#define MAP_TILE_SHIFT 3
#define MAP_TILE_SIZE (1 << MAP_TILE_SHIFT)
#define MAP_TILE_WIDTH 40
#define MAP_TILE_HEIGHT 32
#define MAP_INTERACTIONS_MAX 4
#define MAP_BOXES_MAX 5
#define MAP_LAYER_SOLID_FOR_BODIES BV(15)
#define MAP_LAYER_SOLID_FOR_PROJECTILES BV(14)
#define MAP_LAYER_SLIPGATABLE BV(13)
#define MAP_LAYER_LETHAL BV(12)
#define MAP_LAYER_EXIT BV(11)
#define MAP_LAYER_BUTTON BV(10)
#define MAP_TILE_INDEX_MASK ~( \
	MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | \
	MAP_LAYER_SLIPGATABLE | MAP_LAYER_LETHAL | MAP_LAYER_EXIT | \
	MAP_LAYER_BUTTON \
)

typedef enum tTile {
	TILE_BG_1               = 0,
	TILE_SLIPGATE_1         = 1  | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_SLIPGATABLE,
	TILE_SLIPGATE_2         = 2  | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_SLIPGATABLE,
	TILE_WALL_NO_SLIPGATE_1 = 3  | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES,
	TILE_WALL_1             = 4  | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_SLIPGATABLE,
	TILE_FORCE_FIELD_1      = 5  | MAP_LAYER_SOLID_FOR_BODIES,
	TILE_DEATH_FIELD_1      = 6  | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_LETHAL,
	TILE_EXIT_1             = 7  | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_EXIT,
	TILE_BUTTON_1           = 8  | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_2           = 9  | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_3           = 10 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_4           = 11 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_5           = 12 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_6           = 13 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_7           = 14 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_BUTTON_8           = 15 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES | MAP_LAYER_BUTTON,
	TILE_GATE_1             = 16 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES,
	TILE_RECEIVER           = 17 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES,
	TILE_BOUNCER_SPAWNER    = 18 | MAP_LAYER_SOLID_FOR_BODIES | MAP_LAYER_SOLID_FOR_PROJECTILES,
} tTile;

typedef struct tFix16Coord {
	fix16_t fX;
	fix16_t fY;
} tFix16Coord;

typedef struct tLevel {
	tFix16Coord sSpawnPos;
	tTile pTiles[MAP_TILE_WIDTH][MAP_TILE_HEIGHT]; // x,y
	tFix16Coord pBoxSpawns[MAP_BOXES_MAX];
	UBYTE ubBoxCount;
	UBYTE ubBouncerSpawnerTileX;
	UBYTE ubBouncerSpawnerTileY;
} tLevel;

extern tSlipgate g_pSlipgates[2];
extern tLevel g_sCurrentLevel;

void mapLoad(UBYTE ubIndex);

void mapSave(UBYTE ubIndex);

void mapProcess(void);

void mapPressButtonAt(UBYTE ubX, UBYTE ubY);

void mapPressButtonIndex(UBYTE ubButtonIndex);

//----------------------------------------------------------------- INTERACTIONS

tInteraction *mapGetInteractionByIndex(UBYTE ubInteractionIndex);

tInteraction *mapGetInteractionByTile(UBYTE ubTileX, UBYTE ubTileY);

//----------------------------------------------------------------- MAP CHECKERS

tTile mapGetTileAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsEmptyAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsSolidForProjectilesAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsSolidForBodiesAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsSlipgatableAt(UBYTE ubTileX, UBYTE ubTileY);

//---------------------------------------------------------------- TILE CHECKERS

UBYTE mapTileIsSolidForBodies(tTile eTile);

UBYTE mapTileIsLethal(tTile eTile);

UBYTE mapTileIsExit(tTile eTile);

UBYTE mapTileIsButton(tTile eTile);

//-------------------------------------------------------------------- SLIPGATES

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY);

#endif // SLIPGATES_MAP_H
