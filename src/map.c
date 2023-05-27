/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "map.h"
#include <bartman/gcc8_c_support.h>
#include <ace/utils/file.h>
#include <ace/managers/system.h>
#include "game.h"
#include "bouncer.h"

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

static void mapCloseSlipgates(void) {
	if(g_pSlipgates[0].eNormal != DIRECTION_NONE) {
		setSlipgateTiles(&g_pSlipgates[0], TILE_WALL_1);
		g_pSlipgates[0].eNormal = DIRECTION_NONE;
	}
	if(g_pSlipgates[1].eNormal != DIRECTION_NONE) {
		setSlipgateTiles(&g_pSlipgates[1], TILE_WALL_1);
		g_pSlipgates[1].eNormal = DIRECTION_NONE;
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
		g_sCurrentLevel.sSpawnPos.fX = fix16_from_int(100);
		g_sCurrentLevel.sSpawnPos.fY = fix16_from_int(100);
	}
	else {
		char szName[13];
		sprintf(szName, "level%03hhu.dat", ubIndex);
		systemUse();
		tFile *pFile = fileOpen(szName, "rb");
		fileRead(pFile, &g_sCurrentLevel.sSpawnPos.fX, sizeof(g_sCurrentLevel.sSpawnPos.fX));
		fileRead(pFile, &g_sCurrentLevel.sSpawnPos.fY, sizeof(g_sCurrentLevel.sSpawnPos.fY));

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

		fileRead(pFile, &g_sCurrentLevel.ubBoxCount, sizeof(g_sCurrentLevel.ubBoxCount));
		for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
			fileRead(pFile, &g_sCurrentLevel.pBoxSpawns[i].fX, sizeof(g_sCurrentLevel.pBoxSpawns[i].fX));
			fileRead(pFile, &g_sCurrentLevel.pBoxSpawns[i].fY, sizeof(g_sCurrentLevel.pBoxSpawns[i].fY));
		}

		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
				UWORD uwTileCode;
				fileRead(pFile, &uwTileCode, sizeof(uwTileCode));
				g_sCurrentLevel.pTiles[ubX][ubY] = uwTileCode;
				if(uwTileCode == TILE_BOUNCER_SPAWNER) {
					if(g_sCurrentLevel.ubBouncerSpawnerTileX != BOUNCER_TILE_INVALID) {
						logWrite("ERR: More than one bouncer spawner on map\n");
					}
					g_sCurrentLevel.ubBouncerSpawnerTileX = ubX;
					g_sCurrentLevel.ubBouncerSpawnerTileY = ubY;
				}
			}
		}

		fileClose(pFile);
		systemUnuse();
	}

	g_pSlipgates[0].eNormal = DIRECTION_NONE;
	g_pSlipgates[1].eNormal = DIRECTION_NONE;
	s_ubCurrentInteraction = 0;
}

void mapSave(UBYTE ubIndex) {
	mapCloseSlipgates();

	char szName[13];
	sprintf(szName, "level%03hhu.dat", ubIndex);
	systemUse();
	tFile *pFile = fileOpen(szName, "wb");
	fileWrite(pFile, &g_sCurrentLevel.sSpawnPos.fX, sizeof(g_sCurrentLevel.sSpawnPos.fX));
	fileWrite(pFile, &g_sCurrentLevel.sSpawnPos.fY, sizeof(g_sCurrentLevel.sSpawnPos.fY));

	for(UBYTE ubInteractionIndex = 0; ubInteractionIndex < MAP_INTERACTIONS_MAX; ++ubInteractionIndex) {
		tInteraction *pInteraction = mapGetInteractionByIndex(ubInteractionIndex);
		fileWrite(pFile, &pInteraction->ubTargetCount, sizeof(pInteraction->ubTargetCount));
		fileWrite(pFile, &pInteraction->ubButtonMask, sizeof(pInteraction->ubButtonMask));
		fileWrite(pFile, &pInteraction->wasActive, sizeof(pInteraction->wasActive));
		for(UBYTE ubTargetIndex = 0; ubTargetIndex < pInteraction->ubTargetCount; ++ubTargetIndex) {
			tUbCoordYX *pTileCoord = &pInteraction->pTargetTiles[ubTargetIndex];
			fileWrite(pFile, &pTileCoord->uwYX, sizeof(pTileCoord->uwYX));
		}
	}

	fileWrite(pFile, &g_sCurrentLevel.ubBoxCount, sizeof(g_sCurrentLevel.ubBoxCount));
	for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
		fileWrite(pFile, &g_sCurrentLevel.pBoxSpawns[i].fX, sizeof(g_sCurrentLevel.pBoxSpawns[i].fX));
		fileWrite(pFile, &g_sCurrentLevel.pBoxSpawns[i].fY, sizeof(g_sCurrentLevel.pBoxSpawns[i].fY));
	}

	for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
		for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
			UWORD uwTileCode = mapGetTileAt(ubX, ubY);
			fileWrite(pFile, &uwTileCode, sizeof(uwTileCode));
		}
	}
	fileClose(pFile);
	systemUnuse();
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
	mapPressButtonIndex(eTile - TILE_BUTTON_1);
}

