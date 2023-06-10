/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "map.h"
#include <bartman/gcc8_c_support.h>
#include <ace/utils/file.h>
#include <ace/managers/system.h>
#include "game.h"
#include "bouncer.h"

#define MAP_SPIKES_COOLDOWN 50
#define MAP_DIRTY_TILES_MAX (MAP_TILE_WIDTH * MAP_TILE_HEIGHT)
#define MAP_TURRET_ATTACK_COOLDOWN 10
#define MAP_TURRET_TILE_RANGE 5

typedef struct tTurret {
	tUbCoordYX sTilePos;
	UBYTE isActive;
	UBYTE ubAttackCooldown;
	tUwCoordYX sScanTopLeft;
	tUwCoordYX sScanBottomRight;
	tDirection eDirection;
} tTurret;

//----------------------------------------------------------------- PRIVATE VARS

static tInteraction s_pInteractions[MAP_INTERACTIONS_MAX];
static UBYTE s_ubButtonPressMask;
static UBYTE s_ubCurrentInteraction;
static UWORD s_uwSpikeCooldown;
static UBYTE s_isSpikeActive;
static UBYTE s_pDirtyTiles[MAP_TILE_WIDTH][MAP_TILE_HEIGHT]; // x,y
static tUbCoordYX s_pDirtyTileQueues[2][MAP_DIRTY_TILES_MAX];
static UWORD s_pDirtyTileCounts[2];
static UBYTE s_ubCurrentDirtyList;
static tTurret s_pTurrets[MAP_TURRETS_MAX];
static UBYTE s_ubTurretCount;
static UBYTE s_ubCurrentTurret;

//------------------------------------------------------------ PRIVATE FUNCTIONS

static void mapOpenSlipgate(UBYTE ubIndex) {
	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	// Save terrain tiles
	pSlipgate->pPrevTiles[0] = g_sCurrentLevel.pTiles[pSlipgate->sTilePos.ubX][pSlipgate->sTilePos.ubY];
	pSlipgate->pPrevTiles[1] = g_sCurrentLevel.pTiles[pSlipgate->sTilePosOther.ubX][pSlipgate->sTilePosOther.ubY];

	// Draw slipgate tiles
	g_sCurrentLevel.pTiles[pSlipgate->sTilePos.ubX][pSlipgate->sTilePos.ubY] = TILE_SLIPGATE_1 + ubIndex;
	mapRequestTileDraw(pSlipgate->sTilePos.ubX, pSlipgate->sTilePos.ubY);
	g_sCurrentLevel.pTiles[pSlipgate->sTilePosOther.ubX][pSlipgate->sTilePosOther.ubY] = TILE_SLIPGATE_1 + ubIndex;
	mapRequestTileDraw(pSlipgate->sTilePosOther.ubX, pSlipgate->sTilePosOther.ubY);
}

static void mapCloseSlipgate(UBYTE ubIndex) {
	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	if(pSlipgate->eNormal != DIRECTION_NONE) {
		// Restore terrain tiles
		g_sCurrentLevel.pTiles[pSlipgate->sTilePos.ubX][pSlipgate->sTilePos.ubY] = pSlipgate->pPrevTiles[0];
		mapRequestTileDraw(pSlipgate->sTilePos.ubX, pSlipgate->sTilePos.ubY);
		g_sCurrentLevel.pTiles[pSlipgate->sTilePosOther.ubX][pSlipgate->sTilePosOther.ubY] = pSlipgate->pPrevTiles[1];
		mapRequestTileDraw(pSlipgate->sTilePosOther.ubX, pSlipgate->sTilePosOther.ubY);

		pSlipgate->eNormal = DIRECTION_NONE;
	}
}

