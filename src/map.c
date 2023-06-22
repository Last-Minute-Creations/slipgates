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
	tUwCoordYX sScanTopLeft;
	tUwCoordYX sScanBottomRight;
	tDirection eDirection;
	UBYTE isActive;
	UBYTE ubAttackCooldown;
} tTurret;

typedef enum tNeighborFlag {
	NEIGHBOR_FLAG_N  = BV(0),
	NEIGHBOR_FLAG_NE = BV(1),
	NEIGHBOR_FLAG_E  = BV(2),
	NEIGHBOR_FLAG_SE = BV(3),
	NEIGHBOR_FLAG_S  = BV(4),
	NEIGHBOR_FLAG_SW = BV(5),
	NEIGHBOR_FLAG_W  = BV(6),
	NEIGHBOR_FLAG_NW = BV(7),
} tNeighborFlag;

//----------------------------------------------------------------- PRIVATE VARS

static tInteraction s_pInteractions[MAP_INTERACTIONS_MAX];
static UWORD s_uwButtonPressMask;
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
static tLevel s_sLoadedLevel;

//------------------------------------------------------------ PRIVATE FUNCTIONS

static void mapOpenSlipgate(UBYTE ubIndex) {
	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	// Save terrain tiles
	pSlipgate->pPrevTiles[0] = g_sCurrentLevel.pTiles[pSlipgate->sTilePos.ubX][pSlipgate->sTilePos.ubY];
	pSlipgate->pPrevTiles[1] = g_sCurrentLevel.pTiles[pSlipgate->sTilePosOther.ubX][pSlipgate->sTilePosOther.ubY];

	// Draw slipgate tiles
	g_sCurrentLevel.pTiles[pSlipgate->sTilePos.ubX][pSlipgate->sTilePos.ubY] = TILE_SLIPGATE_A + ubIndex;
	mapRequestTileDraw(pSlipgate->sTilePos.ubX, pSlipgate->sTilePos.ubY);
	g_sCurrentLevel.pTiles[pSlipgate->sTilePosOther.ubX][pSlipgate->sTilePosOther.ubY] = TILE_SLIPGATE_A + ubIndex;
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
		pInteraction->uwButtonMask &&
		(pInteraction->uwButtonMask & s_uwButtonPressMask) == pInteraction->uwButtonMask
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
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = TILE_SPIKES_ON_FLOOR;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY);
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = TILE_SPIKES_ON_BG;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY - 1);
			}
		}
		else {
			for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
				tUbCoordYX sSpikeCoord = g_sCurrentLevel.pSpikeTiles[i];
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = TILE_SPIKES_OFF_FLOOR;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY);
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = TILE_SPIKES_OFF_BG;
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

