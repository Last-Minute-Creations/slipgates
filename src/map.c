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
#define MAP_PENDING_SLIPGATE_OPEN_INVALID 0xFF

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

typedef struct tGatewayKind {
	tTile eTileFront;
	tVisTile eVisTileFirst;
} tGatewayKind;

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
static UBYTE s_ubPendingSlipgateOpenIndex;
static UBYTE s_ubPendingSlipgateDraws;

static const tGatewayKind s_pGatewayKinds[] = {
	{.eTileFront = TILE_DOOR_CLOSED, .eVisTileFirst = VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP},
	{.eTileFront = TILE_DOOR_OPEN, .eVisTileFirst = VIS_TILE_DOOR_LEFT_OPEN_WALL_TOP},
	{.eTileFront = TILE_GRATE, .eVisTileFirst = VIS_TILE_GRATE_LEFT_WALL_TOP},
	{.eTileFront = TILE_DEATH_FIELD, .eVisTileFirst = VIS_TILE_DEATH_FIELD_LEFT_WALL_TOP},
};
#define MAP_GATEWAY_KIND_COUNT (sizeof(s_pGatewayKinds) / sizeof(s_pGatewayKinds[0]))

//------------------------------------------------------------ PRIVATE FUNCTIONS

static void mapOpenSlipgate(UBYTE ubIndex) {
	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	// Save logic tiles
	pSlipgate->pPrevTiles[0] = g_sCurrentLevel.pTiles[pSlipgate->sTilePositions[0].ubX][pSlipgate->sTilePositions[0].ubY];
	pSlipgate->pPrevTiles[1] = g_sCurrentLevel.pTiles[pSlipgate->sTilePositions[1].ubX][pSlipgate->sTilePositions[1].ubY];

	// Change logic tiles to slipgate
	g_sCurrentLevel.pTiles[pSlipgate->sTilePositions[0].ubX][pSlipgate->sTilePositions[0].ubY] = TILE_SLIPGATE_A + ubIndex;
	g_sCurrentLevel.pTiles[pSlipgate->sTilePositions[1].ubX][pSlipgate->sTilePositions[1].ubY] = TILE_SLIPGATE_A + ubIndex;

	if(s_ubPendingSlipgateOpenIndex != MAP_PENDING_SLIPGATE_OPEN_INVALID) {
		logWrite("ERR: Previous pending slipgate haven't been opened!\n");
	}
	s_ubPendingSlipgateOpenIndex = ubIndex;
	s_ubPendingSlipgateDraws = 2;
}

static void mapCloseSlipgate(UBYTE ubIndex) {
	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];
	if(pSlipgate->eNormal != DIRECTION_NONE) {
		// Restore logic tiles
		g_sCurrentLevel.pTiles[pSlipgate->sTilePositions[0].ubX][pSlipgate->sTilePositions[0].ubY] = pSlipgate->pPrevTiles[0];
		mapRequestTileDraw(pSlipgate->sTilePositions[0].ubX, pSlipgate->sTilePositions[0].ubY);
		g_sCurrentLevel.pTiles[pSlipgate->sTilePositions[1].ubX][pSlipgate->sTilePositions[1].ubY] = pSlipgate->pPrevTiles[1];
		mapRequestTileDraw(pSlipgate->sTilePositions[1].ubX, pSlipgate->sTilePositions[1].ubY);

		// Redraw bg tiles
		mapRequestTileDraw(pSlipgate->sTilePositions[2].ubX, pSlipgate->sTilePositions[2].ubY);
		mapRequestTileDraw(pSlipgate->sTilePositions[3].ubX, pSlipgate->sTilePositions[3].ubY);

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
				g_sCurrentLevel.pVisTiles[pTile->sPos.ubX][pTile->sPos.ubY] = pTile->eVisTileActive;
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
				g_sCurrentLevel.pVisTiles[pTile->sPos.ubX][pTile->sPos.ubY] = pTile->eVisTileInactive;
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
				g_sCurrentLevel.pVisTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = VIS_TILE_SPIKES_ON_FLOOR_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY);
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = TILE_SPIKES_ON_BG;
				g_sCurrentLevel.pVisTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = VIS_TILE_SPIKES_ON_BG_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY - 1);
			}
		}
		else {
			for(UBYTE i = 0; i < g_sCurrentLevel.ubSpikeTilesCount; ++i) {
				tUbCoordYX sSpikeCoord = g_sCurrentLevel.pSpikeTiles[i];
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = TILE_SPIKES_OFF_FLOOR;
				g_sCurrentLevel.pVisTiles[sSpikeCoord.ubX][sSpikeCoord.ubY] = VIS_TILE_SPIKES_OFF_FLOOR_1;
				mapRequestTileDraw(sSpikeCoord.ubX, sSpikeCoord.ubY);
				g_sCurrentLevel.pTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = TILE_SPIKES_OFF_BG;
				g_sCurrentLevel.pVisTiles[sSpikeCoord.ubX][sSpikeCoord.ubY - 1] = VIS_TILE_SPIKES_OFF_BG_1;
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
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX - 1][ubTileY])) {
		eNeighbors |= NEIGHBOR_FLAG_W;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX + 1][ubTileY])) {
		eNeighbors |= NEIGHBOR_FLAG_E;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX - 1][ubTileY - 1])) {
		eNeighbors |= NEIGHBOR_FLAG_NW;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX + 1][ubTileY - 1])) {
		eNeighbors |= NEIGHBOR_FLAG_NE;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX - 1][ubTileY + 1])) {
		eNeighbors |= NEIGHBOR_FLAG_SW;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX + 1][ubTileY + 1])) {
		eNeighbors |= NEIGHBOR_FLAG_SE;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX][ubTileY - 1])) {
		eNeighbors |= NEIGHBOR_FLAG_N;
	}
	if(mapTileIsOnWall(pLevel->pTiles[ubTileX][ubTileY + 1])) {
		eNeighbors |= NEIGHBOR_FLAG_S;
	}

	return eNeighbors;
}


