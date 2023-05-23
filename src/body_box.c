/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "body_box.h"
#include "game.h"
#include "map.h"

void bodyInit(tBodyBox *pBody, fix16_t fPosX, fix16_t fPosY, UBYTE ubWidth, UBYTE ubHeight) {
	pBody->fPosX = fPosX;
	pBody->fPosY = fPosY;
	pBody->ubWidth = ubWidth;
	pBody->ubHeight = ubHeight;
	pBody->fAccelerationY = fix16_one / 4; // gravity
}

void moveBodyViaSlipgate(tBodyBox *pBody, UBYTE ubIndexSrc) {
	logWrite("Slipgate!");
	switch(g_pSlipgates[ubIndexSrc].eNormal) {
		case DIRECTION_UP:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE);
					pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
					pBody->fVelocityY = -pBody->fVelocityY;
					// Be sure to teleport exactly at same height, otherwise U-loop will increase velocity
					UWORD wDeltaY = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileY * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
					pBody->fPosY = fix16_add(pBody->fPosY, fix16_from_int(wDeltaY));
				} break;
				case DIRECTION_DOWN: {
					WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE);
					pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileY + 1) * MAP_TILE_SIZE);
				} break;
				case DIRECTION_LEFT: {
					pBody->fVelocityX = -pBody->fVelocityY;
					pBody->fVelocityY = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE - pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fVelocityX = pBody->fVelocityY;
					pBody->fVelocityY = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE:
					break;
			}
			break;
		case DIRECTION_DOWN:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {

				} break;
				case DIRECTION_DOWN: {

				} break;
				case DIRECTION_LEFT: {

				} break;
				case DIRECTION_RIGHT: {

				} break;
				case DIRECTION_NONE: {

				} break;
			}
			break;
		case DIRECTION_LEFT:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					pBody->fVelocityY = -pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE - pBody->ubHeight);
				} break;
				case DIRECTION_DOWN: {
					pBody->fVelocityY = pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileY + 1) * MAP_TILE_SIZE);
				} break;
				case DIRECTION_LEFT: {
					pBody->fVelocityX = -pBody->fVelocityX;
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE - pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE: {

				} break;
			}
			break;
		case DIRECTION_RIGHT:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					pBody->fVelocityY = pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE - pBody->ubHeight);
				} break;
				case DIRECTION_DOWN: {
					pBody->fVelocityY = -pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileY + 1) * MAP_TILE_SIZE);
				} break;
				case DIRECTION_LEFT: {
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileX) * MAP_TILE_SIZE- pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fVelocityX = -pBody->fVelocityX;
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE: {

				} break;
			}
			break;
		case DIRECTION_NONE:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {

				} break;
				case DIRECTION_DOWN: {

				} break;
				case DIRECTION_LEFT: {

				} break;
				case DIRECTION_RIGHT: {

				} break;
				case DIRECTION_NONE: {

				} break;
			}
			break;
	}
}