static void mapInitTurret(tTurret *pTurret) {
		pTurret->isActive = 1;
		pTurret->ubAttackCooldown = 0;

		if(pTurret->eDirection == DIRECTION_LEFT) {
			UBYTE ubLeftTileX = pTurret->sTilePos.ubX;
			for(UBYTE i = 0; i < MAP_TURRET_TILE_RANGE; ++i) {
				if(mapIsCollidingWithPortalProjectilesAt(ubLeftTileX - 1, pTurret->sTilePos.ubY)) {
					break;
				}
				--ubLeftTileX;
			}
			pTurret->sScanTopLeft.uwX = ubLeftTileX * MAP_TILE_SIZE;
			pTurret->sScanTopLeft.uwY = pTurret->sTilePos.ubY * MAP_TILE_SIZE;
			pTurret->sScanBottomRight.uwX = pTurret->sTilePos.ubX * MAP_TILE_SIZE;
			pTurret->sScanBottomRight.uwY = (pTurret->sTilePos.ubY + 1) * MAP_TILE_SIZE;
		}
		else {
			UBYTE ubRightTileX = pTurret->sTilePos.ubX + 1;
			for(UBYTE i = 0; i < MAP_TURRET_TILE_RANGE; ++i) {
				if(mapIsCollidingWithPortalProjectilesAt(ubRightTileX, pTurret->sTilePos.ubY)) {
					break;
				}
				++ubRightTileX;
			}
			pTurret->sScanTopLeft.uwX = (pTurret->sTilePos.ubX + 1) * MAP_TILE_SIZE;
			pTurret->sScanTopLeft.uwY = pTurret->sTilePos.ubY * MAP_TILE_SIZE;
			pTurret->sScanBottomRight.uwX = ubRightTileX * MAP_TILE_SIZE;
			pTurret->sScanBottomRight.uwY = (pTurret->sTilePos.ubY + 1) * MAP_TILE_SIZE;
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

static tNeighborFlag mapGetWallNeighborsOnLevel(
	tLevel *pLevel, UBYTE ubTileX, UBYTE ubTileY
) {
	tNeighborFlag eNeighbors = 0;
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX - 1][ubTileY])) {
		eNeighbors |= NEIGHBOR_FLAG_W;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX + 1][ubTileY])) {
		eNeighbors |= NEIGHBOR_FLAG_E;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX - 1][ubTileY - 1])) {
		eNeighbors |= NEIGHBOR_FLAG_NW;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX + 1][ubTileY - 1])) {
		eNeighbors |= NEIGHBOR_FLAG_NE;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX - 1][ubTileY + 1])) {
		eNeighbors |= NEIGHBOR_FLAG_SW;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX + 1][ubTileY + 1])) {
		eNeighbors |= NEIGHBOR_FLAG_SE;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX][ubTileY - 1])) {
		eNeighbors |= NEIGHBOR_FLAG_N;
	}
	if(mapTileIsCollidingWithBouncers(pLevel->pTiles[ubTileX][ubTileY + 1])) {
		eNeighbors |= NEIGHBOR_FLAG_S;
	}

	return eNeighbors;
}

