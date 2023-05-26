/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "interaction.h"
#include <ace/managers/log.h>

void interactionToggleTile(tInteraction *pInteraction, UBYTE ubTileX, UBYTE ubTileY) {
	tUbCoordYX sTileCoord = {.ubX = ubTileX, .ubY = ubTileY};
	UBYTE ubTileIndex = interactionGetTileIndex(pInteraction, ubTileX, ubTileY);
	if(ubTileIndex == INTERACTION_TILE_INDEX_INVALID) {
		// Tile not on list - add
		if(pInteraction->ubTargetCount < INTERACTION_TARGET_MAX) {
			pInteraction->pTargetTiles[pInteraction->ubTargetCount++].uwYX = sTileCoord.uwYX;
		}
		else {
			logWrite("ERR: No more space for tile in interaction %p\n", pInteraction);
		}
		return;
	}

	// Remove from list, move back all later tiles
	for(UBYTE ubNext = ubTileIndex + 1; ubNext < pInteraction->ubTargetCount; ++ubNext) {
		pInteraction->pTargetTiles[ubNext - 1].uwYX = pInteraction->pTargetTiles[ubNext].uwYX;
	}
	--pInteraction->ubTargetCount;
}

UBYTE interactionGetTileIndex(tInteraction *pInteraction, UBYTE ubTileX, UBYTE ubTileY) {
	tUbCoordYX sTileCoord = {.ubX = ubTileX, .ubY = ubTileY};
	for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
		if(pInteraction->pTargetTiles[i].uwYX == sTileCoord.uwYX) {
			return i;
		}
	}

	return INTERACTION_TILE_INDEX_INVALID;
}