static void mapProcessNextInteraction(void) {
	tInteraction *pInteraction = &s_pInteractions[s_ubCurrentInteraction];

	// Process effects of next interaction
	if(
		pInteraction->ubButtonMask &&
		(pInteraction->ubButtonMask & s_ubButtonPressMask) == pInteraction->ubButtonMask
	) {
		if(!pInteraction->wasActive) {
			for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
				tTogglableTile *pTile = &pInteraction->pTargetTiles[i];
				if(pTile->eKind == INTERACTION_KIND_SLIPGATABLE) {
					mapTryCloseSlipgateAt(0, pTile->sPos);
					mapTryCloseSlipgateAt(1, pTile->sPos);
				}
				g_sCurrentLevel.pTiles[pTile->sPos.ubX][pTile->sPos.ubY] = pTile->eTileActive;
				mapRequestTileDraw(pTile->sPos.ubX, pTile->sPos.ubY);
			}
			pInteraction->wasActive = 1;
		}
	}
	else {
		if(pInteraction->wasActive) {
			for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
				tTogglableTile *pTile = &pInteraction->pTargetTiles[i];
				if(pTile->eKind == INTERACTION_KIND_SLIPGATABLE) {
					mapTryCloseSlipgateAt(0, pTile->sPos);
					mapTryCloseSlipgateAt(1, pTile->sPos);
				}
				g_sCurrentLevel.pTiles[pTile->sPos.ubX][pTile->sPos.ubY] = pTile->eTileInactive;
				mapRequestTileDraw(pTile->sPos.ubX, pTile->sPos.ubY);
			}
			pInteraction->wasActive = 0;
		}
	}

	if(++s_ubCurrentInteraction >= MAP_INTERACTIONS_MAX) {
		s_ubCurrentInteraction = 0;
	}
}

static void mapProcessNextTurret(void) {
	if(++s_ubCurrentTurret > MAP_TURRETS_MAX) {
		s_ubCurrentTurret = 0;
	}

	tTurret *pTurret = &s_pTurrets[s_ubCurrentTurret];
	if(pTurret->isActive) {
		if(pTurret->ubAttackCooldown > 0) {
			BYTE bNewCooldown = (BYTE)pTurret->ubAttackCooldown - (BYTE)s_ubTurretCount;
			pTurret->ubAttackCooldown = MAX(0, bNewCooldown);
		}
		else {
			tPlayer *pPlayer = gameGetPlayer();
			UWORD uwPlayerX = fix16_to_int(pPlayer->sBody.fPosX);
			UWORD uwPlayerY = fix16_to_int(pPlayer->sBody.fPosY);
			if(
				pTurret->sScanTopLeft.uwX < uwPlayerX + pPlayer->sBody.ubWidth &&
				pTurret->sScanBottomRight.uwX > uwPlayerX &&
				pTurret->sScanTopLeft.uwY < uwPlayerY + pPlayer->sBody.ubHeight &&
				pTurret->sScanBottomRight.uwY > uwPlayerY
			) {
				playerDamage(pPlayer, 1);
				pTurret->ubAttackCooldown = MAP_TURRET_ATTACK_COOLDOWN;
			}
		}
	}
}

static UBYTE mapProcessSpikes(void) {
	if(--s_uwSpikeCooldown == 0) {
		s_isSpikeActive = !s_isSpikeActive;
		if(s_isSpikeActive)  {
			for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
				tUbCoordYX sSpikeCoord = g_sCurrentLevel.pSpikeTiles[i];
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = TILE_SPIKES_ON_FLOOR_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY);
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = TILE_SPIKES_ON_BG_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY - 1);
			}
		}
		else {
			for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
				tUbCoordYX sSpikeCoord = g_sCurrentLevel.pSpikeTiles[i];
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = TILE_SPIKES_OFF_FLOOR_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY);
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = TILE_SPIKES_OFF_BG_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY - 1);
			}
		}
		s_uwSpikeCooldown = MAP_SPIKES_COOLDOWN;
		return 1;
	}
	return 0;
}

static void mapDrawPendingTiles(void) {
	s_ubCurrentDirtyList = !s_ubCurrentDirtyList;
	for(UWORD i = 0; i < s_pDirtyTileCounts[s_ubCurrentDirtyList]; ++i) {
		tUbCoordYX sCoord = s_pDirtyTileQueues[s_ubCurrentDirtyList][i];
		gameDrawTile(sCoord.ubX, sCoord.ubY);
		--s_pDirtyTiles[sCoord.ubX][sCoord.ubY];
	}
	s_pDirtyTileCounts[s_ubCurrentDirtyList] = 0;
}