static tVisTile mapCalculateVisTileOnLevel(
	tLevel *pLevel, UBYTE ubTileX, UBYTE ubTileY
) {
	if(
		ubTileX == 0 || ubTileX == MAP_TILE_WIDTH - 1 ||
		ubTileY == 0 || ubTileY == MAP_TILE_HEIGHT - 1
	) {
		return VIS_TILE_WALL_1;
	}

	tNeighborFlag eNeighbors = mapGetWallNeighborsOnLevel(pLevel, ubTileX, ubTileY);
	tTile eTile = pLevel->pTiles[ubTileX][ubTileY];
	switch(eTile) {
		case TILE_BUTTON_A:
		case TILE_BUTTON_B:
		case TILE_BUTTON_C:
		case TILE_BUTTON_D:
		case TILE_BUTTON_E:
		case TILE_BUTTON_F:
		case TILE_BUTTON_G:
		case TILE_BUTTON_H:
			if(ubTileX < MAP_TILE_WIDTH / 2) {
				if(pLevel->pTiles[ubTileX + 1][ubTileY] == eTile) {
					return VIS_TILE_BUTTON_WALL_LEFT_1;
				}
				return VIS_TILE_BUTTON_WALL_LEFT_2;
			}
			else {
				if(pLevel->pTiles[ubTileX + 1][ubTileY] == eTile) {
					return VIS_TILE_BUTTON_WALL_RIGHT_1;
				}
				return VIS_TILE_BUTTON_WALL_RIGHT_2;
			}
			break;
		case TILE_WALL: {
			static const UBYTE pWallLookup[256] = {
				0,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_WALL_CONVEX_N,
				[NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_WALL_CONVEX_NE,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONVEX_E,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONVEX_SE,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONVEX_S,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E] = VIS_TILE_WALL_CONVEX_SW,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S] = VIS_TILE_WALL_CONVEX_W,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S] = VIS_TILE_WALL_CONVEX_NW,
				[255] = VIS_TILE_WALL_1,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONCAVE_N,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONCAVE_N,

				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONCAVE_NE,

				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONCAVE_E,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW] = VIS_TILE_WALL_CONCAVE_E,

				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_WALL_CONCAVE_SE,

				[NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_WALL_CONCAVE_S,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_WALL_CONCAVE_S,

				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N] = VIS_TILE_WALL_CONCAVE_SW,

				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N] = VIS_TILE_WALL_CONCAVE_W,
				[NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE] = VIS_TILE_WALL_CONCAVE_W,

				[NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E] = VIS_TILE_WALL_CONCAVE_NW,
			};
			if(pWallLookup[eNeighbors] != 0) {
				return pWallLookup[eNeighbors];
			}

			logWrite("Unhandled tile at %hhu,%hhu", ubTileX, ubTileY);
		} break;
		case TILE_BG: {
			tTile eTileBelow = pLevel->pTiles[ubTileX][ubTileY + 1];
			if(mapTileIsButton(eTileBelow)) {
				if(ubTileX < MAP_TILE_WIDTH / 2) {
					if(pLevel->pTiles[ubTileX + 1][ubTileY + 1] == eTileBelow) {
						return VIS_TILE_BUTTON_BG_LEFT_1;
					}
					return VIS_TILE_BUTTON_BG_LEFT_2;
				}
				else {
					if(pLevel->pTiles[ubTileX + 1][ubTileY + 1] == eTileBelow) {
						return VIS_TILE_BUTTON_BG_RIGHT_1;
					}
					return VIS_TILE_BUTTON_BG_RIGHT_2;
				}

			}

			static const UBYTE pBgLookup[256] = {
				0,

				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW] = VIS_TILE_BG_CONVEX_N,
				[NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW] = VIS_TILE_BG_CONVEX_N,
				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S] = VIS_TILE_BG_CONVEX_N,
				[NEIGHBOR_FLAG_S] = VIS_TILE_BG_CONVEX_N,

				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONCAVE_NE,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONCAVE_NE,
				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_N] = VIS_TILE_BG_CONCAVE_NE,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_N] = VIS_TILE_BG_CONCAVE_NE,

				[NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONVEX_E,
				[NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONVEX_E,
				[NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_BG_CONVEX_E,
				[NEIGHBOR_FLAG_W] = VIS_TILE_BG_CONVEX_E,

				[NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW] = VIS_TILE_BG_CONCAVE_SE,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW] = VIS_TILE_BG_CONCAVE_SE,
				[NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S] = VIS_TILE_BG_CONCAVE_SE,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S] = VIS_TILE_BG_CONCAVE_SE,

				[NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONVEX_S,
				[NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONVEX_S,
				[NEIGHBOR_FLAG_NE | NEIGHBOR_FLAG_N] = VIS_TILE_BG_CONVEX_S,
				[NEIGHBOR_FLAG_N] = VIS_TILE_BG_CONVEX_S,

				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONCAVE_SW,
				[NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONCAVE_SW,
				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_BG_CONCAVE_SW,
				[NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W] = VIS_TILE_BG_CONCAVE_SW,

				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_NE] = VIS_TILE_BG_CONVEX_W,
				[NEIGHBOR_FLAG_E | NEIGHBOR_FLAG_NE] = VIS_TILE_BG_CONVEX_W,
				[NEIGHBOR_FLAG_SE | NEIGHBOR_FLAG_E] = VIS_TILE_BG_CONVEX_W,
				[NEIGHBOR_FLAG_E] = VIS_TILE_BG_CONVEX_W,

				[NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE] = VIS_TILE_BG_CONCAVE_NW,
				[NEIGHBOR_FLAG_SW | NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N] = VIS_TILE_BG_CONCAVE_NW,
				[NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_NE] = VIS_TILE_BG_CONCAVE_NW,
				[NEIGHBOR_FLAG_W | NEIGHBOR_FLAG_NW | NEIGHBOR_FLAG_N] = VIS_TILE_BG_CONCAVE_NW,

				[NEIGHBOR_FLAG_SW] = VIS_TILE_BG_CONVEX_NE,
				[NEIGHBOR_FLAG_NW] = VIS_TILE_BG_CONVEX_SE,
				[NEIGHBOR_FLAG_NE] = VIS_TILE_BG_CONVEX_SW,
				[NEIGHBOR_FLAG_SE] = VIS_TILE_BG_CONVEX_NW,

			};

			if(pBgLookup[eNeighbors] != 0) {
				return pBgLookup[eNeighbors];
			}
		} break;
		default:
			return VIS_TILE_BG_1;
			break;
	}

	return VIS_TILE_BG_1;
}

