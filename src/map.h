/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_MAP_H
#define SLIPGATES_MAP_H

#include <fixmath/fix16.h>
#include "slipgate.h"
#include "interaction.h"

#define MAP_TILE_SHIFT 3
#define MAP_TILE_SIZE (1 << MAP_TILE_SHIFT)
#define MAP_TILE_WIDTH 40
#define MAP_TILE_HEIGHT 32
#define MAP_INTERACTIONS_MAX 4
#define MAP_BOXES_MAX 5
#define MAP_SPIKES_TILES_MAX 10

typedef struct tFix16Coord {
	fix16_t fX;
	fix16_t fY;
} tFix16Coord;

typedef struct tLevel {
	tFix16Coord sSpawnPos;
	tTile pTiles[MAP_TILE_WIDTH][MAP_TILE_HEIGHT]; // x,y
	tFix16Coord pBoxSpawns[MAP_BOXES_MAX];
	tUbCoordYX pSpikeTiles[MAP_SPIKES_TILES_MAX];
	UBYTE ubBoxCount;
	UBYTE ubBouncerSpawnerTileX;
	UBYTE ubBouncerSpawnerTileY;
	UBYTE ubSpikeTilesCount;
} tLevel;

extern tSlipgate g_pSlipgates[2];
extern tLevel g_sCurrentLevel;

void mapLoad(UBYTE ubIndex);

void mapSave(UBYTE ubIndex);

void mapProcess(void);

void mapPressButtonAt(UBYTE ubX, UBYTE ubY);

void mapPressButtonIndex(UBYTE ubButtonIndex);

void mapAddOrRemoveSpikeTile(UBYTE ubX, UBYTE ubY);

//----------------------------------------------------------------- INTERACTIONS

tInteraction *mapGetInteractionByIndex(UBYTE ubInteractionIndex);

tInteraction *mapGetInteractionByTile(UBYTE ubTileX, UBYTE ubTileY);

//----------------------------------------------------------------- MAP CHECKERS

tTile mapGetTileAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsEmptyAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsCollidingWithPortalProjectilesAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsCollidingWithBouncersAt(UBYTE ubTileX, UBYTE ubTileY);

UBYTE mapIsSlipgatableAt(UBYTE ubTileX, UBYTE ubTileY);

//---------------------------------------------------------------- TILE CHECKERS

UBYTE mapTileIsCollidingWithBoxes(tTile eTile);

UBYTE mapTileIsCollidingWithPortalProjectiles(tTile eTile);

UBYTE mapTileIsCollidingWithBouncers(tTile eTile);

UBYTE mapTileIsCollidingWithPlayers(tTile eTile);

UBYTE mapTileIsSlipgate(tTile eTile);

UBYTE mapTileIsLethal(tTile eTile);

UBYTE mapTileIsExit(tTile eTile);

UBYTE mapTileIsButton(tTile eTile);

//-------------------------------------------------------------------- SLIPGATES

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY);

void mapTryCloseSlipgateAt(UBYTE ubIndex, tUbCoordYX sPos);

#endif // SLIPGATES_MAP_H
