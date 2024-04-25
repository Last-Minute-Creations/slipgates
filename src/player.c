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
#define PLAYER_VELO_DELTA_X_AIR (fix16_one / 16)
#define PLAYER_VELO_DELTA_X_GROUND (PLAYER_VELO_DELTA_X_AIR * 40)
#define PLAYER_ACCELERATION_X_GROUND (PLAYER_VELO_DELTA_X_GROUND / 10)

#define PLAYER_FRAME_COUNT 4
#define PLAYER_FRAME_COOLDOWN 10
#define PLAYER_FRAME_DIR_LEFT 0
#define PLAYER_FRAME_DIR_RIGHT 1
#define PLAYER_GRAB_RANGE 40
#define PLAYER_GRAB_VELO_MAX F16(4)

#define PLAYER_DAMAGE_COOLDOWN 5
#define PLAYER_REGEN_COOLDOWN 20

typedef struct tAnimFrameDef {
	UBYTE *pFrame;
	UBYTE *pMask;
} tAnimFrameDef;

static fix16_t s_fPlayerJumpVeloY = F16(-3);
static UBYTE s_ubFrameCooldown;
static UBYTE s_ubAnimFrame;
static tAnimFrameDef s_pBodyFrameAddresses[2][PLAYER_FRAME_COUNT];
static tAnimFrameDef s_pArmFrameAddresses[GAME_MATH_ANGLE_COUNT / 4];

//------------------------------------------------------------------ PRIVATE FNS

static UBYTE playerCanJump(tPlayer *pPlayer) {
	return pPlayer->sBody.isOnGround;
}

static UBYTE playerCollisionHandler(
	tTile eTile, UBYTE ubTileX, UBYTE ubTileY, void *pData,
	tDirection eBodyMovementDirection
) {
	UBYTE isColliding = mapTileIsCollidingWithPlayers(eTile);
	if(isColliding) {
		if(mapTileIsLethal(eTile)) {
			tPlayer *pPlayer = pData;
			playerDamage(pPlayer, PLAYER_MAX_HEALTH);
		}
		else if(mapTileIsExit(eTile)) {
			UBYTE isHub = (eTile == TILE_EXIT_HUB);
			gameMarkExitReached(ubTileX, ubTileY, isHub);
		}
		else if(mapTileIsButton(eTile)) {
			mapPressButtonAt(ubTileX, ubTileY);
		}
	}
	return isColliding;
}

static void playerSlipgateHandler(void *pData) {
	tPlayer *pPlayer = pData;
	pPlayer->isSlipgated = 1;
}

static void playerDropBox(tPlayer *pPlayer) {
	pPlayer->pGrabbedBox->fAccelerationY = fix16_one / 4;// restore gravity
	pPlayer->pGrabbedBox->isSlipgatable = 1;
	pPlayer->pGrabbedBox = 0;
}

static UBYTE playerTryShootSlipgate(tPlayer *pPlayer, UBYTE ubIndex) {
	if(g_sTracerSlipgate.isActive) {
		return 0;
	}

	tUwCoordYX sDestinationPos = gameGetCrossPosition();
	UWORD uwSourceX = fix16_to_int(pPlayer->sBody.fPosX) + pPlayer->sBody.ubWidth / 2;
	UWORD uwSourceY = fix16_to_int(pPlayer->sBody.fPosY) + pPlayer->sBody.ubHeight / 2;
	tracerStart(
		&g_sTracerSlipgate, uwSourceX, uwSourceY,
		sDestinationPos.uwX, sDestinationPos.uwY, ubIndex
	);

	return 1;
}

//------------------------------------------------------------------- PUBLIC FNS

void playerManagerInit(void) {
	for(UBYTE i = 0; i < PLAYER_FRAME_COUNT; ++i) {
		s_pBodyFrameAddresses[PLAYER_FRAME_DIR_RIGHT][i].pFrame = bobCalcFrameAddress(g_pPlayerFrames, i * 16);
		s_pBodyFrameAddresses[PLAYER_FRAME_DIR_RIGHT][i].pMask = bobCalcFrameAddress(g_pPlayerMasks, i * 16);
		s_pBodyFrameAddresses[PLAYER_FRAME_DIR_LEFT][i].pFrame = bobCalcFrameAddress(g_pPlayerFrames, (PLAYER_FRAME_COUNT + i) * 16);
		s_pBodyFrameAddresses[PLAYER_FRAME_DIR_LEFT][i].pMask = bobCalcFrameAddress(g_pPlayerMasks, (PLAYER_FRAME_COUNT + i) * 16);
	}
	for(UBYTE ubAngle = ANGLE_0; ubAngle < ANGLE_360; ++ubAngle) {
		UBYTE ubArmFrame = ubAngle / 4;
		s_pArmFrameAddresses[ubArmFrame].pFrame = bobCalcFrameAddress(g_pArmFrames, 16 * ubArmFrame);
		s_pArmFrameAddresses[ubArmFrame].pMask = bobCalcFrameAddress(g_pArmMasks, 16 * ubArmFrame);
	}
}

