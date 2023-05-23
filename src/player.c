/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "player.h"
#include "game.h"

#define PLAYER_BODY_WIDTH 8
#define PLAYER_BODY_HEIGHT 16

UBYTE playerCanJump(tPlayer *pPlayer) {
	return pPlayer->sBody.isOnGround;
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
	pPlayer->pGrabbedBox = 0;
}

