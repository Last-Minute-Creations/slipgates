/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "map.h"
#include <bartman/gcc8_c_support.h>
#include <ace/utils/file.h>
#include <ace/managers/system.h>
#include "game.h"

//----------------------------------------------------------------- PRIVATE VARS

static tInteraction s_pInteractions[MAP_INTERACTIONS_MAX];
static UBYTE s_ubButtonPressMask;
static UBYTE s_ubCurrentInteraction;

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
	memset(s_pInteractions, 0, sizeof(s_pInteractions));

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

		for(UBYTE ubInteractionIndex = 0; ubInteractionIndex < MAP_INTERACTIONS_MAX; ++ubInteractionIndex) {
			tInteraction *pInteraction = &s_pInteractions[ubInteractionIndex];
			fileRead(pFile, &pInteraction->ubTargetCount, sizeof(pInteraction->ubTargetCount));
			fileRead(pFile, &pInteraction->ubButtonMask, sizeof(pInteraction->ubButtonMask));
			fileRead(pFile, &pInteraction->wasActive, sizeof(pInteraction->wasActive));
			for(UBYTE ubTargetIndex = 0; ubTargetIndex < pInteraction->ubTargetCount; ++ubTargetIndex) {
				tUbCoordYX *pTileCoord = &pInteraction->pTargetTiles[ubTargetIndex];
				fileRead(pFile, &pTileCoord->uwYX, sizeof(pTileCoord->uwYX));
			}
		}

		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
				UWORD uwTileCode;
				fileRead(pFile, &uwTileCode, sizeof(uwTileCode));
				g_sCurrentLevel.pTiles[ubX][ubY] = uwTileCode;
			}
		}

		fileClose(pFile);
		systemUnuse();
	}

	g_pSlipgates[0].eNormal = DIRECTION_NONE;
	g_pSlipgates[1].eNormal = DIRECTION_NONE;
	s_ubCurrentInteraction = 0;
}

void mapProcess(void) {
	tInteraction *pInteraction = &s_pInteractions[s_ubCurrentInteraction];

	// Process effects of next interaction
	if(
		pInteraction->ubButtonMask &&
		(pInteraction->ubButtonMask & s_ubButtonPressMask) == pInteraction->ubButtonMask
	) {
		if(!pInteraction->wasActive) {
			for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
				g_sCurrentLevel.pTiles[pInteraction->pTargetTiles[i].ubX][pInteraction->pTargetTiles[i].ubY] = TILE_BG_1;
				gameDrawTile(pInteraction->pTargetTiles[i].ubX, pInteraction->pTargetTiles[i].ubY);
			}
			pInteraction->wasActive = 1;
		}
	}
	else {
		if(pInteraction->wasActive) {
			for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
				g_sCurrentLevel.pTiles[pInteraction->pTargetTiles[i].ubX][pInteraction->pTargetTiles[i].ubY] = TILE_GATE_1;
				gameDrawTile(pInteraction->pTargetTiles[i].ubX, pInteraction->pTargetTiles[i].ubY);
			}
			pInteraction->wasActive = 0;
		}
	}

	if(++s_ubCurrentInteraction >= MAP_INTERACTIONS_MAX) {
		s_ubCurrentInteraction = 0;
	}

	// Reset button mask for refresh by body collisions
	s_ubButtonPressMask = 0;
}

void mapPressButtonAt(UBYTE ubX, UBYTE ubY) {
	tTile eTile = g_sCurrentLevel.pTiles[ubX][ubY];
	if(!mapTileIsButton(eTile)) {
		logWrite("ERR: Tile %d at %hhu,%hhu is not a button!\n", eTile, ubX, ubY);
	}
	s_ubButtonPressMask |= BV(eTile - TILE_BUTTON_1);
}

//----------------------------------------------------------------- INTERACTIONS

tInteraction *mapGetInteractionByIndex(UBYTE ubInteractionIndex) {
	return &s_pInteractions[ubInteractionIndex];
}

tInteraction *mapGetInteractionByTile(UBYTE ubTileX, UBYTE ubTileY) {
	for(UBYTE i = 0; i < MAP_INTERACTIONS_MAX; ++ i) {
		UBYTE ubTileIndex = interactionGetTileIndex(
			&s_pInteractions[i], ubTileX, ubTileY
		);
		if(ubTileIndex != INTERACTION_TILE_INDEX_INVALID) {
			return &s_pInteractions[i];
		}
	}
	return 0;
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

UBYTE mapTileIsButton(tTile eTile) {
	return (eTile & MAP_TILE_MASK_BUTTON) != 0;
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