//------------------------------------------------------------- PUBLIC FUNCTIONS

void mapLoad(UBYTE ubIndex) {
	memset(s_pInteractions, 0, sizeof(s_pInteractions));
	g_sCurrentLevel.ubSpikeTilesCount = 0;
	s_ubTurretCount = 0;

	if(ubIndex == 0) {
		// hadcoded level
		memset(&g_sCurrentLevel, 0, sizeof(g_sCurrentLevel));
		for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
			g_sCurrentLevel.pTiles[ubX][0] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[ubX][1] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[ubX][2] = TILE_WALL_1;
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

		strcpy(
			g_sCurrentLevel.szStoryText,
			"The ruins looked dormant, with no traces of previous adventurers\n"
			"in sight, but soon first obstacles, and a helpful utility appeared."
		);
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
				tTogglableTile *pTargetTile = &pInteraction->pTargetTiles[ubTargetIndex];
				fileRead(pFile, &pTargetTile->eKind, sizeof(pTargetTile->eKind));
				fileRead(pFile, &pTargetTile->eTileActive, sizeof(pTargetTile->eTileActive));
				fileRead(pFile, &pTargetTile->eTileInactive, sizeof(pTargetTile->eTileInactive));
				fileRead(pFile, &pTargetTile->sPos.uwYX, sizeof(pTargetTile->sPos.uwYX));
			}
		}

		fileRead(pFile, &g_sCurrentLevel.ubBoxCount, sizeof(g_sCurrentLevel.ubBoxCount));
		for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
			fileRead(pFile, &g_sCurrentLevel.pBoxSpawns[i].fX, sizeof(g_sCurrentLevel.pBoxSpawns[i].fX));
			fileRead(pFile, &g_sCurrentLevel.pBoxSpawns[i].fY, sizeof(g_sCurrentLevel.pBoxSpawns[i].fY));
		}

		fileRead(pFile, &g_sCurrentLevel.ubSpikeTilesCount, sizeof(g_sCurrentLevel.ubSpikeTilesCount));
		for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
			fileRead(pFile, &g_sCurrentLevel.pSpikeTiles[i].uwYX, sizeof(g_sCurrentLevel.pSpikeTiles[i].uwYX));
		}

		UBYTE ubTurretCount;
		fileRead(pFile, &ubTurretCount, sizeof(ubTurretCount));

		UBYTE ubStoryTextLength;
		fileRead(pFile, &ubStoryTextLength, sizeof(ubStoryTextLength));
		if(ubStoryTextLength) {
			fileRead(pFile, g_sCurrentLevel.szStoryText, ubStoryTextLength);
			g_sCurrentLevel.szStoryText[ubStoryTextLength] = '\0';
		}

		UBYTE ubReservedCount1;
		fileRead(pFile, &ubReservedCount1, sizeof(ubReservedCount1));

		UBYTE ubReservedCount2;
		fileRead(pFile, &ubReservedCount2, sizeof(ubReservedCount2));

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
	s_uwSpikeCooldown = 1;
	s_isSpikeActive = 0;
	s_pDirtyTileCounts[0] = 0;
	s_pDirtyTileCounts[1] = 0;
	s_ubCurrentDirtyList = 0;
	memset(s_pDirtyTiles, 0, MAP_TILE_WIDTH * MAP_TILE_HEIGHT);
}

