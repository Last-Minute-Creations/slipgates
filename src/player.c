/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "player.h"
#include <ace/managers/key.h>
#include <ace/managers/mouse.h>
#include "game.h"
#include "game_math.h"
#include "assets.h"

#define PLAYER_BODY_WIDTH 8
#define PLAYER_BODY_HEIGHT 16
#define PLAYER_VELO_DELTA_X_GROUND 3
#define PLAYER_VELO_DELTA_X_AIR (fix16_one / 4)

#define PLAYER_FRAME_COUNT 4
#define PLAYER_FRAME_DIR_LEFT 0
#define PLAYER_FRAME_DIR_RIGHT 1
#define PLAYER_GRAB_RANGE 40
#define PLAYER_GRAB_VELO_MAX F16(4)

typedef struct tAnimFrameDef {
	UBYTE *pFrame;
	UBYTE *pMask;
} tAnimFrameDef;

static fix16_t s_fPlayerJumpVeloY = F16(-3);
static UBYTE s_ubFrameCooldown;
static UBYTE s_ubAnimFrame;
static tAnimFrameDef pFrameAddresses[2][PLAYER_FRAME_COUNT];

//------------------------------------------------------------------ PRIVATE FNS

static UBYTE playerCanJump(tPlayer *pPlayer) {
	return pPlayer->sBody.isOnGround;
}

static UBYTE playerCollisionHandler(
	tTile eTile, UNUSED_ARG UBYTE ubTileX, UNUSED_ARG UBYTE ubTileY, void *pData,
	tDirection eBodyMovementDirection
) {
	UBYTE isColliding = mapTileIsCollidingWithPlayers(eTile);
	if(isColliding) {
		if(mapTileIsLethal(eTile)) {
			tPlayer *pPlayer = pData;
			playerDamage(pPlayer, 100);
		}
		else if(mapTileIsExit(eTile)) {
			gameMarkExitReached();
		}
		else if(mapTileIsButton(eTile)) {
			mapPressButtonAt(ubTileX, ubTileY);
		}
		else if(
			mapTileIsActiveTurret(eTile) && eBodyMovementDirection == DIRECTION_DOWN
		) {
			mapDisableTurretAt(ubTileX, ubTileY);
		}
	}
	return isColliding;
}

static void playerDropBox(tPlayer *pPlayer) {
	pPlayer->pGrabbedBox->fAccelerationY = fix16_one / 4;// restore gravity
	pPlayer->pGrabbedBox = 0;
}

//------------------------------------------------------------------- PUBLIC FNS

void playerManagerInit(void) {
	for(UBYTE i = 0; i < PLAYER_FRAME_COUNT; ++i) {
		pFrameAddresses[PLAYER_FRAME_DIR_RIGHT][i].pFrame = bobCalcFrameAddress(g_pPlayerFrames, i * 16);
		pFrameAddresses[PLAYER_FRAME_DIR_RIGHT][i].pMask = bobCalcFrameAddress(g_pPlayerMasks, i * 16);
		pFrameAddresses[PLAYER_FRAME_DIR_LEFT][i].pFrame = bobCalcFrameAddress(g_pPlayerFrames, (PLAYER_FRAME_COUNT + i) * 16);
		pFrameAddresses[PLAYER_FRAME_DIR_LEFT][i].pMask = bobCalcFrameAddress(g_pPlayerMasks, (PLAYER_FRAME_COUNT + i) * 16);
	}
}

UBYTE playerTryShootSlipgateAt(
	tPlayer *pPlayer, UBYTE ubIndex, UBYTE ubAngle
) {
	if(g_sTracerSlipgate.isActive) {
		return 0;
	}

	tUwCoordYX sDestinationPos = gameGetCrossPosition();
	UWORD uwSourceX = fix16_to_int(pPlayer->sBody.fPosX) + pPlayer->sBody.ubWidth / 2;
	UWORD uwSourceY = fix16_to_int(pPlayer->sBody.fPosY) + pPlayer->sBody.ubHeight / 2;
	tracerStart(
		&g_sTracerSlipgate, uwSourceX, uwSourceY,
		sDestinationPos.uwX, sDestinationPos.uwY, ubAngle, ubIndex
	);

	return 1;
}

void playerReset(tPlayer *pPlayer, fix16_t fPosX, fix16_t fPosY) {
	bodyInit(&pPlayer->sBody, fPosX, fPosY, PLAYER_BODY_WIDTH, PLAYER_BODY_HEIGHT);
	pPlayer->sBody.cbTileCollisionHandler = playerCollisionHandler;
	pPlayer->sBody.pOnCollidedData = pPlayer;
	pPlayer->pGrabbedBox = 0;
	pPlayer->bHealth = 10;
	s_ubAnimFrame = 0;
	s_ubFrameCooldown = 10;
}

