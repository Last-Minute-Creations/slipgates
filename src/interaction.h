/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_INTERACTION_H
#define SLIPGATES_INTERACTION_H

#include <ace/types.h>

#define INTERACTION_TARGET_MAX 4
#define INTERACTION_TILE_INDEX_INVALID 255

typedef struct tInteraction {
	tUbCoordYX pTargetTiles[INTERACTION_TARGET_MAX];
	UBYTE ubTargetCount;
	UBYTE ubButtonMask;
	UBYTE wasActive;
} tInteraction;

void interactionToggleTile(tInteraction *pInteraction, UBYTE ubTileX, UBYTE ubTileY);

UBYTE interactionGetTileIndex(tInteraction *pInteraction, UBYTE ubTileX, UBYTE ubTileY);

#endif // SLIPGATES_INTERACTION_H