void bodySimulate(tBodyBox *pBody) {
	pBody->fVelocityY = fix16_clamp(fix16_add(pBody->fVelocityY, pBody->fAccelerationY), fix16_from_int(-7), fix16_from_int(7));

	fix16_t fNewPosX = fix16_add(pBody->fPosX, pBody->fVelocityX);
	fix16_t fNewPosY = pBody->fPosY;
	UWORD uwTop = fix16_to_int(fNewPosY);
	UWORD uwMid = uwTop + pBody->ubHeight / 2 - 1;
	UWORD uwBottom = uwTop + pBody->ubHeight - 1;
	UWORD uwLeft = fix16_to_int(fNewPosX);
	UWORD uwRight = uwLeft + pBody->ubWidth - 1;

	pBody->isOnGround = 0;
	if(pBody->fVelocityX > 0) {
		// moving right
		UWORD uwTileRight = (uwRight + 1) / MAP_TILE_SIZE;
		if(
			mapIsTileSolidForBodies(uwTileRight, uwTop / MAP_TILE_SIZE) ||
			mapIsTileSolidForBodies(uwTileRight, uwMid / MAP_TILE_SIZE) ||
			mapIsTileSolidForBodies(uwTileRight, uwBottom / MAP_TILE_SIZE)
		) {
			fNewPosX = fix16_from_int(uwTileRight * MAP_TILE_SIZE - pBody->ubWidth);
			pBody->fVelocityX = 0;
		}
		else if(
			mapGetTileAt(uwTileRight, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_1 &&
			g_pSlipgates[0].eNormal == DIRECTION_LEFT
		) {
			// Slipgate A
			moveBodyViaSlipgate(pBody, 0);
			fNewPosX = pBody->fPosX;
			fNewPosY = pBody->fPosY;
		}
		else if(
			mapGetTileAt(uwTileRight, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_2 &&
			g_pSlipgates[1].eNormal == DIRECTION_LEFT
		) {
			// Slipgate B
			moveBodyViaSlipgate(pBody, 1);
			fNewPosX = pBody->fPosX;
			fNewPosY = pBody->fPosY;
		}
	}
	else if(pBody->fVelocityX < 0) {
		// moving left
		UWORD uwTileLeft = (uwLeft - 1) / MAP_TILE_SIZE;
		if(
			mapIsTileSolidForBodies(uwTileLeft, uwTop / MAP_TILE_SIZE) ||
			mapIsTileSolidForBodies(uwTileLeft, uwMid / MAP_TILE_SIZE) ||
			mapIsTileSolidForBodies(uwTileLeft, uwBottom / MAP_TILE_SIZE)
		) {
			fNewPosX = fix16_from_int((uwTileLeft + 1) * MAP_TILE_SIZE);
			pBody->fVelocityX = 0;
		}
		else if(
			mapGetTileAt(uwTileLeft, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_1 &&
			g_pSlipgates[0].eNormal == DIRECTION_RIGHT
		) {
			// Slipgate A
			moveBodyViaSlipgate(pBody, 0);
			fNewPosX = pBody->fPosX;
			fNewPosY = pBody->fPosY;
		}
		else if(
			mapGetTileAt(uwTileLeft, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_2 &&
			g_pSlipgates[1].eNormal == DIRECTION_RIGHT
		) {
			// Slipgate B
			moveBodyViaSlipgate(pBody, 1);
			fNewPosX = pBody->fPosX;
			fNewPosY = pBody->fPosY;
		}
	}

	pBody->fPosX = fNewPosX;
	pBody->fPosY = fNewPosY;
	fNewPosY = fix16_add(fNewPosY, pBody->fVelocityY);

	// Helper vars could be invalid at this point and manual fine-grained
	// management of their updates is painful, so update all of them.
	uwTop = fix16_to_int(fNewPosY);
	uwMid = uwTop + pBody->ubHeight / 2 - 1;
	uwBottom = uwTop + pBody->ubHeight - 1;
	uwLeft = fix16_to_int(fNewPosX);
	uwRight = uwLeft + pBody->ubWidth - 1;

	if(pBody->fVelocityY > 0) {
		// falling down
		UWORD uwTileBottom = (uwBottom + 1) / MAP_TILE_SIZE;
		if(
			mapIsTileSolidForBodies(uwLeft / MAP_TILE_SIZE, uwTileBottom) ||
			mapIsTileSolidForBodies(uwRight / MAP_TILE_SIZE, uwTileBottom)
		) {
			// collide with floor
			uwTop = ((uwTileBottom) * MAP_TILE_SIZE) - pBody->ubHeight;
			fNewPosY = fix16_from_int(uwTop);
			pBody->fVelocityY = 0;
			pBody->isOnGround = 1;
		}
		else if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTileBottom) == TILE_SLIPGATE_1 &&
			g_pSlipgates[0].eNormal == DIRECTION_UP
		) {
			// Slipgate A
			moveBodyViaSlipgate(pBody, 0);
			fNewPosY = pBody->fPosY;
		}
		else if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTileBottom) == TILE_SLIPGATE_2 &&
			g_pSlipgates[1].eNormal == DIRECTION_UP
		) {
			// Slipgate B
			moveBodyViaSlipgate(pBody, 1);
			fNewPosY = pBody->fPosY;
		}
	}
	else if(pBody->fVelocityY < 0) {
		// flying up
		if(
			mapIsTileSolidForBodies(uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE) ||
			mapIsTileSolidForBodies(uwRight / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE)
		) {
			// collide with ceil
			fNewPosY = fix16_from_int((uwTop / MAP_TILE_SIZE + 1) * MAP_TILE_SIZE);
			// pBody->fVelocityY = 0;
		}
		// else if(mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE) == TILE_SLIPGATE_1) {
		// 	// Slipgate A
		// 	uwTop = 256 - MAP_TILE_SIZE - 32;
		// 	fNewPosY = fix16_from_int(uwTop);
		// }
		// else if(mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE) == TILE_SLIPGATE_2) {
		// 	// Slipgate B
		// 	uwTop = 256 - MAP_TILE_SIZE - 32;
		// 	fNewPosY = fix16_from_int(uwTop);
		// }
	}

	pBody->fPosY = fNewPosY;

	pBody->sBob.sPos.uwX = fix16_to_int(pBody->fPosX);
	pBody->sBob.sPos.uwY = fix16_to_int(pBody->fPosY);
}