static void mapFillVisTiles(tLevel *pLevel) {
	for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			pLevel->pVisTiles[ubX][ubY] = mapCalculateVisTileOnLevel(pLevel, ubX, ubY);
		}
	}
}

//------------------------------------------------------------- PUBLIC FUNCTIONS

void mapLoad(UBYTE ubIndex) {
	memset(s_pInteractions, 0, sizeof(s_pInteractions));
	s_sLoadedLevel.ubSpikeTilesCount = 0;
	s_ubTurretCount = 0;

	if(ubIndex == 0) {
		// hadcoded level
		memset(&s_sLoadedLevel, 0, sizeof(s_sLoadedLevel));
		for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
			s_sLoadedLevel.pTiles[ubX][0] = TILE_WALL;
			s_sLoadedLevel.pTiles[ubX][1] = TILE_WALL;
			s_sLoadedLevel.pTiles[ubX][2] = TILE_WALL;
			s_sLoadedLevel.pTiles[ubX][MAP_TILE_HEIGHT - 2] = TILE_WALL;
			s_sLoadedLevel.pTiles[ubX][MAP_TILE_HEIGHT - 1] = TILE_WALL;
		}
		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			s_sLoadedLevel.pTiles[0][ubY] = TILE_WALL;
			s_sLoadedLevel.pTiles[1][ubY] = TILE_WALL;
			s_sLoadedLevel.pTiles[MAP_TILE_WIDTH - 2][ubY] = TILE_WALL;
			s_sLoadedLevel.pTiles[MAP_TILE_WIDTH - 1][ubY] = TILE_WALL;
		}
		s_sLoadedLevel.sSpawnPos.fX = fix16_from_int(100);
		s_sLoadedLevel.sSpawnPos.fY = fix16_from_int(100);

		strcpy(
			s_sLoadedLevel.szStoryText,
			"The ruins looked dormant, with no traces of previous adventurers\n"
			"in sight, but soon first obstacles, and a helpful utility appeared."
		);
	}
	else {
		char szName[30];
		sprintf(szName, "data/levels/L%03hhu.dat", ubIndex);
		systemUse();
		tFile *pFile = fileOpen(szName, "rb");
		fileRead(pFile, &s_sLoadedLevel.sSpawnPos.fX, sizeof(s_sLoadedLevel.sSpawnPos.fX));
		fileRead(pFile, &s_sLoadedLevel.sSpawnPos.fY, sizeof(s_sLoadedLevel.sSpawnPos.fY));

		for(UBYTE ubInteractionIndex = 0; ubInteractionIndex < MAP_INTERACTIONS_MAX; ++ubInteractionIndex) {
			tInteraction *pInteraction = &s_pInteractions[ubInteractionIndex];
			fileRead(pFile, &pInteraction->ubTargetCount, sizeof(pInteraction->ubTargetCount));
			fileRead(pFile, &pInteraction->uwButtonMask, sizeof(pInteraction->uwButtonMask));
			fileRead(pFile, &pInteraction->wasActive, sizeof(pInteraction->wasActive));
			pInteraction->wasActive = 0;
			for(UBYTE ubTargetIndex = 0; ubTargetIndex < pInteraction->ubTargetCount; ++ubTargetIndex) {
				tTogglableTile *pTargetTile = &pInteraction->pTargetTiles[ubTargetIndex];
				fileRead(pFile, &pTargetTile->eKind, sizeof(pTargetTile->eKind));
				fileRead(pFile, &pTargetTile->eTileActive, sizeof(pTargetTile->eTileActive));
				fileRead(pFile, &pTargetTile->eTileInactive, sizeof(pTargetTile->eTileInactive));
				fileRead(pFile, &pTargetTile->sPos.uwYX, sizeof(pTargetTile->sPos.uwYX));
			}
		}

		fileRead(pFile, &s_sLoadedLevel.ubBoxCount, sizeof(s_sLoadedLevel.ubBoxCount));
		for(UBYTE i = 0; i < s_sLoadedLevel.ubBoxCount; ++i) {
			fileRead(pFile, &s_sLoadedLevel.pBoxSpawns[i].fX, sizeof(s_sLoadedLevel.pBoxSpawns[i].fX));
			fileRead(pFile, &s_sLoadedLevel.pBoxSpawns[i].fY, sizeof(s_sLoadedLevel.pBoxSpawns[i].fY));
		}

		fileRead(pFile, &s_sLoadedLevel.ubSpikeTilesCount, sizeof(s_sLoadedLevel.ubSpikeTilesCount));
		for(UBYTE i = 0; i < s_sLoadedLevel.ubSpikeTilesCount; ++i) {
			fileRead(pFile, &s_sLoadedLevel.pSpikeTiles[i].uwYX, sizeof(s_sLoadedLevel.pSpikeTiles[i].uwYX));
		}

		fileRead(pFile, &s_sLoadedLevel.ubTurretCount, sizeof(s_sLoadedLevel.ubTurretCount));
		for(UBYTE i = 0; i < s_sLoadedLevel.ubTurretCount; ++i) {
			fileRead(pFile, &s_sLoadedLevel.pTurretSpawns[i].sTilePos.uwYX, sizeof(s_sLoadedLevel.pTurretSpawns[i].sTilePos.uwYX));
			fileRead(pFile, &s_sLoadedLevel.pTurretSpawns[i].eDirection, sizeof(s_sLoadedLevel.pTurretSpawns[i].eDirection));
		}

		UBYTE ubStoryTextLength;
		fileRead(pFile, &ubStoryTextLength, sizeof(ubStoryTextLength));
		if(ubStoryTextLength) {
			fileRead(pFile, s_sLoadedLevel.szStoryText, ubStoryTextLength);
			s_sLoadedLevel.szStoryText[ubStoryTextLength] = '\0';
		}

		UBYTE ubReservedCount1;
		fileRead(pFile, &ubReservedCount1, sizeof(ubReservedCount1));

		UBYTE ubReservedCount2;
		fileRead(pFile, &ubReservedCount2, sizeof(ubReservedCount2));

		for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
			for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
				UWORD uwTileCode;
				fileRead(pFile, &uwTileCode, sizeof(uwTileCode));
				s_sLoadedLevel.pTiles[ubX][ubY] = uwTileCode;
				if(uwTileCode == TILE_BOUNCER_SPAWNER) {
					if(s_sLoadedLevel.ubBouncerSpawnerTileX != BOUNCER_TILE_INVALID) {
						logWrite("ERR: More than one bouncer spawner on map\n");
					}
					s_sLoadedLevel.ubBouncerSpawnerTileX = ubX;
					s_sLoadedLevel.ubBouncerSpawnerTileY = ubY;
				}
			}
		}

		fileClose(pFile);
		systemUnuse();
	}

	mapFillVisTiles(&s_sLoadedLevel);
	mapRestart();
}

