/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_BODY_BOX_H
#define SLIPGATES_BODY_BOX_H

#include <fixmath/fix16.h>
#include <ace/managers/bob.h>
#include "map.h"

typedef void (*tTileCollisionHandler)(
	tTile eTile, UBYTE ubTileX, UBYTE ubTileY, void *pData
);

typedef UBYTE (*tTileCollisionChecker)(tTile eTile);

typedef struct tBodyBox {
	tBob sBob;
	fix16_t fPosX;
	fix16_t fPosY;
	fix16_t fVelocityX;
	fix16_t fVelocityY;
	fix16_t fAccelerationX;
	fix16_t fAccelerationY;
	tTileCollisionHandler onCollided;
	tTileCollisionChecker cbTileCollisionCheck;
	void *pOnCollidedData;
	UBYTE ubWidth;
	UBYTE ubHeight;
	UBYTE isOnGround;
	UBYTE isAlive;
} tBodyBox;

void bodyInit(tBodyBox *pBody, fix16_t fPosX, fix16_t fPosY, UBYTE ubWidth, UBYTE ubHeight);

void bodySimulate(tBodyBox *pBody);

void bodyTeleport(tBodyBox *pBody, UWORD uwX, UWORD uwY);

#endif // SLIPGATES_BODY_BOX_H
