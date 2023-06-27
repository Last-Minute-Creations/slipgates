/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_PLAYER_H
#define SLIPGATES_PLAYER_H

#include "body_box.h"

#define PLAYER_MAX_HEALTH 10

typedef struct tPlayer {
	tBodyBox sBody;
	tBodyBox *pGrabbedBox;
	BYTE bHealth;
	UBYTE isSlipgated;
	UBYTE ubDamageFrameCooldown;
	UBYTE ubRegenCooldown;
} tPlayer;

void playerManagerInit(void);

void playerReset(tPlayer *pPlayer, fix16_t fPosX, fix16_t fPosY);

void playerProcess(tPlayer *pPlayer);

UBYTE playerTryShootSlipgateAt(tPlayer *pPlayer, UBYTE ubIndex, UBYTE ubAngle);

void playerDamage(tPlayer *pPlayer, UBYTE ubAmount);

#endif // SLIPGATES_PLAYER_H
