/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_INTERACTION_H
#define SLIPGATES_INTERACTION_H

#include <ace/types.h>
#include "tile.h"
#include "vis_tile.h"

#define INTERACTION_TARGET_MAX 10
#define INTERACTION_TILE_INDEX_INVALID 255

typedef enum tInteractionKind {
	INTERACTION_KIND_GATE,
	INTERACTION_KIND_SLIPGATABLE,
} tInteractionKind;

typedef struct tTogglableTile {
	tUbCoordYX sPos;
	tInteractionKind eKind;
	tTile eTileActive;
	tTile eTileInactive;
	tVisTile eVisTileActive;
	tVisTile eVisTileInactive;
} tTogglableTile;

typedef struct tInteraction {
	tTogglableTile pTargetTiles[INTERACTION_TARGET_MAX];
	UBYTE ubTargetCount;
	UWORD uwButtonMask;
	UBYTE wasActive;
} tInteraction;

void interactionChangeOrRemoveTile(
	tInteraction *pOldInteraction, tInteraction *pInteraction,
	UBYTE ubTileX, UBYTE ubTileY,
	tInteractionKind eKind, tTile eTileActive, tTile eTileInactive,
	tVisTile eVisTileActive, tVisTile eVisTileInactive
);

UBYTE interactionGetTileIndex(tInteraction *pInteraction, UBYTE ubTileX, UBYTE ubTileY);

#endif // SLIPGATES_INTERACTION_H
