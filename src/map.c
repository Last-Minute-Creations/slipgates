/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "map.h"
#include <bartman/gcc8_c_support.h>
#include <ace/utils/file.h>
#include <ace/managers/system.h>

//----------------------------------------------------------------- PRIVATE VARS

//------------------------------------------------------------ PRIVATE FUNCTIONS

static void setSlipgateTiles(const tSlipgate *pSlipgate, tTile eTile) {
	g_sCurrentLevel.pTiles[pSlipgate->uwTileX][pSlipgate->uwTileY] = eTile;
	switch(pSlipgate->eNormal) {
		case DIRECTION_UP:
		case DIRECTION_DOWN:
			g_sCurrentLevel.pTiles[pSlipgate->uwTileX + 1][pSlipgate->uwTileY] = eTile;
			break;
		case DIRECTION_LEFT:
		case DIRECTION_RIGHT:
			g_sCurrentLevel.pTiles[pSlipgate->uwTileX][pSlipgate->uwTileY + 1] = eTile;
			break;
		case DIRECTION_NONE:
			break;
	}
}

//------------------------------------------------------------- PUBLIC FUNCTIONS

void mapLoad(UBYTE ubIndex) {
	if(ubIndex == 0) {
		// hadcoded level
		memset(&g_sCurrentLevel, 0, sizeof(g_sCurrentLevel));
		for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
			g_sCurrentLevel.pTiles[ubX][0] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[ubX][1] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[ubX][MAP_TILE_HEIGHT - 2] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[ubX][MAP_TILE_HEIGHT - 1] = TILE_WALL_1;
		}
		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			g_sCurrentLevel.pTiles[0][ubY] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[1][ubY] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[MAP_TILE_WIDTH - 2][ubY] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[MAP_TILE_WIDTH - 1][ubY] = TILE_WALL_1;
		}
		g_sCurrentLevel.fStartX = fix16_from_int(100);
		g_sCurrentLevel.fStartY = fix16_from_int(100);
	}
	else {
		char szName[13];
		sprintf(szName, "level%03hhu.dat", ubIndex);
		systemUse();
		tFile *pFile = fileOpen(szName, "rb");
		fileRead(pFile, &g_sCurrentLevel.fStartX, sizeof(g_sCurrentLevel.fStartX));
		fileRead(pFile, &g_sCurrentLevel.fStartY, sizeof(g_sCurrentLevel.fStartY));

		for(UBYTE y = 0; y < MAP_TILE_HEIGHT; ++y) {
			for(UBYTE x = 0; x < MAP_TILE_WIDTH; ++x) {
				UWORD uwTileCode;
				fileRead(pFile, &uwTileCode, sizeof(uwTileCode));
				g_sCurrentLevel.pTiles[x][y] = uwTileCode;
			}
		}
		fileClose(pFile);
		systemUnuse();
	}
	g_pSlipgates[0].eNormal = DIRECTION_NONE;
	g_pSlipgates[1].eNormal = DIRECTION_NONE;
}

//----------------------------------------------------------------- MAP CHECKERS

tTile mapGetTileAt(UBYTE ubTileX, UBYTE ubTileY) {
	return g_sCurrentLevel.pTiles[ubTileX][ubTileY];
}

UBYTE mapIsTileEmptyAt(UBYTE ubTileX, UBYTE ubTileY) {
	return g_sCurrentLevel.pTiles[ubTileX][ubTileY] == TILE_BG_1;
}

UBYTE mapIsTileSolidForBodiesAt(UBYTE ubTileX, UBYTE ubTileY) {
	return mapTileIsSolidForBodies(g_sCurrentLevel.pTiles[ubTileX][ubTileY]);
}

UBYTE mapIsTileSolidForProjectilesAt(UBYTE ubTileX, UBYTE ubTileY) {
	return (g_sCurrentLevel.pTiles[ubTileX][ubTileY] & MAP_TILE_MASK_SOLID_FOR_PROJECTILES) != 0;
}

UBYTE mapIsTileSlipgatableAt(UBYTE ubTileX, UBYTE ubTileY) {
	return (g_sCurrentLevel.pTiles[ubTileX][ubTileY] & MAP_TILE_MASK_SLIPGATABLE) != 0;
}

//---------------------------------------------------------------- TILE CHECKERS

UBYTE mapTileIsSolidForBodies(tTile eTile) {
	return (eTile & MAP_TILE_MASK_SOLID_FOR_BODIES) != 0;
}

UBYTE mapTileIsLethal(tTile eTile) {
	return (eTile & MAP_TILE_MASK_LETHAL) != 0;
}

//-------------------------------------------------------------------- SLIPGATES

void mapCloseSlipgates(void) {
	if(g_pSlipgates[0].eNormal != DIRECTION_NONE) {
		setSlipgateTiles(&g_pSlipgates[0], TILE_WALL_1);
		g_pSlipgates[0].eNormal = DIRECTION_NONE;
	}
	if(g_pSlipgates[1].eNormal != DIRECTION_NONE) {
		setSlipgateTiles(&g_pSlipgates[1], TILE_WALL_1);
		g_pSlipgates[1].eNormal = DIRECTION_NONE;
	}
}

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY) {
	if(!mapIsTileSlipgatableAt(ubTileX, ubTileY)) {
		return 0;
	}

	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	if(pSlipgate->eNormal != DIRECTION_NONE) {
		setSlipgateTiles(pSlipgate, TILE_WALL_1);
	}

	if(
		mapIsTileEmptyAt(ubTileX - 1, ubTileY) &&
		mapIsTileSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsTileEmptyAt(ubTileX - 1, ubTileY + 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY + 1;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_LEFT;
	}
	else if(
		mapIsTileEmptyAt(ubTileX + 1, ubTileY) &&
		mapIsTileSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsTileEmptyAt(ubTileX + 1, ubTileY + 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY + 1;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_RIGHT;
	}
	else if(
		mapIsTileEmptyAt(ubTileX, ubTileY - 1) &&
		mapIsTileSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsTileEmptyAt(ubTileX + 1, ubTileY - 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX + 1;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_UP;
	}
	else if(
		mapIsTileEmptyAt(ubTileX, ubTileY + 1) &&
		mapIsTileSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsTileEmptyAt(ubTileX + 1, ubTileY + 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX + 1;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_DOWN;
	}

	setSlipgateTiles(pSlipgate, ubIndex == 0 ? TILE_SLIPGATE_1 : TILE_SLIPGATE_2);
	return 1;
}

//------------------------------------------------------------------ GLOBAL VARS

tSlipgate g_pSlipgates[2] = {
	{.uwTileX = 7, .uwTileY = 15, .eNormal = DIRECTION_NONE},
	{.uwTileX = 11, .uwTileY = 15, .eNormal = DIRECTION_NONE},
};

tLevel g_sCurrentLevel;
