#include "body_box.h"
#include "game.h"
#include "map.h"

static const fix16_t s_fAlmostOne = fix16_one - 1;

void bodyInit(tBodyBox *pBody, fix16_t fPosX, fix16_t fPosY) {
	pBody->fPosX = fPosX;
	pBody->fPosY = fPosY;
	pBody->fAccelerationY = fix16_one / 4;
}

void moveBodyViaSlipgate(tBodyBox *pBody, UBYTE ubIndexSrc) {
	logWrite("Slipgate!");
	switch(g_pSlipgates[ubIndexSrc].eNormal) {
		case DIRECTION_UP:
			if(g_pSlipgates[!ubIndexSrc].eNormal == DIRECTION_UP) {
				WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileX * 16) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileX * 16);
				pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
				pBody->fVelocityY = fix16_mul(pBody->fVelocityY, fix16_from_int(-1));
				// Be sure to teleport exactly at same height, otherwise U-loop will increase velocity
				UWORD wDeltaY = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileY * 16) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileY * 16);
				pBody->fPosY = fix16_add(pBody->fPosY, fix16_from_int(wDeltaY));
			}
			else {
				WORD wDeltaX = -(WORD)(g_pSlipgates[ubIndexSrc].uwTileX * 16) + (WORD)(g_pSlipgates[!ubIndexSrc].uwTileX * 16);
				pBody->fPosX = fix16_add(pBody->fPosX, fix16_from_int(wDeltaX));
				pBody->fPosY = fix16_from_int((g_pSlipgates[!ubIndexSrc].uwTileY + 1) * 16);
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
	fix16_t fNewPosX = fix16_add(pBody->fPosX, pBody->fVelocityX);
	fix16_t fNewPosY = fix16_add(pBody->fPosY, pBody->fVelocityY);
	UWORD uwTop = fix16_to_int(fix16_add(pBody->fPosY, s_fAlmostOne));
	UWORD uwBottom = uwTop + 32 - 1;
	UWORD uwLeft = fix16_to_int(pBody->fPosX);
	UWORD uwRight = uwLeft + 16 - 1;

	pBody->fPosX = fNewPosX;

	if(pBody->fVelocityY > 0) {
		if(mapIsTileSolid(uwLeft / 16, uwBottom / 16) || mapIsTileSolid(uwRight / 16, uwBottom / 16)) {
			// collide with floor
			uwTop = ((uwBottom / 16) * 16) - 32;
			fNewPosY = fix16_from_int(uwTop);
			pBody->fVelocityY = 0;
		}
		else if(mapGetTileAt(uwLeft / 16, uwBottom / 16) == TILE_SLIPGATE_1 && g_pSlipgates[0].eNormal == DIRECTION_UP) {
			// Slipgate A
			moveBodyViaSlipgate(pBody, 0);
			fNewPosY = pBody->fPosY;
		}
		else if(mapGetTileAt(uwLeft / 16, uwBottom / 16) == TILE_SLIPGATE_2 && g_pSlipgates[1].eNormal == DIRECTION_UP) {
			// Slipgate B
			moveBodyViaSlipgate(pBody, 1);
			fNewPosY = pBody->fPosY;
		}
	}
	else if(pBody->fVelocityY < 0) {
		if(mapIsTileSolid(uwLeft / 16, uwTop / 16) || mapIsTileSolid(uwRight / 16, uwTop / 16)) {
			// collide with ceil
			uwTop = ((uwTop / 16 + 1) * 16);
			fNewPosY = fix16_from_int(uwTop);
			// pBody->fVelocityY = 0;
		}
		else if(mapGetTileAt(uwLeft / 16, uwTop / 16) == TILE_SLIPGATE_1) {
			// Slipgate A
			uwTop = 256 - 16 - 32;
			fNewPosY = fix16_from_int(uwTop);
		}
		else if(mapGetTileAt(uwLeft / 16, uwTop / 16) == TILE_SLIPGATE_2) {
			// Slipgate B
			uwTop = 256 - 16 - 32;
			fNewPosY = fix16_from_int(uwTop);
		}
	}
	pBody->fPosY = fNewPosY;

	pBody->sBob.sPos.uwX = fix16_to_int(pBody->fPosX);
	pBody->sBob.sPos.uwY = fix16_to_int(pBody->fPosY);

	pBody->fVelocityY = fix16_clamp(fix16_add(pBody->fVelocityY, pBody->fAccelerationY), fix16_from_int(-15), fix16_from_int(15));
}