void mapRestart(void) {
	memcpy(&g_sCurrentLevel, &s_sLoadedLevel, sizeof(s_sLoadedLevel));

	for(UBYTE ubInteractionIndex = 0; ubInteractionIndex < MAP_INTERACTIONS_MAX; ++ubInteractionIndex) {
		s_pInteractions[ubInteractionIndex].wasActive = 0;
	}

	s_ubTurretCount = s_sLoadedLevel.ubTurretCount;
	for(UBYTE i = 0; i < s_ubTurretCount; ++i)  {
		// Populate turret vars from spawns
		s_pTurrets[i].sTilePos.uwYX = s_sLoadedLevel.pTurretSpawns[i].sTilePos.uwYX;
		s_pTurrets[i].eDirection = s_sLoadedLevel.pTurretSpawns[i].eDirection;

		// Now that tiles are loaded, determine turret range etc
		mapInitTurret(&s_pTurrets[i]);
	}

	g_pSlipgates[0].eNormal = DIRECTION_NONE;
	g_pSlipgates[1].eNormal = DIRECTION_NONE;
	s_ubCurrentInteraction = 0;
	s_uwSpikeCooldown = 1;
	s_isSpikeActive = 0;
	s_pDirtyTileCounts[0] = 0;
	s_pDirtyTileCounts[1] = 0;
	s_ubCurrentDirtyList = 0;
	s_ubCurrentTurret = 0;
	memset(s_pDirtyTiles, 0, MAP_TILE_WIDTH * MAP_TILE_HEIGHT);
}