void mapSave(UBYTE ubIndex) {
	mapCloseSlipgate(0);
	mapCloseSlipgate(1);

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
			tTogglableTile *pTargetTile = &pInteraction->pTargetTiles[ubTargetIndex];
			fileWrite(pFile, &pTargetTile->eKind, sizeof(pTargetTile->eKind));
			fileWrite(pFile, &pTargetTile->eTileActive, sizeof(pTargetTile->eTileActive));
			fileWrite(pFile, &pTargetTile->eTileInactive, sizeof(pTargetTile->eTileInactive));
			fileWrite(pFile, &pTargetTile->sPos.uwYX, sizeof(pTargetTile->sPos.uwYX));
		}
	}

	fileWrite(pFile, &g_sCurrentLevel.ubBoxCount, sizeof(g_sCurrentLevel.ubBoxCount));
	for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
		fileWrite(pFile, &g_sCurrentLevel.pBoxSpawns[i].fX, sizeof(g_sCurrentLevel.pBoxSpawns[i].fX));
		fileWrite(pFile, &g_sCurrentLevel.pBoxSpawns[i].fY, sizeof(g_sCurrentLevel.pBoxSpawns[i].fY));
	}

	fileWrite(pFile, &g_sCurrentLevel.ubSpikeTilesCount, sizeof(g_sCurrentLevel.ubSpikeTilesCount));
	for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
		fileWrite(pFile, &g_sCurrentLevel.pSpikeTiles[i].uwYX, sizeof(g_sCurrentLevel.pSpikeTiles[i].uwYX));
	}

		UBYTE ubTurretCount = 0;
		fileWrite(pFile, &ubTurretCount, sizeof(ubTurretCount));

		UBYTE ubStoryTextLength = strlen(g_sCurrentLevel.szStoryText);
		fileWrite(pFile, &ubStoryTextLength, sizeof(ubStoryTextLength));
		if(ubStoryTextLength) {
			fileWrite(pFile, g_sCurrentLevel.szStoryText, ubStoryTextLength);
		}

		UBYTE ubReservedCount1 = 0;
		fileWrite(pFile, &ubReservedCount1, sizeof(ubReservedCount1));

		UBYTE ubReservedCount2 = 0;
		fileWrite(pFile, &ubReservedCount2, sizeof(ubReservedCount2));

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
	if(!mapProcessSpikes()) {
		mapProcessNextInteraction();
		mapProcessNextTurret();
	}

	mapDrawPendingTiles();

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

void mapDisableTurretAt(UBYTE ubX, UBYTE ubY) {
	tUbCoordYX sPos = {.ubX = ubX, .ubY = ubY};
	for(UBYTE i = 0; i < s_ubTurretCount; ++i) {
		if(s_pTurrets[i].sTilePos.uwYX == sPos.uwYX) {
			// Disable relevant turret logic
			s_pTurrets[i].isActive = 0;

			// Update turret tile
			g_sCurrentLevel.pTiles[ubX][ubY] = (
				(g_sCurrentLevel.pTiles[ubX][ubY] == TILE_TURRET_ACTIVE_LEFT) ?
				TILE_TURRET_INACTIVE_LEFT : TILE_TURRET_INACTIVE_RIGHT
			);
			mapRequestTileDraw(ubX, ubY);
			break;
		}
	}
}

void mapPressButtonIndex(UBYTE ubButtonIndex) {
	s_ubButtonPressMask |= BV(ubButtonIndex);
}

void mapAddOrRemoveSpikeTile(UBYTE ubX, UBYTE ubY) {
	tUbCoordYX sPos = {.ubX = ubX, .ubY = ubY};
	// Remove if list already contains pos
	for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
		if(g_sCurrentLevel.pSpikeTiles[i].uwYX == sPos.uwYX) {
			g_sCurrentLevel.pTiles[ubX][ubY] = TILE_WALL_1;
			g_sCurrentLevel.pTiles[ubX][ubY - 1] = TILE_BG_1;
			while(++i < g_sCurrentLevel.ubSpikeTilesCount) {
				g_sCurrentLevel.pSpikeTiles[i - 1].uwYX = g_sCurrentLevel.pSpikeTiles[i].uwYX;
			}
			--g_sCurrentLevel.ubSpikeTilesCount;
			return;
		}
	}

	if(g_sCurrentLevel.ubSpikeTilesCount < MAP_SPIKES_TILES_MAX) {
		g_sCurrentLevel.pSpikeTiles[g_sCurrentLevel.ubSpikeTilesCount++].uwYX = sPos.uwYX;
		g_sCurrentLevel.pTiles[ubX][ubY] = s_isSpikeActive ? TILE_SPIKES_ON_FLOOR_1 : TILE_SPIKES_OFF_FLOOR_1;
		g_sCurrentLevel.pTiles[ubX][ubY - 1] = s_isSpikeActive ? TILE_SPIKES_ON_BG_1 : TILE_SPIKES_OFF_BG_1;
	}
}

