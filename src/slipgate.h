/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_SLIPGATE_H
#define SLIPGATES_SLIPGATE_H

#include <ace/types.h>
#include "direction.h"

typedef struct tSlipgate {
	tUbCoordYX sTilePos; // top-left tile
	tUbCoordYX sTilePosOther; // other tile
	tDirection eNormal; // set to DIRECTION_NONE when is off
} tSlipgate;

UBYTE slipgateIsOccupyingTile(const tSlipgate *pSlipgate, tUbCoordYX sPos);

#endif // SLIPGATES_SLIPGATE_H
