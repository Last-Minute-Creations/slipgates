/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_SLIPGATE_H
#define SLIPGATES_SLIPGATE_H

#include <ace/types.h>
#include "direction.h"
#include "tile.h"

#define SLIPGATE_A 0
#define SLIPGATE_B 1
#define SLIPGATE_AIM 2

typedef struct tSlipgate {
	tUbCoordYX sTilePositions[4]; // 0 is always top-left wall tile, 1 is other wall tile, 2-3 are bg tiles
	UBYTE isAiming;
	tDirection eNormal; // set to DIRECTION_NONE when is off
	tTile pPrevTiles[2];
} tSlipgate;

UBYTE slipgateIsOccupyingTile(const tSlipgate *pSlipgate, tUbCoordYX sPos);

#endif // SLIPGATES_SLIPGATE_H