void mapAddOrRemoveTurret(UBYTE ubX, UBYTE ubY, tDirection eDirection) {
	tUbCoordYX sPos = {.ubX = ubX, .ubY = ubY};
	// Remove if list already contains pos
	for(UBYTE i = 0; i < s_ubTurretCount; ++i) {
		if(s_pTurrets[i].sTilePos.uwYX == sPos.uwYX) {
			g_sCurrentLevel.pTiles[ubX][ubY] = TILE_BG_1;
			while(++i < s_ubTurretCount) {
				s_pTurrets[i - 1] = s_pTurrets[i];
			}
			--s_ubTurretCount;
			return;
		}
	}

	if(s_ubTurretCount < MAP_SPIKES_TILES_MAX) {
		tTurret *pTurret = &s_pTurrets[s_ubTurretCount];
		++s_ubTurretCount;

		// Set up turret
		// TODO: move to initTurret() and call when recalc is needed
		pTurret->sTilePos.uwYX = sPos.uwYX;
		pTurret->isActive = 1;
		pTurret->eDirection = eDirection;
		pTurret->ubAttackCooldown = 0;
		g_sCurrentLevel.pTiles[ubX][ubY] = (
			(eDirection == DIRECTION_LEFT) ?
			TILE_TURRET_ACTIVE_LEFT :
			TILE_TURRET_ACTIVE_RIGHT
		);

		if(eDirection == DIRECTION_LEFT) {
			UBYTE ubLeftTileX = ubX;
			for(UBYTE i = 0; i < MAP_TURRET_TILE_RANGE; ++i) {
				if(mapIsCollidingWithPortalProjectilesAt(ubLeftTileX - 1, ubY)) {
					break;
				}
				--ubLeftTileX;
			}
			pTurret->sScanTopLeft = (tUwCoordYX) {.uwX = ubLeftTileX * MAP_TILE_SIZE, .uwY = ubY * MAP_TILE_SIZE};
			pTurret->sScanBottomRight = (tUwCoordYX) {.uwX = ubX * MAP_TILE_SIZE, .uwY = (ubY + 1) * MAP_TILE_SIZE};
		}
		else {
			UBYTE ubRightTileX = ubX + 1;
			for(UBYTE i = 0; i < MAP_TURRET_TILE_RANGE; ++i) {
				if(mapIsCollidingWithPortalProjectilesAt(ubRightTileX, ubY)) {
					break;
				}
				++ubRightTileX;
			}
			pTurret->sScanTopLeft = (tUwCoordYX) {.uwX = (ubX + 1) * MAP_TILE_SIZE, .uwY = ubY * MAP_TILE_SIZE};
			pTurret->sScanBottomRight = (tUwCoordYX) {.uwX = ubRightTileX * MAP_TILE_SIZE, .uwY = (ubY + 1) * MAP_TILE_SIZE};
		}

		// if(pTurret->sScanBottomRight.uwX - pTurret->sScanTopLeft.uwX != 0) {
		// 	tSimpleBufferManager *pBuffer = gameGetBuffer();
		// 	blitRect(
		// 		pBuffer->pBack,
		// 		pTurret->sScanTopLeft.uwX, pTurret->sScanTopLeft.uwY,
		// 		pTurret->sScanBottomRight.uwX - pTurret->sScanTopLeft.uwX,
		// 		pTurret->sScanBottomRight.uwY - pTurret->sScanTopLeft.uwY, 8
		// 	);
		// }
	}
}

void mapRequestTileDraw(UBYTE ubX, UBYTE ubY) {
	if(s_pDirtyTiles[ubX][ubY] < 2) {
		tUbCoordYX sCoord = {.ubX = ubX, .ubY = ubY};
		s_pDirtyTileQueues[s_ubCurrentDirtyList][s_pDirtyTileCounts[s_ubCurrentDirtyList]++] = sCoord;
		if(s_pDirtyTiles[ubX][ubY] < 1) {
			s_pDirtyTileQueues[!s_ubCurrentDirtyList][s_pDirtyTileCounts[!s_ubCurrentDirtyList]++] = sCoord;
		}
		s_pDirtyTiles[ubX][ubY] = 2;
	}
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
	return (g_sCurrentLevel.pTiles[ubTileX][ubTileY] & TILE_LAYER_SLIPGATABLE) != 0;
}