void playerProcess(tPlayer *pPlayer) {
	if(!pPlayer->bHealth) {
		return;
	}

	tUwCoordYX sPosCross = gameGetCrossPosition();
	UWORD uwPlayerCenterX = fix16_to_int(pPlayer->sBody.fPosX) + pPlayer->sBody.ubWidth / 2;
	UWORD uwPlayerCenterY = fix16_to_int(pPlayer->sBody.fPosY) + pPlayer->sBody.ubHeight / 2;
	UBYTE ubAimAngle = getAngleBetweenPoints(
		uwPlayerCenterX, uwPlayerCenterY, sPosCross.uwX, sPosCross.uwY
	);

	// Player's grabbed box
	if(pPlayer->pGrabbedBox) {
		UWORD uwCursorDistance = fastMagnitude(
			ABS((WORD)sPosCross.uwX - (WORD)uwPlayerCenterX),
			ABS((WORD)sPosCross.uwY - (WORD)uwPlayerCenterY)
		);
		fix16_t fHalfBoxWidth = fix16_from_int(pPlayer->pGrabbedBox->ubWidth / 2);
		fix16_t fBoxDistance = fix16_from_int(MIN(PLAYER_GRAB_RANGE,uwCursorDistance));
		fix16_t fBoxTargetX = fix16_sub(fix16_add(fix16_from_int(uwPlayerCenterX), fix16_mul(ccos(ubAimAngle), fBoxDistance)), fHalfBoxWidth);
		fix16_t fBoxTargetY = fix16_sub(fix16_add(fix16_from_int(uwPlayerCenterY), fix16_mul(csin(ubAimAngle), fBoxDistance)), fHalfBoxWidth);
		fix16_t fBoxVeloX = fix16_clamp(fix16_sub(fBoxTargetX, pPlayer->pGrabbedBox->fPosX), -PLAYER_GRAB_VELO_MAX, PLAYER_GRAB_VELO_MAX);
		fix16_t fBoxVeloY = fix16_clamp(fix16_sub(fBoxTargetY, pPlayer->pGrabbedBox->fPosY), -PLAYER_GRAB_VELO_MAX, PLAYER_GRAB_VELO_MAX);
		pPlayer->pGrabbedBox->fVelocityX = fBoxVeloX;
		pPlayer->pGrabbedBox->fVelocityY = fBoxVeloY;
	}

	// TODO: don't update after death
	UBYTE ubAnimDirection = (uwPlayerCenterX <= sPosCross.uwX) ? PLAYER_FRAME_DIR_RIGHT : PLAYER_FRAME_DIR_LEFT;

	// Player shooting slipgates
	tracerProcess(&g_sTracerSlipgate);
	if(mouseUse(MOUSE_PORT_1, MOUSE_LMB) || keyUse(KEY_Q)) {
		if(pPlayer->pGrabbedBox) {
			playerDropBox(pPlayer);
		}
		else {
			playerTryShootSlipgateAt(pPlayer, 0, ubAimAngle);
		}
	}
	else if(mouseUse(MOUSE_PORT_1, MOUSE_RMB) || keyUse(KEY_E)) {
		if(pPlayer->pGrabbedBox) {
			playerDropBox(pPlayer);
		}
		else {
			playerTryShootSlipgateAt(pPlayer, 1, ubAimAngle);
		}
	}

	// Player movement
	if(keyUse(KEY_W) && playerCanJump(pPlayer)) {
		pPlayer->sBody.fVelocityY = s_fPlayerJumpVeloY;
	}

	if(pPlayer->sBody.isOnGround) {
		pPlayer->sBody.fVelocityX = 0;
		if(keyCheck(KEY_A)) {
			pPlayer->sBody.fVelocityX = fix16_from_int(-PLAYER_VELO_DELTA_X_GROUND);
		}
		else if(keyCheck(KEY_D)) {
			pPlayer->sBody.fVelocityX = fix16_from_int(PLAYER_VELO_DELTA_X_GROUND);
		}
	}
	else {
		if(keyCheck(KEY_A)) {
			if(pPlayer->sBody.fVelocityX >= 0) {
				pPlayer->sBody.fVelocityX = fix16_sub(
					pPlayer->sBody.fVelocityX,
					PLAYER_VELO_DELTA_X_AIR
				);
			}
		}
		else if(keyCheck(KEY_D)) {
			if(pPlayer->sBody.fVelocityX <= 0) {
				pPlayer->sBody.fVelocityX = fix16_add(
					pPlayer->sBody.fVelocityX,
					PLAYER_VELO_DELTA_X_AIR
				);
			}
		}
	}

	// Player pickup
	if(keyUse(KEY_F)) {
		// TODO: generalize using collision tilemap
		if(pPlayer->pGrabbedBox) {
			playerDropBox(pPlayer);
		}
		else {
			UWORD uwCursorDistance = fastMagnitude(
				ABS((WORD)sPosCross.uwX - (WORD)uwPlayerCenterX),
				ABS((WORD)sPosCross.uwY - (WORD)uwPlayerCenterY)
			);
			if(uwCursorDistance < PLAYER_GRAB_RANGE) {
				tBodyBox *pBox = gameGetBoxAt(sPosCross.uwX, sPosCross.uwY);
				if(pBox) {
					UWORD uwBoxX = fix16_to_int(pBox->fPosX);
					UWORD uwBoxY = fix16_to_int(pBox->fPosY);
					if(
						uwBoxX <= sPosCross.uwX && sPosCross.uwX < uwBoxX + pBox->ubWidth &&
						uwBoxY <= sPosCross.uwY && sPosCross.uwY < uwBoxY + pBox->ubHeight
					) {
						pPlayer->pGrabbedBox = pBox;
						pPlayer->pGrabbedBox->fAccelerationY = 0;
					}
				}
			}
		}
	}

	if(--s_ubFrameCooldown == 0) {
		s_ubFrameCooldown = 10;
		if(++s_ubAnimFrame == PLAYER_FRAME_COUNT) {
			s_ubAnimFrame = 0;
		}

		bobSetFrame(
			&pPlayer->sBody.sBob,
			pFrameAddresses[ubAnimDirection][s_ubAnimFrame].pFrame,
			pFrameAddresses[ubAnimDirection][s_ubAnimFrame].pMask
		);
	}
}

void playerDamage(tPlayer *pPlayer, UBYTE ubAmount) {
	pPlayer->bHealth = MAX(0, pPlayer->bHealth - ubAmount);

	if(pPlayer->bHealth == 0) {
		// ded
		pPlayer->sBody.fVelocityX = 0;
	}
}
