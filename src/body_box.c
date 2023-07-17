/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "body_box.h"
#include "game.h"

static const fix16_t s_fVeloLimitPositive = F16(11);
static const fix16_t s_fVeloLimitNegative = F16(-11);
static const fix16_t s_fVeloClampPositive = F16(7);
static const fix16_t s_fVeloClampNegative = F16(-7);

static UBYTE bodyCheckCollision(
	tBodyBox *pBody, UBYTE ubTileX, UBYTE ubTileY,
	tDirection eBodyMovementDirection
) {
	tTile eTile = mapGetTileAt(ubTileX, ubTileY);
	return pBody->cbTileCollisionHandler(
		eTile, ubTileX, ubTileY, pBody->pHandlerData, eBodyMovementDirection
	);
}

void bodyInit(
	tBodyBox *pBody, fix16_t fPosX, fix16_t fPosY, UBYTE ubWidth, UBYTE ubHeight
) {
	pBody->fPosX = fPosX;
	pBody->fPosY = fPosY;
	pBody->ubWidth = ubWidth;
	pBody->ubHeight = ubHeight;
	pBody->fAccelerationY = fix16_one / 4; // gravity
	pBody->cbTileCollisionHandler = 0;
	pBody->cbSlipgateHandler = 0;
	pBody->bBobOffsX = 0;
}

static UBYTE moveBodyViaSlipgate(tBodyBox *pBody, UBYTE ubIndexSrc) {
	// Here be dragons

	switch(g_pSlipgates[ubIndexSrc].eNormal) {
		case DIRECTION_UP:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE);
					pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
					pBody->fVelocityY = -pBody->fVelocityY;
					// Be sure to teleport exactly at same height, otherwise U-loop will increase velocity
					UWORD wDeltaY = -(WORD)(g_pSlipgates[ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
					pBody->fPosY = fix16_add(pBody->fPosY, fix16_from_int(wDeltaY));
				} break;
				case DIRECTION_DOWN: {
					WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE);
					pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY + 1) * MAP_TILE_SIZE);
				} break;
				case DIRECTION_LEFT: {
					pBody->fVelocityX = -pBody->fVelocityY;
					pBody->fVelocityY = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE - pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fVelocityX = pBody->fVelocityY;
					pBody->fVelocityY = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE:
				case DIRECTION_COUNT:
					return 0;
			}
			break;
		case DIRECTION_DOWN:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE);
					pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY) * MAP_TILE_SIZE - pBody->ubHeight);
				} break;
				case DIRECTION_DOWN: {
					WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE);
					pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
					pBody->fVelocityY = -pBody->fVelocityY;
					// Be sure to teleport exactly at same height, otherwise U-loop will increase velocity
					UWORD wDeltaY = -(WORD)(g_pSlipgates[ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
					pBody->fPosY = fix16_add(pBody->fPosY, fix16_from_int(wDeltaY));
				} break;
				case DIRECTION_LEFT: {
					pBody->fVelocityX = pBody->fVelocityY;
					pBody->fVelocityY = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX) * MAP_TILE_SIZE - pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fVelocityX = -pBody->fVelocityY;
					pBody->fVelocityY = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE:
				case DIRECTION_COUNT:
					return 0;
			}
			break;
		case DIRECTION_LEFT:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					pBody->fVelocityY = -pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE - pBody->ubHeight);
				} break;
				case DIRECTION_DOWN: {
					pBody->fVelocityY = pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY + 1) * MAP_TILE_SIZE);
				} break;
				case DIRECTION_LEFT: {
					pBody->fVelocityX = -pBody->fVelocityX;
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE - pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE:
				case DIRECTION_COUNT:
					return 0;
			}
			break;
		case DIRECTION_RIGHT:
			switch(g_pSlipgates[!ubIndexSrc].eNormal) {
				case DIRECTION_UP: {
					pBody->fVelocityY = pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE - pBody->ubHeight);
				} break;
				case DIRECTION_DOWN: {
					pBody->fVelocityY = -pBody->fVelocityX;
					pBody->fVelocityX = 0; // faster / easier to control (?) than swapped variant
					pBody->fPosX = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY + 1) * MAP_TILE_SIZE);
				} break;
				case DIRECTION_LEFT: {
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX) * MAP_TILE_SIZE- pBody->ubWidth);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_RIGHT: {
					pBody->fVelocityX = -pBody->fVelocityX;
					pBody->fPosX = fix16_from_int((g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubX + 1) * MAP_TILE_SIZE);
					pBody->fPosY = fix16_from_int(g_pSlipgates[!ubIndexSrc].sTilePositions[0].ubY * MAP_TILE_SIZE);
				} break;
				case DIRECTION_NONE:
				case DIRECTION_COUNT:
					return 0;
			}
			break;
		case DIRECTION_NONE:
		case DIRECTION_COUNT:
			return 0;
	}
	logWrite("Slipgated!");
	return 1;
}