//---------------------------------------------------------------- TILE CHECKERS

UBYTE mapTileIsCollidingWithBoxes(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS | TILE_LAYER_FORCE_FIELDS)) != 0;
}

UBYTE mapTileIsCollidingWithPortalProjectiles(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS | TILE_LAYER_SLIPGATES)) != 0;
}

UBYTE mapTileIsCollidingWithBouncers(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS)) != 0;
}

UBYTE mapTileIsCollidingWithPlayers(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS | TILE_LAYER_LETHALS | TILE_LAYER_FORCE_FIELDS)) != 0;
}

UBYTE mapTileIsSlipgate(tTile eTile) {
	return (eTile & TILE_LAYER_SLIPGATES) != 0;
}

UBYTE mapTileIsLethal(tTile eTile) {
	return (eTile & TILE_LAYER_LETHALS) != 0;
}

UBYTE mapTileIsExit(tTile eTile) {
	return (eTile & TILE_LAYER_EXIT) != 0;
}

UBYTE mapTileIsButton(tTile eTile) {
	return (eTile & TILE_LAYER_BUTTON) != 0;
}

UBYTE mapTileIsActiveTurret(tTile eTile) {
	return (eTile & TILE_LAYER_ACTIVE_TURRET) != 0;
}

//-------------------------------------------------------------------- SLIPGATES

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY) {
	if(!mapIsSlipgatableAt(ubTileX, ubTileY)) {
		return 0;
	}

	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];

	if(
		mapIsEmptyAt(ubTileX - 1, ubTileY) &&
		mapIsSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY + 1)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY + 1;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_LEFT;
	}
	else if(
		mapIsEmptyAt(ubTileX - 1, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY - 1;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_LEFT;
	}
	else if(
		mapIsEmptyAt(ubTileX + 1, ubTileY) &&
		mapIsSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY + 1)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY + 1;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_RIGHT;
	}
	else if(
		mapIsEmptyAt(ubTileX + 1, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY - 1;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_RIGHT;
	}
	else if(
		mapIsEmptyAt(ubTileX, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY - 1)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX + 1;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_UP;
	}
	else if(
		mapIsEmptyAt(ubTileX - 1, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX, ubTileY - 1)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX - 1;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_UP;
	}
	else if(
		mapIsEmptyAt(ubTileX, ubTileY + 1) &&
		mapIsSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY + 1)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX + 1;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_DOWN;
	}
	else if(
		mapIsEmptyAt(ubTileX - 1, ubTileY + 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX, ubTileY + 1)
	) {
		mapCloseSlipgate(ubIndex);
		g_pSlipgates[ubIndex].sTilePos.ubX = ubTileX - 1;
		g_pSlipgates[ubIndex].sTilePos.ubY = ubTileY;
		g_pSlipgates[ubIndex].sTilePosOther.ubX = ubTileX;
		g_pSlipgates[ubIndex].sTilePosOther.ubY = ubTileY;
		g_pSlipgates[ubIndex].eNormal = DIRECTION_DOWN;
	}
	else {
		return 0;
	}

	mapTryCloseSlipgateAt(!ubIndex, pSlipgate->sTilePos);
	mapTryCloseSlipgateAt(!ubIndex, pSlipgate->sTilePosOther);

	mapOpenSlipgate(ubIndex);
	return 1;
}

void mapTryCloseSlipgateAt(UBYTE ubIndex, tUbCoordYX sPos) {
	if(slipgateIsOccupyingTile(&g_pSlipgates[ubIndex], sPos)) {
		mapCloseSlipgate(ubIndex);
	}
}

//------------------------------------------------------------------ GLOBAL VARS

tSlipgate g_pSlipgates[2];
tLevel g_sCurrentLevel;
