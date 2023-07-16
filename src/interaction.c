/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "interaction.h"
#include <ace/managers/log.h>

void interactionChangeOrRemoveTile(
	tInteraction *pOldInteraction, tInteraction *pInteraction,
	UBYTE ubTileX, UBYTE ubTileY,
	tInteractionKind eKind, tTile eTileActive, tTile eTileInactive,
	tVisTile eVisTileActive, tVisTile eVisTileInactive
) {
	// Remove tile from its current interaction if it has one
	if(pOldInteraction) {
		// Remove from list, move back all later tiles
		UBYTE ubTileIndex = interactionGetTileIndex(pOldInteraction, ubTileX, ubTileY);
		for(UBYTE ubNext = ubTileIndex + 1; ubNext < pOldInteraction->ubTargetCount; ++ubNext) {
			pOldInteraction->pTargetTiles[ubNext - 1] = pOldInteraction->pTargetTiles[ubNext];
		}
		--pOldInteraction->ubTargetCount;
	}

	// Prevent re-adding tile to same interaction if it was removed
	if(pOldInteraction == pInteraction) {
		return;
	}
	if(!pInteraction) {
		return;
	}

	// Add tile to new interaction
	if(pInteraction->ubTargetCount < INTERACTION_TARGET_MAX) {
		tUbCoordYX sTileCoord = {.ubX = ubTileX, .ubY = ubTileY};
		pInteraction->pTargetTiles[pInteraction->ubTargetCount].sPos.uwYX = sTileCoord.uwYX;
		pInteraction->pTargetTiles[pInteraction->ubTargetCount].eKind = eKind;
		pInteraction->pTargetTiles[pInteraction->ubTargetCount].eTileActive = eTileActive;
		pInteraction->pTargetTiles[pInteraction->ubTargetCount].eTileInactive = eTileInactive;
		pInteraction->pTargetTiles[pInteraction->ubTargetCount].eVisTileActive = eVisTileActive;
		pInteraction->pTargetTiles[pInteraction->ubTargetCount].eVisTileInactive = eVisTileInactive;
		++pInteraction->ubTargetCount;
	}
	else {
		logWrite("ERR: No more space for tile in interaction %p\n", pInteraction);
	}
}

UBYTE interactionGetTileIndex(tInteraction *pInteraction, UBYTE ubTileX, UBYTE ubTileY) {
	tUbCoordYX sTileCoord = {.ubX = ubTileX, .ubY = ubTileY};
	for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
		if(pInteraction->pTargetTiles[i].sPos.uwYX == sTileCoord.uwYX) {
			return i;
		}
	}

	return INTERACTION_TILE_INDEX_INVALID;
}
