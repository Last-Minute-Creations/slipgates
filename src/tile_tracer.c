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
	fix16_t fSin = csin(ubAngle);
	fix16_t fCos = ccos(ubAngle);

	// Big enough number in case of division by zero
	static const fix16_t fInfinity = F16(32767) / 2;

	pTracer->fAccumulatorX = (fCos == 0) ? fInfinity : fix16_div(fix16_from_int((WORD)(uwNextTileX * MAP_TILE_SIZE) - (WORD)uwSourceX), fCos); // LUT: 8k
	pTracer->fAccumulatorY = (fSin == 0) ? fInfinity : fix16_div(fix16_from_int((WORD)(uwNextTileY * MAP_TILE_SIZE) - (WORD)uwSourceY), fSin); // LUT: 8k
	pTracer->fAccumulatorDeltaX = (fCos == 0) ? fInfinity : fix16_div(fix16_from_int(MAP_TILE_SIZE), fCos); // LUT: 512
	pTracer->fAccumulatorDeltaY = (fSin == 0) ? fInfinity : fix16_div(fix16_from_int(MAP_TILE_SIZE), fSin); // LUT: 512

	pTracer->uwTileX = uwSourceX / MAP_TILE_SIZE;
	pTracer->uwTileY = uwSourceY / MAP_TILE_SIZE;
	pTracer->isActive = 1;
	pTracer->ubIndex = ubIndex;
}

void tracerProcess(tTileTracer *pTracer) {
	if(!pTracer->isActive) {
		return;
	}

	// tSimpleBufferManager *pBuffer = gameGetBuffer();

	for(UBYTE i = TRACER_ITERATIONS_PER_FRAME; i--;) {
		if(fix16_abs(pTracer->fAccumulatorX) < fix16_abs(pTracer->fAccumulatorY)) {
			pTracer->fAccumulatorX = fix16_add(pTracer->fAccumulatorX, pTracer->fAccumulatorDeltaX);
			pTracer->uwTileX += pTracer->wDeltaTileX;
		}
		else {
			pTracer->fAccumulatorY = fix16_add(pTracer->fAccumulatorY, pTracer->fAccumulatorDeltaY);
			pTracer->uwTileY += pTracer->wDeltaTileY;
		}

		// Debug draw of tracer trajectory
		// UWORD uwPosX = pTracer->uwTileX * MAP_TILE_SIZE + 3;
		// UWORD uwPosY = pTracer->uwTileY * MAP_TILE_SIZE + 3;
		// blitRect(pBuffer->pBack, uwPosX, uwPosY, 2, 2, 8);
		// blitRect(pBuffer->pFront, uwPosX, uwPosY, 2, 2, 8);

		if(mapIsCollidingWithPortalProjectilesAt(pTracer->uwTileX, pTracer->uwTileY)) {
			pTracer->isActive = 0;
			mapTrySpawnSlipgate(pTracer->ubIndex, pTracer->uwTileX, pTracer->uwTileY);
			break;
		}
	}
}
