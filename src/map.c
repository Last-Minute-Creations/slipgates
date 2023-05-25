/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "map.h"
#include <bartman/gcc8_c_support.h>
#include <ace/utils/file.h>
#include <ace/managers/system.h>
#include "game.h"

#define MAP_INTERACTION_TARGET_MAX 2

typedef struct tInteraction {
	tUbCoordYX pDoorTiles[MAP_INTERACTION_TARGET_MAX];
	UBYTE ubTargetCount;
	UBYTE isActive;
	UBYTE wasActive;
} tInteraction;

//----------------------------------------------------------------- PRIVATE VARS

static tInteraction s_sInteraction;

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
	// Reset all interactions
	s_sInteraction.isActive = 0;
	s_sInteraction.wasActive = 0;
	s_sInteraction.ubTargetCount = 0;

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

		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
				UWORD uwTileCode;
				fileRead(pFile, &uwTileCode, sizeof(uwTileCode));
				g_sCurrentLevel.pTiles[ubX][ubY] = uwTileCode;

				if(uwTileCode == TILE_GATE_1) {
					if(s_sInteraction.ubTargetCount >= MAP_INTERACTION_TARGET_MAX) {
						logWrite("ERR: Interaction target limit reached!");
					}
					else {
						s_sInteraction.pDoorTiles[s_sInteraction.ubTargetCount].ubX = ubX;
						s_sInteraction.pDoorTiles[s_sInteraction.ubTargetCount].ubY = ubY;
						++s_sInteraction.ubTargetCount;
					}
				}
			}
		}
		fileClose(pFile);
		systemUnuse();
	}
	g_pSlipgates[0].eNormal = DIRECTION_NONE;
	g_pSlipgates[1].eNormal = DIRECTION_NONE;
}

void mapProcess(void) {
	// Process effects of all interactions
	if(s_sInteraction.isActive) {
		if(!s_sInteraction.wasActive) {
			for(UBYTE i = 0; i < s_sInteraction.ubTargetCount; ++i) {
				g_sCurrentLevel.pTiles[s_sInteraction.pDoorTiles[i].ubX][s_sInteraction.pDoorTiles[i].ubY] = TILE_BG_1;
				gameDrawTile(s_sInteraction.pDoorTiles[i].ubX, s_sInteraction.pDoorTiles[i].ubY);
			}
			s_sInteraction.wasActive = 1;
		}

		// Reset interaction for refresh by body collisions
		s_sInteraction.isActive = 0;
	}
	else {
		if(s_sInteraction.wasActive) {
			for(UBYTE i = 0; i < s_sInteraction.ubTargetCount; ++i) {
				g_sCurrentLevel.pTiles[s_sInteraction.pDoorTiles[i].ubX][s_sInteraction.pDoorTiles[i].ubY] = TILE_GATE_1;
				gameDrawTile(s_sInteraction.pDoorTiles[i].ubX, s_sInteraction.pDoorTiles[i].ubY);
			}
			s_sInteraction.wasActive = 0;
		}
	}
}

void mapInteractAt(UNUSED_ARG UBYTE ubX, UNUSED_ARG UBYTE ubY) {
	s_sInteraction.isActive = 1;
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

UBYTE mapTileIsExit(tTile eTile) {
	return (eTile & MAP_TILE_MASK_EXIT) != 0;
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
