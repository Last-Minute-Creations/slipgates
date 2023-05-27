/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tile_tracer.h"
#include "map.h"
#include "game_math.h"
#include "game.h"

#define TRACER_SLIPGATE_SPEED 7
#define TRACER_ITERATIONS_PER_FRAME 4

void tracerInit(tTileTracer *pTracer) {
	pTracer->isActive = 0;
}

void tracerStart(
	tTileTracer *pTracer, UWORD uwSourceX, UWORD uwSourceY,
	UWORD uwTargetX, UWORD uwTargetY, UBYTE ubAngle, UBYTE ubIndex
) {
	WORD wDeltaX = uwTargetX - uwSourceX;
	WORD wDeltaY = uwTargetY - uwSourceY;

	pTracer->wDeltaTileX = (wDeltaX > 0) - (wDeltaX < 0);
	pTracer->wDeltaTileY = (wDeltaY > 0) - (wDeltaY < 0);
	UWORD uwNextTileX = uwSourceX / MAP_TILE_SIZE + pTracer->wDeltaTileX;
	UWORD uwNextTileY = uwSourceY / MAP_TILE_SIZE + pTracer->wDeltaTileY;
	pTracer->fAccumulatorX = fix16_from_int((WORD)(uwNextTileX * MAP_TILE_SIZE) - (WORD)uwSourceX);
	pTracer->fAccumulatorY = fix16_from_int((WORD)(uwNextTileY * MAP_TILE_SIZE) - (WORD)uwSourceY);

	fix16_t fSin = csin(ubAngle);
	fix16_t fCos = ccos(ubAngle);
	pTracer->fAccumulatorDeltaX = (fCos == 0) ? fix16_from_int(32767) : (fix16_div(fix16_from_int(MAP_TILE_SIZE), ccos(ubAngle)));
	pTracer->fAccumulatorDeltaY = (fSin == 0) ? fix16_from_int(32767) : (fix16_div(fix16_from_int(MAP_TILE_SIZE), csin(ubAngle)));

	pTracer->uwTileX = uwSourceX / MAP_TILE_SIZE;
	pTracer->uwTileY = uwSourceY / MAP_TILE_SIZE;
	pTracer->isActive = 1;
	pTracer->ubIndex = ubIndex;
}

void tracerProcess(tTileTracer *pTracer) {
	if(!pTracer->isActive) {
		return;
	}

	for(UBYTE i = TRACER_ITERATIONS_PER_FRAME; i--;) {
		if(fix16_abs(pTracer->fAccumulatorX) < fix16_abs(pTracer->fAccumulatorY)) {
			pTracer->fAccumulatorX = fix16_add(pTracer->fAccumulatorX, pTracer->fAccumulatorDeltaX);
			pTracer->uwTileX += pTracer->wDeltaTileX;
		} else {
			pTracer->fAccumulatorY = fix16_add(pTracer->fAccumulatorY, pTracer->fAccumulatorDeltaY);
			pTracer->uwTileY += pTracer->wDeltaTileY;
		}

		UWORD uwPosX = pTracer->uwTileX * MAP_TILE_SIZE + 3;
		UWORD uwPosY = pTracer->uwTileY * MAP_TILE_SIZE + 3;

		// Debug draw of tracer trajectory
		// blitRect(s_pBufferMain->pBack, uwPosX, uwPosY, 2, 2, 8);
		// blitRect(s_pBufferMain->pFront, uwPosX, uwPosY, 2, 2, 8);

		UWORD uwTileX = uwPosX / MAP_TILE_SIZE;
		UWORD uwTileY = uwPosY / MAP_TILE_SIZE;

		if(mapIsCollidingWithPortalProjectilesAt(uwTileX, uwTileY)) {
			pTracer->isActive = 0;
			UWORD uwSlipgateOldTile1X = g_pSlipgates[pTracer->ubIndex].uwTileX;
			UWORD uwSlipgateOldTile1Y = g_pSlipgates[pTracer->ubIndex].uwTileY;
			UWORD uwSlipgateOldTile2X = g_pSlipgates[pTracer->ubIndex].uwOtherTileX;
			UWORD uwSlipgateOldTile2Y = g_pSlipgates[pTracer->ubIndex].uwOtherTileY;
			// TODO: tile 2
			if(mapTrySpawnSlipgate(pTracer->ubIndex, uwTileX, uwTileY)) {
				gameDrawTile(uwSlipgateOldTile1X, uwSlipgateOldTile1Y);
				gameDrawTile(uwSlipgateOldTile2X, uwSlipgateOldTile2Y);
				gameDrawTile(g_pSlipgates[pTracer->ubIndex].uwTileX, g_pSlipgates[pTracer->ubIndex].uwTileY);
				gameDrawTile(g_pSlipgates[pTracer->ubIndex].uwOtherTileX, g_pSlipgates[pTracer->ubIndex].uwOtherTileY);
				break;
			}
		}
	}
}