static tVisTile mapCalculateVisTileForGatewayWallTiles(
	tLevel *pLevel, UBYTE ubTileX, UBYTE ubTileY,
	tTile eTile, tVisTile eVisTileFirst
) {
	tTile eTileLeft = pLevel->pTiles[ubTileX - 1][ubTileY];
	tTile eTileRight = pLevel->pTiles[ubTileX + 1][ubTileY];
	tTile eTileAbove = pLevel->pTiles[ubTileX][ubTileY - 1];
	tTile eTileBelow = pLevel->pTiles[ubTileX][ubTileY + 1];

	if(eTileAbove == eTile) {
		return (eVisTileFirst + (
			(ubTileX < MAP_TILE_WIDTH / 2) ?
			VIS_TILE_DOOR_LEFT_CLOSED_WALL_BOTTOM :
			VIS_TILE_DOOR_RIGHT_CLOSED_WALL_BOTTOM
		) - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP);
	}
	if(eTileBelow == eTile) {
		return (eVisTileFirst + (
			(ubTileX < MAP_TILE_WIDTH / 2) ?
			VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP :
			VIS_TILE_DOOR_RIGHT_CLOSED_WALL_TOP
		) - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP);
	}
	if(eTileLeft == eTile) {
		return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_WALL_RIGHT - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
	}
	if(eTileRight == eTile) {
		return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_WALL_LEFT - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
	}

	return VIS_TILE_BG_1;
}

static tVisTile mapCalculateVisTileForGatewayBgTiles(
	tLevel *pLevel, UBYTE ubTileX, UBYTE ubTileY,
	tTile eTile, tVisTile eVisTileFirst
) {
	tTile eTileBelow = pLevel->pTiles[ubTileX][ubTileY + 1];

	if(eTileBelow == eTile) {
		if(pLevel->pTiles[ubTileX - 1][ubTileY + 1] != eTile) {
			return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_TOP_LEFT - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
		}
		if(pLevel->pTiles[ubTileX + 1][ubTileY + 1] != eTile) {
			return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_TOP_RIGHT - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
		}
		return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_TOP_MID - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
	}
	return VIS_TILE_BG_1;
}