void mapSave(UBYTE ubIndex) {
	mapCloseSlipgate(0);
	mapCloseSlipgate(1);

	char szName[30];
	sprintf(szName, "data/levels/L%03hhu.dat", ubIndex);
	systemUse();
	tFile *pFile = fileOpen(szName, "wb");
	fileWrite(pFile, &g_sCurrentLevel.sSpawnPos.fX, sizeof(g_sCurrentLevel.sSpawnPos.fX));
	fileWrite(pFile, &g_sCurrentLevel.sSpawnPos.fY, sizeof(g_sCurrentLevel.sSpawnPos.fY));

	for(UBYTE ubInteractionIndex = 0; ubInteractionIndex < MAP_INTERACTIONS_MAX; ++ubInteractionIndex) {
		tInteraction *pInteraction = mapGetInteractionByIndex(ubInteractionIndex);
		fileWrite(pFile, &pInteraction->ubTargetCount, sizeof(pInteraction->ubTargetCount));
		fileWrite(pFile, &pInteraction->uwButtonMask, sizeof(pInteraction->uwButtonMask));
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

		fileWrite(pFile, &s_ubTurretCount, sizeof(s_ubTurretCount));
		for(UBYTE i = 0; i < s_ubTurretCount; ++i) {
			fileWrite(pFile, &s_pTurrets[i].sTilePos.uwYX, sizeof(s_pTurrets[i].sTilePos.uwYX));
			fileWrite(pFile, &s_pTurrets[i].eDirection, sizeof(s_pTurrets[i].eDirection));
		}

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
	s_uwButtonPressMask = 0;
}

void mapPressButtonAt(UBYTE ubX, UBYTE ubY) {
	tTile eTile = g_sCurrentLevel.pTiles[ubX][ubY];
	if(!mapTileIsButton(eTile)) {
		logWrite("ERR: Tile %d at %hhu,%hhu is not a button!\n", eTile, ubX, ubY);
	}
	mapPressButtonIndex(eTile - TILE_BUTTON_A);
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
	s_uwButtonPressMask |= BV(ubButtonIndex);
}

UWORD mapGetButtonPresses(void)
{
	return s_uwButtonPressMask;
}

void mapAddOrRemoveSpikeTile(UBYTE ubX, UBYTE ubY) {
	tUbCoordYX sPos = {.ubX = ubX, .ubY = ubY};
	// Remove if list already contains pos
	for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
		if(g_sCurrentLevel.pSpikeTiles[i].uwYX == sPos.uwYX) {
			g_sCurrentLevel.pTiles[ubX][ubY] = TILE_WALL;
			g_sCurrentLevel.pTiles[ubX][ubY - 1] = TILE_BG;
			while(++i < g_sCurrentLevel.ubSpikeTilesCount) {
				g_sCurrentLevel.pSpikeTiles[i - 1].uwYX = g_sCurrentLevel.pSpikeTiles[i].uwYX;
			}
			--g_sCurrentLevel.ubSpikeTilesCount;
			return;
		}
	}

	if(g_sCurrentLevel.ubSpikeTilesCount < MAP_SPIKES_TILES_MAX) {
		g_sCurrentLevel.pSpikeTiles[g_sCurrentLevel.ubSpikeTilesCount++].uwYX = sPos.uwYX;
		g_sCurrentLevel.pTiles[ubX][ubY] = s_isSpikeActive ? TILE_SPIKES_ON_FLOOR : TILE_SPIKES_OFF_FLOOR;
		g_sCurrentLevel.pTiles[ubX][ubY - 1] = s_isSpikeActive ? TILE_SPIKES_ON_BG : TILE_SPIKES_OFF_BG;
	}
}

