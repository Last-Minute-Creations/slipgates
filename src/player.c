/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "player.h"
#include <ace/managers/key.h>
#include <ace/managers/mouse.h>
#include "game.h"
#include "game_math.h"

#define PLAYER_BODY_WIDTH 8
#define PLAYER_BODY_HEIGHT 16
#define PLAYER_VELO_DELTA_X_GROUND 3
#define PLAYER_VELO_DELTA_X_AIR (fix16_one / 4)

static fix16_t s_fPlayerJumpVeloY = F16(-3);

//------------------------------------------------------------------ PRIVATE FNS

static UBYTE playerCanJump(tPlayer *pPlayer) {
	return pPlayer->sBody.isOnGround;
}

//------------------------------------------------------------------- PUBLIC FNS

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
	pPlayer->pGrabbedBox = 0;
	pPlayer->ubHealth = 1;
}

void playerProcess(tPlayer *pPlayer) {
	if(!pPlayer->ubHealth) {
		return;
	}

	tUwCoordYX sPosCross = gameGetCrossPosition();
	// Player's grabbed box
	if(pPlayer->pGrabbedBox) {
		pPlayer->pGrabbedBox->fPosX = fix16_from_int(sPosCross.uwX - pPlayer->pGrabbedBox->ubWidth / 2);
		pPlayer->pGrabbedBox->fPosY = fix16_from_int(sPosCross.uwY - pPlayer->pGrabbedBox->ubHeight / 2);
	}

	// Player shooting slipgates
	UBYTE ubAimAngle = getAngleBetweenPoints(
		fix16_to_int(pPlayer->sBody.fPosX), fix16_to_int(pPlayer->sBody.fPosY),
		sPosCross.uwX, sPosCross.uwY
	);
	tracerProcess(&g_sTracerSlipgate);
	if(mouseUse(MOUSE_PORT_1, MOUSE_LMB) || keyUse(KEY_Q)) {
		playerTryShootSlipgateAt(pPlayer, 0, ubAimAngle);
	}
	else if(mouseUse(MOUSE_PORT_1, MOUSE_RMB) || keyUse(KEY_E)) {
		playerTryShootSlipgateAt(pPlayer, 1, ubAimAngle);
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
		tBodyBox *pBox = gameGetBox();
		UWORD uwBoxX = fix16_to_int(pBox->fPosX);
		UWORD uwBoxY = fix16_to_int(pBox->fPosY);
		if(pPlayer->pGrabbedBox) {
			pPlayer->pGrabbedBox = 0;
		}
		else {
			if(
				uwBoxX <= sPosCross.uwX && sPosCross.uwX < uwBoxX + pBox->ubWidth &&
				uwBoxY <= sPosCross.uwY && sPosCross.uwY < uwBoxY + pBox->ubHeight
			) {
				pPlayer->pGrabbedBox = pBox;
			}
		}
	}
}