void playerReset(tPlayer *pPlayer, fix16_t fPosX, fix16_t fPosY) {
	bodyInit(&pPlayer->sBody, fPosX, fPosY, PLAYER_BODY_WIDTH, PLAYER_BODY_HEIGHT);
	bobInit(&pPlayer->sBobArm, 16, 16, 0, g_pArmFrames->Planes[0], g_pArmMasks->Planes[0], 0, 0);
	pPlayer->sBody.bBobOffsX = -4;
	pPlayer->sBody.cbTileCollisionHandler = playerCollisionHandler;
	pPlayer->sBody.cbSlipgateHandler = playerSlipgateHandler;
	pPlayer->sBody.pHandlerData = pPlayer;
	pPlayer->pGrabbedBox = 0;
	pPlayer->bHealth = PLAYER_MAX_HEALTH;
	pPlayer->ubDamageFrameCooldown = 0;
	pPlayer->ubRegenCooldown = PLAYER_REGEN_COOLDOWN;
	pPlayer->ubAnimDirection = 0;
	s_ubAnimFrame = 0;
	s_ubFrameCooldown = PLAYER_FRAME_COOLDOWN;
}

void playerProcess(tPlayer *pPlayer) {
	if(!pPlayer->bHealth) {
		return;
	}

	if(--pPlayer->ubRegenCooldown == 0) {
		pPlayer->ubRegenCooldown = PLAYER_REGEN_COOLDOWN;
		pPlayer->bHealth = MIN(pPlayer->bHealth + 1, PLAYER_MAX_HEALTH);
	}

	tUwCoordYX sPosCross = gameGetCrossPosition();
	UWORD uwPlayerCenterX = fix16_to_int(pPlayer->sBody.fPosX) + pPlayer->sBody.ubWidth / 2;
	UWORD uwPlayerCenterY = fix16_to_int(pPlayer->sBody.fPosY) + pPlayer->sBody.ubHeight / 2;
	UBYTE ubAimAngle = getAngleBetweenPoints(
		uwPlayerCenterX, uwPlayerCenterY, sPosCross.uwX, sPosCross.uwY
	);

	if(pPlayer->isSlipgated) {
		if(pPlayer->pGrabbedBox) {
			fix16_t fHalfBoxWidth = fix16_from_int(pPlayer->pGrabbedBox->ubWidth / 2);
			pPlayer->pGrabbedBox->fPosX = fix16_sub(fix16_from_int(uwPlayerCenterX), fHalfBoxWidth);
			pPlayer->pGrabbedBox->fPosY = fix16_sub(fix16_from_int(uwPlayerCenterY), fHalfBoxWidth);
		}
		pPlayer->isSlipgated = 0;
	}

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
	pPlayer->ubAnimDirection = (uwPlayerCenterX <= sPosCross.uwX) ? PLAYER_FRAME_DIR_RIGHT : PLAYER_FRAME_DIR_LEFT;

	// Player shooting slipgates
	tracerProcess(&g_sTracerSlipgate);
	playerTryShootSlipgate(pPlayer, SLIPGATE_AIM);
	if(mouseUse(MOUSE_PORT_1, MOUSE_LMB) || keyUse(KEY_Q)) {
		if(pPlayer->pGrabbedBox) {
			playerDropBox(pPlayer);
		}
		else {
			if(g_sTracerSlipgate.ubIndex == SLIPGATE_AIM) {
				g_sTracerSlipgate.isActive = 0;
			}
			playerTryShootSlipgate(pPlayer, 0);
		}
	}
	else if(mouseUse(MOUSE_PORT_1, MOUSE_RMB) || keyUse(KEY_E)) {
		if(pPlayer->pGrabbedBox) {
			playerDropBox(pPlayer);
		}
		else {
			if(g_sTracerSlipgate.ubIndex == SLIPGATE_AIM) {
				g_sTracerSlipgate.isActive = 0;
			}
			playerTryShootSlipgate(pPlayer, 1);
		}
	}

	// Player movement
	if(keyUse(KEY_W) && playerCanJump(pPlayer)) {
		pPlayer->sBody.fVelocityY = s_fPlayerJumpVeloY;
	}

	if(pPlayer->sBody.isOnGround) {
		if(keyCheck(KEY_A)) {
			if(pPlayer->sBody.fVelocityX > 0) {
				pPlayer->sBody.fVelocityX = 0;
			}
			else if(pPlayer->sBody.fVelocityX > -PLAYER_VELO_DELTA_X_GROUND) {
				pPlayer->sBody.fVelocityX = fix16_sub(pPlayer->sBody.fVelocityX, PLAYER_ACCELERATION_X_GROUND);
			}
		}
		else if(keyCheck(KEY_D)) {
			if(pPlayer->sBody.fVelocityX < 0) {
				pPlayer->sBody.fVelocityX = 0;
			}
			else if(pPlayer->sBody.fVelocityX < PLAYER_VELO_DELTA_X_GROUND) {
				pPlayer->sBody.fVelocityX = fix16_add(pPlayer->sBody.fVelocityX, PLAYER_ACCELERATION_X_GROUND);
			}
		}
		else {
			pPlayer->sBody.fVelocityX = 0;
		}
	}
	else {
		if(keyCheck(KEY_A)) {
			if(pPlayer->sBody.fVelocityX > -PLAYER_VELO_DELTA_X_GROUND) {
				pPlayer->sBody.fVelocityX = fix16_sub(
					pPlayer->sBody.fVelocityX,
					PLAYER_VELO_DELTA_X_AIR
				);
			}
		}
		else if(keyCheck(KEY_D)) {
			if(pPlayer->sBody.fVelocityX < PLAYER_VELO_DELTA_X_GROUND) {
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
						pPlayer->pGrabbedBox->isSlipgatable = 0;
						pPlayer->pGrabbedBox->fAccelerationY = 0;
					}
				}
			}
		}
	}

	UBYTE isUpdateFrame = 0;
	if(pPlayer->ubDamageFrameCooldown) {
		--pPlayer->ubDamageFrameCooldown;
		isUpdateFrame = 1;
	}

	if(--s_ubFrameCooldown == 0) {
		s_ubFrameCooldown = PLAYER_FRAME_COOLDOWN;
		if(++s_ubAnimFrame == PLAYER_FRAME_COUNT) {
			s_ubAnimFrame = 0;
		}
		isUpdateFrame = 1;
	}

	if(isUpdateFrame) {
		UBYTE *pFrameData;
		if(pPlayer->ubDamageFrameCooldown) {
			pFrameData = g_pPlayerWhiteFrame->Planes[0];
		}
		else {
			pFrameData = s_pBodyFrameAddresses[pPlayer->ubAnimDirection][0].pFrame;
			// pFrameData = s_pBodyFrameAddresses[pPlayer->ubAnimDirection][s_ubAnimFrame].pFrame;
		}
		bobSetFrame(
			&pPlayer->sBody.sBob,
			pFrameData,
			s_pBodyFrameAddresses[pPlayer->ubAnimDirection][0].pMask
			// s_pBodyFrameAddresses[pPlayer->ubAnimDirection][s_ubAnimFrame].pMask
		);
	}

	UBYTE ubArmFrame = ubAimAngle / 4;
	bobSetFrame(
		&pPlayer->sBobArm,
		s_pArmFrameAddresses[ubArmFrame].pFrame,
		s_pArmFrameAddresses[ubArmFrame].pMask
	);
}

void playerProcessArm(tPlayer *pPlayer) {
	pPlayer->sBobArm.sPos.uwX = pPlayer->sBody.sBob.sPos.uwX;
	if(pPlayer->ubAnimDirection == PLAYER_FRAME_DIR_LEFT) {
		pPlayer->sBobArm.sPos.uwX += 1;
	}
	pPlayer->sBobArm.sPos.uwY = pPlayer->sBody.sBob.sPos.uwY - 2;
}

void playerDamage(tPlayer *pPlayer, UBYTE ubAmount) {
	pPlayer->bHealth = MAX(0, pPlayer->bHealth - ubAmount);
	pPlayer->ubDamageFrameCooldown = PLAYER_DAMAGE_COOLDOWN;
	pPlayer->ubRegenCooldown = PLAYER_REGEN_COOLDOWN;

	UBYTE *pFrameData = g_pPlayerWhiteFrame->Planes[0];
	pPlayer->sBody.sBob.pFrameData = pFrameData;

	if(pPlayer->bHealth == 0) {
		// ded
		pPlayer->sBody.fVelocityX = 0;
	}
}