void mapPressButtonIndex(UBYTE ubButtonIndex) {
	s_ubButtonPressMask |= BV(ubButtonIndex);
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

UBYTE mapIsEmptyAt(UBYTE ubTileX, UBYTE ubTileY) {
	return g_sCurrentLevel.pTiles[ubTileX][ubTileY] == TILE_BG_1;
}

UBYTE mapIsCollidingWithPortalProjectilesAt(UBYTE ubTileX, UBYTE ubTileY) {
	return mapTileIsCollidingWithPortalProjectiles(g_sCurrentLevel.pTiles[ubTileX][ubTileY]);
}

UBYTE mapIsCollidingWithBouncersAt(UBYTE ubTileX, UBYTE ubTileY) {
	return mapTileIsCollidingWithBouncers(g_sCurrentLevel.pTiles[ubTileX][ubTileY]);
}

UBYTE mapIsSlipgatableAt(UBYTE ubTileX, UBYTE ubTileY) {
	return (g_sCurrentLevel.pTiles[ubTileX][ubTileY] & MAP_LAYER_SLIPGATABLE) != 0;
}

//---------------------------------------------------------------- TILE CHECKERS

UBYTE mapTileIsCollidingWithBoxes(tTile eTile) {
	return (eTile & (MAP_LAYER_WALLS | MAP_LAYER_FORCE_FIELDS)) != 0;
}

UBYTE mapTileIsCollidingWithPortalProjectiles(tTile eTile) {
	return (eTile & (MAP_LAYER_WALLS | MAP_LAYER_SLIPGATES)) != 0;
}

UBYTE mapTileIsCollidingWithBouncers(tTile eTile) {
	return (eTile & (MAP_LAYER_WALLS)) != 0;
}

UBYTE mapTileIsCollidingWithPlayers(tTile eTile) {
	return (eTile & (MAP_LAYER_WALLS | MAP_LAYER_LETHALS | MAP_LAYER_FORCE_FIELDS)) != 0;
}

UBYTE mapTileIsSlipgate(tTile eTile) {
	return (eTile & MAP_LAYER_SLIPGATES) != 0;
}

UBYTE mapTileIsLethal(tTile eTile) {
	return (eTile & MAP_LAYER_LETHALS) != 0;
}

UBYTE mapTileIsExit(tTile eTile) {
	return (eTile & MAP_LAYER_EXIT) != 0;
}

UBYTE mapTileIsButton(tTile eTile) {
	return (eTile & MAP_LAYER_BUTTON) != 0;
}

//-------------------------------------------------------------------- SLIPGATES

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY) {
	if(!mapIsSlipgatableAt(ubTileX, ubTileY)) {
		return 0;
	}

	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	if(pSlipgate->eNormal != DIRECTION_NONE) {
		setSlipgateTiles(pSlipgate, TILE_WALL_1);
	}

	if(
		mapIsEmptyAt(ubTileX - 1, ubTileY) &&
		mapIsSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY + 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY + 1;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_LEFT;
	}
	else if(
		mapIsEmptyAt(ubTileX + 1, ubTileY) &&
		mapIsSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY + 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY + 1;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_RIGHT;
	}
	else if(
		mapIsEmptyAt(ubTileX, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY - 1)
	) {
		g_pSlipgates[ubIndex].uwTileX = ubTileX;
		g_pSlipgates[ubIndex].uwTileY = ubTileY;
		g_pSlipgates[ubIndex].uwOtherTileX = ubTileX + 1;
		g_pSlipgates[ubIndex].uwOtherTileY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_UP;
	}
	else if(
		mapIsEmptyAt(ubTileX, ubTileY + 1) &&
		mapIsSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY + 1)
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