void bodySimulate(tBodyBox *pBody) {
	// Here be dragons
	// This function is a fucking abomination. I'm going to hell for this.

	pBody->fVelocityY = fix16_clamp(
		fix16_add(pBody->fVelocityY, pBody->fAccelerationY),
		s_fVeloLimitNegative, s_fVeloLimitPositive
	);

	// Prevents skipping tiles
	fix16_t fVeloClampedX = fix16_clamp(
		pBody->fVelocityX, s_fVeloClampNegative, s_fVeloClampPositive
	);

	fix16_t fNewPosX = fix16_add(pBody->fPosX, fVeloClampedX);
	fix16_t fNewPosY = pBody->fPosY;
	UWORD uwTop = fix16_to_int(fNewPosY);
	UWORD uwMid = uwTop + pBody->ubHeight / 2 - 1;
	UWORD uwBottom = uwTop + pBody->ubHeight - 1;
	UWORD uwLeft = fix16_to_int(fNewPosX);
	UWORD uwRight = uwLeft + pBody->ubWidth - 1;

	pBody->isOnGround = 0;
	if(fVeloClampedX > 0) {
		// moving right
		UWORD uwTileRight = (uwRight + 1) / MAP_TILE_SIZE;

		if(
			bodyCheckCollision(pBody, uwTileRight, uwTop / MAP_TILE_SIZE, DIRECTION_RIGHT) ||
			bodyCheckCollision(pBody, uwTileRight, uwMid / MAP_TILE_SIZE, DIRECTION_RIGHT) ||
			bodyCheckCollision(pBody, uwTileRight, uwBottom / MAP_TILE_SIZE, DIRECTION_RIGHT)
		) {
			// collide with wall
			fNewPosX = fix16_from_int(uwTileRight * MAP_TILE_SIZE - pBody->ubWidth);
			pBody->fVelocityX = 0;
		}
		else if(
			mapGetTileAt(uwTileRight, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_A &&
			g_pSlipgates[0].eNormal == DIRECTION_LEFT
		) {
			// Slipgate A
			if(moveBodyViaSlipgate(pBody, 0)) {
				fNewPosX = pBody->fPosX;
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with wall
				fNewPosX = fix16_from_int(uwTileRight * MAP_TILE_SIZE - pBody->ubWidth);
				pBody->fVelocityX = 0;
			}
		}
		else if(
			mapGetTileAt(uwTileRight, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_B &&
			g_pSlipgates[1].eNormal == DIRECTION_LEFT
		) {
			// Slipgate B
			if(moveBodyViaSlipgate(pBody, 1)) {
				fNewPosX = pBody->fPosX;
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with wall
				fNewPosX = fix16_from_int(uwTileRight * MAP_TILE_SIZE - pBody->ubWidth);
				pBody->fVelocityX = 0;
			}
		}
	}
	else if(fVeloClampedX < 0) {
		// moving left
		UWORD uwTileLeft = (uwLeft - 1) / MAP_TILE_SIZE;
		if(
			bodyCheckCollision(pBody, uwTileLeft, uwTop / MAP_TILE_SIZE, DIRECTION_LEFT) ||
			bodyCheckCollision(pBody, uwTileLeft, uwMid / MAP_TILE_SIZE, DIRECTION_LEFT) ||
			bodyCheckCollision(pBody, uwTileLeft, uwBottom / MAP_TILE_SIZE, DIRECTION_LEFT)
		) {
			// collide with wall
			fNewPosX = fix16_from_int((uwTileLeft + 1) * MAP_TILE_SIZE);
			pBody->fVelocityX = 0;
		}
		else if(
			mapGetTileAt(uwTileLeft, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_A &&
			g_pSlipgates[0].eNormal == DIRECTION_RIGHT
		) {
			// Slipgate A
			if(moveBodyViaSlipgate(pBody, 0)) {
				fNewPosX = pBody->fPosX;
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with wall
				fNewPosX = fix16_from_int((uwTileLeft + 1) * MAP_TILE_SIZE);
				pBody->fVelocityX = 0;
			}
		}
		else if(
			mapGetTileAt(uwTileLeft, uwBottom  / MAP_TILE_SIZE) == TILE_SLIPGATE_B &&
			g_pSlipgates[1].eNormal == DIRECTION_RIGHT
		) {
			// Slipgate B
			if(moveBodyViaSlipgate(pBody, 1)) {
				fNewPosX = pBody->fPosX;
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with wall
				fNewPosX = fix16_from_int((uwTileLeft + 1) * MAP_TILE_SIZE);
				pBody->fVelocityX = 0;
			}
		}
	}

	pBody->fPosX = fNewPosX;
	pBody->fPosY = fNewPosY;

	// Prevents skipping tiles
	fix16_t fVeloClampedY = fix16_clamp(
		pBody->fVelocityY, s_fVeloClampNegative, s_fVeloClampPositive
	);

	fNewPosY = fix16_add(fNewPosY, fVeloClampedY);

	// Helper vars could be invalid at this point and manual fine-grained
	// management of their updates is painful, so update all of them.
	uwTop = fix16_to_int(fNewPosY);
	uwMid = uwTop + pBody->ubHeight / 2 - 1;
	uwBottom = uwTop + pBody->ubHeight - 1;
	uwLeft = fix16_to_int(fNewPosX);
	uwRight = uwLeft + pBody->ubWidth - 1;

	if(fVeloClampedY > 0) {
		// falling down
		UWORD uwTileBottom = (uwBottom + 1) / MAP_TILE_SIZE;
		if(
			bodyCheckCollision(pBody, uwLeft / MAP_TILE_SIZE, uwTileBottom, DIRECTION_DOWN) ||
			bodyCheckCollision(pBody, uwRight / MAP_TILE_SIZE, uwTileBottom, DIRECTION_DOWN)
		) {
			// collide with floor
			uwTop = ((uwTileBottom) * MAP_TILE_SIZE) - pBody->ubHeight;
			fNewPosY = fix16_from_int(uwTop);
			pBody->fVelocityY = 0;
			pBody->isOnGround = 1;
			if(fVeloClampedY) {
				static fix16_t fFriction = fix16_one/2;
				if(fVeloClampedY > 0) {
					fVeloClampedY = fix16_max(fix16_sub(fVeloClampedY, fFriction), 0);
				}
				else {
					fVeloClampedY = fix16_min(fix16_add(fVeloClampedY, fFriction), 0);
				}
			}
		}
		else if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTileBottom) == TILE_SLIPGATE_A &&
			g_pSlipgates[0].eNormal == DIRECTION_UP
		) {
			// Slipgate A
			if(moveBodyViaSlipgate(pBody, 0)) {
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with floor
				uwTop = ((uwTileBottom) * MAP_TILE_SIZE) - pBody->ubHeight;
				fNewPosY = fix16_from_int(uwTop);
				pBody->fVelocityY = 0;
				pBody->isOnGround = 1;
			}
		}
		else if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTileBottom) == TILE_SLIPGATE_B &&
			g_pSlipgates[1].eNormal == DIRECTION_UP
		) {
			// Slipgate B
			if(moveBodyViaSlipgate(pBody, 1)) {
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with floor
				uwTop = ((uwTileBottom) * MAP_TILE_SIZE) - pBody->ubHeight;
				fNewPosY = fix16_from_int(uwTop);
				pBody->fVelocityY = 0;
				pBody->isOnGround = 1;
			}
		}
	}
	else if(fVeloClampedY < 0) {
		// flying up
		if(
			bodyCheckCollision(pBody, uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE, DIRECTION_UP) ||
			bodyCheckCollision(pBody, uwRight / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE, DIRECTION_UP)
		) {
			// collide with ceil
			fNewPosY = fix16_from_int((uwTop / MAP_TILE_SIZE + 1) * MAP_TILE_SIZE);
			// pBody->fVelocityY = 0;
		}
		else if(mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE) == TILE_SLIPGATE_A) {
			// Slipgate A
			if(moveBodyViaSlipgate(pBody, 0)) {
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with ceil
				fNewPosY = fix16_from_int((uwTop / MAP_TILE_SIZE + 1) * MAP_TILE_SIZE);
				// pBody->fVelocityY = 0;
			}
		}
		else if(mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE) == TILE_SLIPGATE_B) {
			// Slipgate B
			if(moveBodyViaSlipgate(pBody, 1)) {
				fNewPosY = pBody->fPosY;
				if(pBody->cbSlipgateHandler) {
					pBody->cbSlipgateHandler(pBody->pHandlerData);
				}
			}
			else {
				// collide with ceil
				fNewPosY = fix16_from_int((uwTop / MAP_TILE_SIZE + 1) * MAP_TILE_SIZE);
				// pBody->fVelocityY = 0;
			}
		}
	}

	pBody->fPosY = fNewPosY;

	pBody->sBob.sPos.uwX = fix16_to_int(pBody->fPosX) + pBody->bBobOffsX;
	pBody->sBob.sPos.uwY = fix16_to_int(pBody->fPosY);
}

void bodyTeleport(tBodyBox *pBody, UWORD uwX, UWORD uwY) {
	pBody->fPosX = fix16_from_int(uwX);
	pBody->fPosY = fix16_from_int(uwY);
}