void mapAddOrRemoveTurret(UBYTE ubX, UBYTE ubY, tDirection eDirection) {
	tUbCoordYX sPos = {.ubX = ubX, .ubY = ubY};
	// Remove if list already contains pos
	for(UBYTE i = 0; i < s_ubTurretCount; ++i) {
		if(s_pTurrets[i].sTilePos.uwYX == sPos.uwYX) {
			g_sCurrentLevel.pTiles[ubX][ubY] = TILE_BG;
			while(++i < s_ubTurretCount) {
				s_pTurrets[i - 1] = s_pTurrets[i];
			}
			--s_ubTurretCount;
			return;
		}
	}

	if(s_ubTurretCount < MAP_SPIKES_TILES_MAX) {
		tTurret *pTurret = &s_pTurrets[s_ubTurretCount];
		pTurret->sTilePos.uwYX = sPos.uwYX;
		pTurret->eDirection = eDirection;

		g_sCurrentLevel.pTiles[ubX][ubY] = (
			(pTurret->eDirection == DIRECTION_LEFT) ?
			TILE_TURRET_ACTIVE_LEFT :
			TILE_TURRET_ACTIVE_RIGHT
		);

		++s_ubTurretCount;
		mapInitTurret(pTurret);
	}
}

void mapRequestTileDraw(UBYTE ubTileX, UBYTE ubTileY) {
	if(s_pDirtyTiles[ubTileX][ubTileY] < 2) {
		tUbCoordYX sCoord = {.ubX = ubTileX, .ubY = ubTileY};
		s_pDirtyTileQueues[s_ubCurrentDirtyList][s_pDirtyTileCounts[s_ubCurrentDirtyList]++] = sCoord;
		if(s_pDirtyTiles[ubTileX][ubTileY] < 1) {
			s_pDirtyTileQueues[!s_ubCurrentDirtyList][s_pDirtyTileCounts[!s_ubCurrentDirtyList]++] = sCoord;
		}
		s_pDirtyTiles[ubTileX][ubTileY] = 2;
	}
}

void mapRecalculateVisTilesNearTileAt(UBYTE ubTileX, UBYTE ubTileY) {
	for(UBYTE ubX = ubTileX - 1; ubX <= ubTileX + 1; ++ubX) {
		for(UBYTE ubY = ubTileY - 1; ubY <= ubTileY + 1; ++ubY) {
			// Checking if eOld == eNew won't work for changing buttons (same vis, different logic tiles)
			tVisTile eNew = mapCalculateVisTileOnLevel(&g_sCurrentLevel, ubX, ubY);
			g_sCurrentLevel.pVisTiles[ubX][ubY] = eNew;
			mapRequestTileDraw(ubX, ubY);
		}
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

tVisTile mapGetVisTileAt(UBYTE ubTileX, UBYTE ubTileY) {
	return g_sCurrentLevel.pVisTiles[ubTileX][ubTileY];
}

tTile mapGetTileAt(UBYTE ubTileX, UBYTE ubTileY) {
	return g_sCurrentLevel.pTiles[ubTileX][ubTileY];
}

UBYTE mapIsEmptyAt(UBYTE ubTileX, UBYTE ubTileY) {
	return g_sCurrentLevel.pTiles[ubTileX][ubTileY] == TILE_BG;
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
	return (eTile & (TILE_LAYER_WALLS | TILE_LAYER_GRATES)) != 0;
}

UBYTE mapTileIsCollidingWithPortalProjectiles(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS | TILE_LAYER_SLIPGATES)) != 0;
}

UBYTE mapTileIsCollidingWithBouncers(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS)) != 0;
}

UBYTE mapTileIsCollidingWithPlayers(tTile eTile) {
	return (eTile & (TILE_LAYER_WALLS | TILE_LAYER_LETHALS | TILE_LAYER_GRATES)) != 0;
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
