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
			if(g_pSlipgates[!ubIndexSrc].eNormal == DIRECTION_UP) {
				WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE);
				pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
				pBody->fVelocityY = fix16_mul(pBody->fVelocityY, fix16_from_int(-1));
				// Be sure to teleport exactly at same height, otherwise U-loop will increase velocity
				UWORD wDeltaY = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileY * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileY * MAP_TILE_SIZE);
				pBody->fPosY = fix16_add(pBody->fPosY, fix16_from_int(wDeltaY));
			}
			else {
				WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileX * MAP_TILE_SIZE) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileX * MAP_TILE_SIZE);
				pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
				pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileY + 1) * MAP_TILE_SIZE);
			}
			break;
		case DIRECTION_DOWN:
			break;
		case DIRECTION_LEFT:
			break;
		case DIRECTION_RIGHT:
			break;
		case DIRECTION_NONE:
			break;
	}
}

void bodySimulate(tBodyBox *pBody) {
	pBody->fVelocityY = fix16_clamp(fix16_add(pBody->fVelocityY, pBody->fAccelerationY), fix16_from_int(-7), fix16_from_int(7));

	fix16_t fNewPosX = fix16_add(pBody->fPosX, pBody->fVelocityX);
	fix16_t fNewPosY = fix16_add(pBody->fPosY, pBody->fVelocityY);
	UWORD uwTop = fix16_to_int(pBody->fPosY);
	UWORD uwBottom = uwTop + pBody->ubHeight;
	UWORD uwLeft = fix16_to_int(pBody->fPosX);
	UWORD uwRight = uwLeft + pBody->ubWidth - 1;

	pBody->isOnGround = 0;
	pBody->fPosX = fNewPosX;

	if(pBody->fVelocityY > 0) {
		if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwBottom / MAP_TILE_SIZE) == TILE_WALL_1 ||
			mapGetTileAt(uwRight / MAP_TILE_SIZE, uwBottom / MAP_TILE_SIZE) == TILE_WALL_1
		) {
			// collide with floor
			uwTop = ((uwBottom / MAP_TILE_SIZE) * MAP_TILE_SIZE) - pBody->ubHeight;
			fNewPosY = fix16_from_int(uwTop);
			pBody->fVelocityY = 0;
			pBody->isOnGround = 1;
		}
		else if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwBottom / MAP_TILE_SIZE) == TILE_SLIPGATE_1 &&
			g_pSlipgates[0].eNormal == DIRECTION_UP
		) {
			// Slipgate A
			moveBodyViaSlipgate(pBody, 0);
			fNewPosY = pBody->fPosY;
		}
		else if(
			mapGetTileAt(uwLeft / MAP_TILE_SIZE, uwBottom / MAP_TILE_SIZE) == TILE_SLIPGATE_2 &&
			g_pSlipgates[1].eNormal == DIRECTION_UP
		) {
			// Slipgate B
			moveBodyViaSlipgate(pBody, 1);
			fNewPosY = pBody->fPosY;
		}
	}
	else if(pBody->fVelocityY < 0) {
		if(
			mapIsTileSolid(uwLeft / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE) ||
			mapIsTileSolid(uwRight / MAP_TILE_SIZE, uwTop / MAP_TILE_SIZE)
		) {
			// collide with ceil
			uwTop = ((uwTop / MAP_TILE_SIZE + 1) * MAP_TILE_SIZE);
			fNewPosY = fix16_from_int(uwTop);
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
