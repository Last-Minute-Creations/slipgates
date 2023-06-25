/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "slipgate.h"

UBYTE slipgateIsOccupyingTile(const tSlipgate *pSlipgate, tUbCoordYX sPos) {
	return (
		pSlipgate->sTilePositions[0].uwYX == sPos.uwYX ||
		pSlipgate->sTilePositions[1].uwYX == sPos.uwYX
	);
}