static tVisTile mapCalculateVisTileForGatewayFrontTiles(
	tLevel *pLevel, UBYTE ubTileX, UBYTE ubTileY,
	tTile eTile, tVisTile eVisTileFirst
) {
	tTile eTileLeft = pLevel->pTiles[ubTileX - 1][ubTileY];
	tTile eTileRight = pLevel->pTiles[ubTileX + 1][ubTileY];
	tTile eTileAbove = pLevel->pTiles[ubTileX][ubTileY - 1];
	tTile eTileBelow = pLevel->pTiles[ubTileX][ubTileY + 1];

	if(eTileLeft == eTile || eTileRight == eTile) {
		// Horizontal gateway
		if(eTileLeft != eTile) {
			return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_BOTTOM_LEFT - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
		}
		if(eTileRight != eTile) {
			return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_BOTTOM_RIGHT - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
		}
		return eVisTileFirst + VIS_TILE_DOOR_DOWN_CLOSED_BOTTOM_MID - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
	}
	else {
		// Vertical gateway
		if(ubTileX < MAP_TILE_WIDTH / 2) {
			if(eTileAbove != eTile) {
				return eVisTileFirst + VIS_TILE_DOOR_LEFT_CLOSED_TOP - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
			}
			if(eTileBelow != eTile) {
				return eVisTileFirst + VIS_TILE_DOOR_LEFT_CLOSED_BOTTOM - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
			}
			return eVisTileFirst + VIS_TILE_DOOR_LEFT_CLOSED_MID - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
		}
		else {
			if(eTileAbove != eTile) {
				return eVisTileFirst + VIS_TILE_DOOR_RIGHT_CLOSED_TOP - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
			}
			if(eTileBelow != eTile) {
				return eVisTileFirst + VIS_TILE_DOOR_RIGHT_CLOSED_BOTTOM - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
			}
			return eVisTileFirst + VIS_TILE_DOOR_RIGHT_CLOSED_MID - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP;
		}
	}

	return VIS_TILE_BG_1;
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
	tTile eTileLeft = pLevel->pTiles[ubTileX - 1][ubTileY];
	tTile eTileRight = pLevel->pTiles[ubTileX + 1][ubTileY];
	tTile eTileAbove = pLevel->pTiles[ubTileX][ubTileY - 1];
	tTile eTileBelow = pLevel->pTiles[ubTileX][ubTileY + 1];
	switch(eTile) {
		case TILE_DOOR_CLOSED:
		case TILE_DOOR_OPEN:
		case TILE_GRATE:
		case TILE_DEATH_FIELD:
			for(UBYTE i = 0; i < MAP_GATEWAY_KIND_COUNT; ++i) {
				if(s_pGatewayKinds[i].eTileFront == eTile) {
					return mapCalculateVisTileForGatewayFrontTiles(
						pLevel, ubTileX, ubTileY, eTile, s_pGatewayKinds[i].eVisTileFirst
					);
				}
			}
			break;
		case TILE_BOUNCER_SPAWNER:
			if((eNeighbors & NEIGHBOR_FLAG_S) == 0) {
				return VIS_TILE_BOUNCER_SPAWNER_WALL_DOWN;
			}
			if((eNeighbors & NEIGHBOR_FLAG_N) == 0) {
				return VIS_TILE_BOUNCER_SPAWNER_WALL_UP;
			}
			if((eNeighbors & NEIGHBOR_FLAG_E) == 0) {
				return VIS_TILE_BOUNCER_SPAWNER_WALL_RIGHT;
			}
			if((eNeighbors & NEIGHBOR_FLAG_W) == 0) {
				return VIS_TILE_BOUNCER_SPAWNER_WALL_LEFT;
			}
			break;
		case TILE_RECEIVER:
			if((eNeighbors & NEIGHBOR_FLAG_S) == 0) {
				return VIS_TILE_RECEIVER_WALL_DOWN;
			}
			if((eNeighbors & NEIGHBOR_FLAG_N) == 0) {
				return VIS_TILE_RECEIVER_WALL_UP;
			}
			if((eNeighbors & NEIGHBOR_FLAG_E) == 0) {
				return VIS_TILE_RECEIVER_WALL_RIGHT;
			}
			if((eNeighbors & NEIGHBOR_FLAG_W) == 0) {
				return VIS_TILE_RECEIVER_WALL_LEFT;
			}
			break;
		case TILE_PIPE:
			if(
				eTileAbove == TILE_PIPE && eTileBelow == TILE_PIPE &&
				eTileLeft == TILE_PIPE && eTileRight == TILE_PIPE
			) {
				return VIS_TILE_PIPE_NESW;
			}
			if(eTileAbove == TILE_PIPE && eTileBelow == TILE_PIPE) {
				return VIS_TILE_PIPE_NS;
			}
			if(eTileLeft == TILE_PIPE && eTileRight == TILE_PIPE) {
				return VIS_TILE_PIPE_EW;
			}
			if(eTileAbove == TILE_PIPE) {
				if(eTileLeft == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_WALL_PIPE_S;
				}
				return VIS_TILE_WALL_PIPE_S;
			}
			if(eTileBelow == TILE_PIPE) {
				if(eTileLeft == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_WALL_PIPE_N;
				}
				return VIS_TILE_WALL_PIPE_N;
			}
			if(eTileLeft == TILE_PIPE) {
				if(eTileBelow == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_WALL_PIPE_E;
				}
				return VIS_TILE_WALL_PIPE_E;
			}
			if(eTileRight == TILE_PIPE) {
				if(eTileBelow == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_WALL_PIPE_W;
				}
				return VIS_TILE_WALL_PIPE_W;
			}
			break;
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
		case TILE_EXIT: {
			tTile eTileAbove = pLevel->pTiles[ubTileX][ubTileY - 1];
			tTile eTileBelow = pLevel->pTiles[ubTileX][ubTileY + 1];
			if(ubTileX < MAP_TILE_WIDTH / 2) {
				if(eTileAbove != eTile) {
					return VIS_TILE_EXIT_WALL_LEFT_TOP;
				}
				if(eTileBelow != eTile) {
					return VIS_TILE_EXIT_WALL_LEFT_BOTTOM;
				}
				return VIS_TILE_EXIT_WALL_LEFT_MID;
			}
			else {
				if(eTileAbove != eTile) {
					return VIS_TILE_EXIT_WALL_RIGHT_TOP;
				}
				if(eTileBelow != eTile) {
					return VIS_TILE_EXIT_WALL_RIGHT_BOTTOM;
				}
				return VIS_TILE_EXIT_WALL_RIGHT_MID;
			}
		} break;
		case TILE_WALL:
		case TILE_WALL_BLOCKED: {
			for(UBYTE i = 0; i < MAP_GATEWAY_KIND_COUNT; ++i) {
				tVisTile eVisTile = mapCalculateVisTileForGatewayWallTiles(
					pLevel, ubTileX, ubTileY,
					s_pGatewayKinds[i].eTileFront, s_pGatewayKinds[i].eVisTileFirst
				);
				if(eVisTile != VIS_TILE_BG_1) {
					return eVisTile;
				}
			}

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
				if(eTile == TILE_WALL_BLOCKED) {
					return pWallLookup[eNeighbors] + VIS_TILE_BLOCKED_WALL_CONVEX_N - VIS_TILE_WALL_CONVEX_N;
				}
				return pWallLookup[eNeighbors];
			}

			logWrite("Unhandled tile at %hhu,%hhu", ubTileX, ubTileY);
		} break;
		case TILE_BG: {
			if(eTileAbove == TILE_BOUNCER_SPAWNER) {
				return VIS_TILE_BOUNCER_SPAWNER_BG_DOWN;
			}
			if(eTileBelow == TILE_BOUNCER_SPAWNER) {
				return VIS_TILE_BOUNCER_SPAWNER_BG_UP;
			}
			if(eTileLeft == TILE_BOUNCER_SPAWNER) {
				return VIS_TILE_BOUNCER_SPAWNER_BG_RIGHT;
			}
			if(eTileRight == TILE_BOUNCER_SPAWNER) {
				return VIS_TILE_BOUNCER_SPAWNER_BG_LEFT;
			}
			if(eTileAbove == TILE_RECEIVER) {
				return VIS_TILE_RECEIVER_BG_DOWN;
			}
			if(eTileBelow == TILE_RECEIVER) {
				return VIS_TILE_RECEIVER_BG_UP;
			}
			if(eTileLeft == TILE_RECEIVER) {
				return VIS_TILE_RECEIVER_BG_RIGHT;
			}
			if(eTileRight == TILE_RECEIVER) {
				return VIS_TILE_RECEIVER_BG_LEFT;
			}
			if(eTileAbove == TILE_PIPE) {
				if(pLevel->pTiles[ubTileX - 1][ubTileY - 1] == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_BG_PIPE_S;
				}
				return VIS_TILE_BG_PIPE_S;
			}
			if(eTileBelow == TILE_PIPE) {
				if(pLevel->pTiles[ubTileX - 1][ubTileY + 1] == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_BG_PIPE_N;
				}
				return VIS_TILE_BG_PIPE_N;
			}
			if(eTileLeft == TILE_PIPE) {
				if(pLevel->pTiles[ubTileX - 1][ubTileY + 1] == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_BG_PIPE_E;
				}
				return VIS_TILE_BG_PIPE_E;
			}
			if(eTileRight == TILE_PIPE) {
				if(pLevel->pTiles[ubTileX + 1][ubTileY + 1] == TILE_WALL_BLOCKED) {
					return VIS_TILE_BLOCKED_BG_PIPE_W;
				}
				return VIS_TILE_BG_PIPE_W;
			}
			if(eTileLeft == TILE_EXIT) {
				if(pLevel->pTiles[ubTileX - 1][ubTileY - 1] != TILE_EXIT) {
					return VIS_TILE_EXIT_BG_LEFT_TOP;
				}
				if(pLevel->pTiles[ubTileX - 1][ubTileY + 1] != TILE_EXIT) {
					return VIS_TILE_EXIT_BG_LEFT_BOTTOM;
				}
				return VIS_TILE_EXIT_BG_LEFT_MID;
			}
			if(eTileRight == TILE_EXIT) {
				if(pLevel->pTiles[ubTileX + 1][ubTileY - 1] != TILE_EXIT) {
					return VIS_TILE_EXIT_BG_RIGHT_TOP;
				}
				if(pLevel->pTiles[ubTileX + 1][ubTileY + 1] != TILE_EXIT) {
					return VIS_TILE_EXIT_BG_RIGHT_BOTTOM;
				}
				return VIS_TILE_EXIT_BG_RIGHT_MID;
			}

			for(UBYTE i = 0; i < MAP_GATEWAY_KIND_COUNT; ++i) {
				tVisTile eVisTile = mapCalculateVisTileForGatewayBgTiles(
					pLevel, ubTileX, ubTileY,
					s_pGatewayKinds[i].eTileFront, s_pGatewayKinds[i].eVisTileFirst
				);
				if(eVisTile != VIS_TILE_BG_1) {
					return eVisTile;
				}
			}

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
				if(
					eTileAbove == TILE_WALL_BLOCKED || eTileBelow == TILE_WALL_BLOCKED ||
					eTileLeft == TILE_WALL_BLOCKED || eTileRight == TILE_WALL_BLOCKED || (
						(
							eNeighbors & (
								NEIGHBOR_FLAG_N | NEIGHBOR_FLAG_E |
								NEIGHBOR_FLAG_S | NEIGHBOR_FLAG_W
							)
						) == 0 && (
							pLevel->pTiles[ubTileX - 1][ubTileY - 1] == TILE_WALL_BLOCKED ||
							pLevel->pTiles[ubTileX - 1][ubTileY + 1] == TILE_WALL_BLOCKED ||
							pLevel->pTiles[ubTileX + 1][ubTileY - 1] == TILE_WALL_BLOCKED ||
							pLevel->pTiles[ubTileX + 1][ubTileY + 1] == TILE_WALL_BLOCKED
						)
					)
				) {
					return pBgLookup[eNeighbors] + VIS_TILE_BLOCKED_BG_CONVEX_N - VIS_TILE_BG_CONVEX_N;
				}
				return pBgLookup[eNeighbors];
			}
		} break;
		case TILE_TURRET_ACTIVE_LEFT:
			return VIS_TILE_TURRET_ACTIVE_LEFT;
		case TILE_TURRET_ACTIVE_RIGHT:
			return VIS_TILE_TURRET_ACTIVE_RIGHT;
		case TILE_TURRET_INACTIVE_LEFT:
			return VIS_TILE_TURRET_INACTIVE_LEFT;
		case TILE_TURRET_INACTIVE_RIGHT:
			return VIS_TILE_TURRET_INACTIVE_RIGHT;
		case TILE_SPIKES_OFF_BG:
			return VIS_TILE_SPIKES_OFF_BG_1;
		case TILE_SPIKES_OFF_FLOOR:
			return VIS_TILE_SPIKES_OFF_FLOOR_1;
		case TILE_SPIKES_ON_BG:
			return VIS_TILE_SPIKES_ON_BG_1;
		case TILE_SPIKES_ON_FLOOR:
			return VIS_TILE_SPIKES_ON_FLOOR_1;
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
	s_ubPendingSlipgateOpenIndex = MAP_PENDING_SLIPGATE_OPEN_INVALID;
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

	if(s_ubPendingSlipgateOpenIndex != MAP_PENDING_SLIPGATE_OPEN_INVALID) {
		gameDrawSlipgate(s_ubPendingSlipgateOpenIndex);
		if(--s_ubPendingSlipgateDraws == 0) {
			s_ubPendingSlipgateOpenIndex = MAP_PENDING_SLIPGATE_OPEN_INVALID;
		}
	}

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
			if(g_sCurrentLevel.pTiles[ubX][ubY] == TILE_TURRET_ACTIVE_LEFT) {
				g_sCurrentLevel.pTiles[ubX][ubY] = TILE_TURRET_INACTIVE_LEFT;
				g_sCurrentLevel.pVisTiles[ubX][ubY] = VIS_TILE_TURRET_INACTIVE_LEFT;
			}
			else {
				g_sCurrentLevel.pTiles[ubX][ubY] = TILE_TURRET_INACTIVE_RIGHT;
				g_sCurrentLevel.pVisTiles[ubX][ubY] = VIS_TILE_TURRET_INACTIVE_RIGHT;
			}
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
		if(s_isSpikeActive) {
			g_sCurrentLevel.pTiles[ubX][ubY] = TILE_SPIKES_ON_FLOOR;
			g_sCurrentLevel.pVisTiles[ubX][ubY] = VIS_TILE_SPIKES_OFF_FLOOR_1;
			g_sCurrentLevel.pTiles[ubX][ubY - 1] = TILE_SPIKES_ON_BG;
			g_sCurrentLevel.pVisTiles[ubX][ubY - 1] = VIS_TILE_SPIKES_OFF_BG_1;
		}
		else {
			g_sCurrentLevel.pTiles[ubX][ubY] = TILE_SPIKES_OFF_FLOOR;
			g_sCurrentLevel.pVisTiles[ubX][ubY] = VIS_TILE_SPIKES_ON_FLOOR_1;
			g_sCurrentLevel.pTiles[ubX][ubY - 1] = TILE_SPIKES_OFF_BG;
			g_sCurrentLevel.pVisTiles[ubX][ubY - 1] = VIS_TILE_SPIKES_ON_BG_1;
		}
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

	if(s_ubTurretCount < MAP_TURRETS_MAX) {
		tTurret *pTurret = &s_pTurrets[s_ubTurretCount];
		pTurret->sTilePos.uwYX = sPos.uwYX;
		pTurret->eDirection = eDirection;

		g_sCurrentLevel.pTiles[ubX][ubY] = (
			(pTurret->eDirection == DIRECTION_LEFT) ?
			TILE_TURRET_ACTIVE_LEFT :
			TILE_TURRET_ACTIVE_RIGHT
		);
		mapRecalculateVisTilesNearTileAt(ubX, ubY);

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

void mapSetOrRemoveDoorInteractionAt(
	UBYTE ubInteractionIndex, UBYTE ubTileX, UBYTE ubTileY
) {
	static const BYTE bClosedToOpen = (
		VIS_TILE_DOOR_LEFT_OPEN_WALL_TOP - VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP
	);

	tInteraction *pInteraction = mapGetInteractionByIndex(ubInteractionIndex);
	if(
		g_sCurrentLevel.pTiles[ubTileX - 1][ubTileY] == TILE_DOOR_CLOSED ||
		g_sCurrentLevel.pTiles[ubTileX + 1][ubTileY] == TILE_DOOR_CLOSED
	) {
		// Horizontal door
		// Move to leftmost tile
		UBYTE ubX = ubTileX;
		while(g_sCurrentLevel.pTiles[ubX][ubTileY] == TILE_DOOR_CLOSED) {
			--ubX;
		}
		// Left wall tile
		interactionChangeOrRemoveTile(
			mapGetInteractionByTile(ubX, ubTileY),
			pInteraction, ubX, ubTileY, INTERACTION_KIND_GATE,
			TILE_WALL, TILE_WALL,
			g_sCurrentLevel.pVisTiles[ubX][ubTileY] + bClosedToOpen,
			g_sCurrentLevel.pVisTiles[ubX][ubTileY]
		);
		++ubX;

		// Middle door/bg tiles
		while(g_sCurrentLevel.pTiles[ubX][ubTileY] == TILE_DOOR_CLOSED) {
			interactionChangeOrRemoveTile(
				mapGetInteractionByTile(ubX, ubTileY),
				pInteraction, ubX, ubTileY - 1, INTERACTION_KIND_GATE,
				TILE_DOOR_OPEN, TILE_DOOR_CLOSED,
				g_sCurrentLevel.pVisTiles[ubX][ubTileY - 1] + bClosedToOpen,
				g_sCurrentLevel.pVisTiles[ubX][ubTileY - 1]
			);
			interactionChangeOrRemoveTile(
				mapGetInteractionByTile(ubX, ubTileY),
				pInteraction, ubX, ubTileY, INTERACTION_KIND_GATE,
				TILE_DOOR_OPEN, TILE_DOOR_CLOSED,
				g_sCurrentLevel.pVisTiles[ubX][ubTileY] + bClosedToOpen,
				g_sCurrentLevel.pVisTiles[ubX][ubTileY]
			);
			++ubX;
		}

		// Right wall tile
		interactionChangeOrRemoveTile(
			mapGetInteractionByTile(ubX, ubTileY),
			pInteraction, ubX, ubTileY, INTERACTION_KIND_GATE,
			TILE_WALL, TILE_WALL,
			g_sCurrentLevel.pVisTiles[ubX][ubTileY] + bClosedToOpen,
			g_sCurrentLevel.pVisTiles[ubX][ubTileY]
		);
	}
	else {
		// Vertical door
		// Move to topmost tile
		UBYTE ubY = ubTileY;
		while(g_sCurrentLevel.pTiles[ubTileX][ubY] == TILE_DOOR_CLOSED) {
			--ubY;
		}
		// Top wall tile
		interactionChangeOrRemoveTile(
			mapGetInteractionByTile(ubTileX, ubY),
			pInteraction, ubTileX, ubY, INTERACTION_KIND_GATE,
			TILE_WALL, TILE_WALL,
			g_sCurrentLevel.pVisTiles[ubTileX][ubY] + bClosedToOpen,
			g_sCurrentLevel.pVisTiles[ubTileX][ubY]
		);
		++ubY;

		// Middle door tiles
		while(g_sCurrentLevel.pTiles[ubTileX][ubY] == TILE_DOOR_CLOSED) {
			interactionChangeOrRemoveTile(
				mapGetInteractionByTile(ubTileX, ubY),
				pInteraction, ubTileX, ubY, INTERACTION_KIND_GATE,
				TILE_DOOR_OPEN, TILE_DOOR_CLOSED,
				g_sCurrentLevel.pVisTiles[ubTileX][ubY] + bClosedToOpen,
				g_sCurrentLevel.pVisTiles[ubTileX][ubY]
			);
			++ubY;
		}

		// Bottom wall tile
		interactionChangeOrRemoveTile(
			mapGetInteractionByTile(ubTileX, ubY),
			pInteraction, ubTileX, ubY, INTERACTION_KIND_GATE,
			TILE_WALL, TILE_WALL,
			g_sCurrentLevel.pVisTiles[ubTileX][ubY] + bClosedToOpen,
			g_sCurrentLevel.pVisTiles[ubTileX][ubY]
		);
	}
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

UBYTE mapTileIsOnWall(tTile eTile) {
	return (
		eTile == TILE_SLIPGATE_A ||
		eTile == TILE_SLIPGATE_B ||
		eTile == TILE_WALL_BLOCKED ||
		eTile == TILE_WALL ||
		eTile == TILE_SPIKES_OFF_FLOOR ||
		eTile == TILE_SPIKES_ON_FLOOR ||
		eTile == TILE_BUTTON_A ||
		eTile == TILE_BUTTON_B ||
		eTile == TILE_BUTTON_C ||
		eTile == TILE_BUTTON_D ||
		eTile == TILE_BUTTON_E ||
		eTile == TILE_BUTTON_F ||
		eTile == TILE_BUTTON_G ||
		eTile == TILE_BUTTON_H ||
		eTile == TILE_RECEIVER ||
		eTile == TILE_BOUNCER_SPAWNER ||
		eTile == TILE_PIPE ||
		eTile == TILE_EXIT ||
		0
	);
}

//-------------------------------------------------------------------- SLIPGATES

UBYTE mapTrySpawnSlipgate(UBYTE ubIndex, UBYTE ubTileX, UBYTE ubTileY) {
	if(!mapIsSlipgatableAt(ubTileX, ubTileY)) {
		return 0;
	}

	tSlipgate *pSlipgate = &g_pSlipgates[ubIndex];

	// TODO: this should only check for empty tile on left/right/up/down direction
	// and then do the same comparisons for x-1,x,x+1 for up/down and y for left-right
	if(
		!mapIsEmptyAt(ubTileX, ubTileY - 1) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY) &&
		mapIsSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY + 1) &&
		!mapIsEmptyAt(ubTileX, ubTileY + 2)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX;
		pSlipgate->sTilePositions[0].ubY = ubTileY;
		pSlipgate->sTilePositions[1].ubX = ubTileX;
		pSlipgate->sTilePositions[1].ubY = ubTileY + 1;
		pSlipgate->sTilePositions[2].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[2].ubY = ubTileY;
		pSlipgate->sTilePositions[3].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[3].ubY = ubTileY + 1;
		pSlipgate->eNormal = DIRECTION_LEFT;
	}
	else if(
		!mapIsEmptyAt(ubTileX, ubTileY - 2) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY) &&
		!mapIsEmptyAt(ubTileX - 1, ubTileY + 1)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX;
		pSlipgate->sTilePositions[0].ubY = ubTileY - 1;
		pSlipgate->sTilePositions[1].ubX = ubTileX;
		pSlipgate->sTilePositions[1].ubY = ubTileY;
		pSlipgate->sTilePositions[2].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[2].ubY = ubTileY - 1;
		pSlipgate->sTilePositions[3].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[3].ubY = ubTileY;
		pSlipgate->eNormal = DIRECTION_LEFT;
	}
	else if(
		!mapIsEmptyAt(ubTileX, ubTileY - 1) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY) &&
		mapIsSlipgatableAt(ubTileX, ubTileY + 1) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY + 1) &&
		!mapIsEmptyAt(ubTileX, ubTileY + 2)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX;
		pSlipgate->sTilePositions[0].ubY = ubTileY;
		pSlipgate->sTilePositions[1].ubX = ubTileX;
		pSlipgate->sTilePositions[1].ubY = ubTileY + 1;
		pSlipgate->sTilePositions[2].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[2].ubY = ubTileY;
		pSlipgate->sTilePositions[3].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[3].ubY = ubTileY + 1;
		pSlipgate->eNormal = DIRECTION_RIGHT;
	}
	else if(
		!mapIsEmptyAt(ubTileX, ubTileY - 2) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY) &&
		!mapIsEmptyAt(ubTileX + 1, ubTileY + 1)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX;
		pSlipgate->sTilePositions[0].ubY = ubTileY - 1;
		pSlipgate->sTilePositions[1].ubX = ubTileX;
		pSlipgate->sTilePositions[1].ubY = ubTileY;
		pSlipgate->sTilePositions[2].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[2].ubY = ubTileY - 1;
		pSlipgate->sTilePositions[3].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[3].ubY = ubTileY;
		pSlipgate->eNormal = DIRECTION_RIGHT;
	}
	else if(
		!mapIsEmptyAt(ubTileX - 1, ubTileY) &&
		mapIsEmptyAt(ubTileX, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY - 1) &&
		!mapIsEmptyAt(ubTileX + 2, ubTileY)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX;
		pSlipgate->sTilePositions[0].ubY = ubTileY;
		pSlipgate->sTilePositions[1].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[1].ubY = ubTileY;
		pSlipgate->sTilePositions[2].ubX = ubTileX;
		pSlipgate->sTilePositions[2].ubY = ubTileY - 1;
		pSlipgate->sTilePositions[3].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[3].ubY = ubTileY - 1;
		pSlipgate->eNormal = DIRECTION_UP;
	}
	else if(
		!mapIsEmptyAt(ubTileX - 2, ubTileY) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY - 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX, ubTileY - 1) &&
		!mapIsEmptyAt(ubTileX + 1, ubTileY)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[0].ubY = ubTileY;
		pSlipgate->sTilePositions[1].ubX = ubTileX;
		pSlipgate->sTilePositions[1].ubY = ubTileY;
		pSlipgate->sTilePositions[2].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[2].ubY = ubTileY - 1;
		pSlipgate->sTilePositions[3].ubX = ubTileX;
		pSlipgate->sTilePositions[3].ubY = ubTileY - 1;
		pSlipgate->eNormal = DIRECTION_UP;
	}
	else if(
		!mapIsEmptyAt(ubTileX - 1, ubTileY) &&
		mapIsEmptyAt(ubTileX, ubTileY + 1) &&
		mapIsSlipgatableAt(ubTileX + 1, ubTileY) &&
		mapIsEmptyAt(ubTileX + 1, ubTileY + 1) &&
		!mapIsEmptyAt(ubTileX + 2, ubTileY)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX;
		pSlipgate->sTilePositions[0].ubY = ubTileY;
		pSlipgate->sTilePositions[1].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[1].ubY = ubTileY;
		pSlipgate->sTilePositions[2].ubX = ubTileX;
		pSlipgate->sTilePositions[2].ubY = ubTileY + 1;
		pSlipgate->sTilePositions[3].ubX = ubTileX + 1;
		pSlipgate->sTilePositions[3].ubY = ubTileY + 1;
		pSlipgate->eNormal = DIRECTION_DOWN;
	}
	else if(
		!mapIsEmptyAt(ubTileX - 2, ubTileY) &&
		mapIsEmptyAt(ubTileX - 1, ubTileY + 1) &&
		mapIsSlipgatableAt(ubTileX, ubTileY) &&
		mapIsEmptyAt(ubTileX, ubTileY + 1) &&
		!mapIsEmptyAt(ubTileX + 1, ubTileY)
	) {
		mapCloseSlipgate(ubIndex);
		pSlipgate->sTilePositions[0].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[0].ubY = ubTileY;
		pSlipgate->sTilePositions[1].ubX = ubTileX;
		pSlipgate->sTilePositions[1].ubY = ubTileY;
		pSlipgate->sTilePositions[2].ubX = ubTileX - 1;
		pSlipgate->sTilePositions[2].ubY = ubTileY + 1;
		pSlipgate->sTilePositions[3].ubX = ubTileX;
		pSlipgate->sTilePositions[3].ubY = ubTileY + 1;
		pSlipgate->eNormal = DIRECTION_DOWN;
	}
	else {
		return 0;
	}

	mapTryCloseSlipgateAt(!ubIndex, pSlipgate->sTilePositions[0]);
	mapTryCloseSlipgateAt(!ubIndex, pSlipgate->sTilePositions[1]);

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
