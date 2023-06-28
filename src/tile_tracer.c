/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tile_tracer.h"
#include "map.h"
#include "game_math.h"
#include "game.h"

#define TRACER_SLIPGATE_SPEED 7
#define TRACER_ITERATIONS_PER_FRAME 4
// #define DEBUG_TRACER_DRAW_TRAJECTORY

void tracerInit(tTileTracer *pTracer) {
	pTracer->isActive = 0;
}

void tracerStart(
	tTileTracer *pTracer, UWORD uwSourceX, UWORD uwSourceY,
	UWORD uwTargetX, UWORD uwTargetY, UBYTE ubIndex
) {
	WORD wDeltaX = uwTargetX - uwSourceX;
	WORD wDeltaY = uwTargetY - uwSourceY;

	UWORD uwTraversedX;
	UWORD uwTraversedY;
	if(wDeltaX > 0) {
		pTracer->wDeltaTileX = 1;
		uwTraversedX = uwSourceX - FLOOR_TO_FACTOR(uwSourceX, MAP_TILE_SIZE);
	}
	else if(wDeltaX < 0) {
		wDeltaX = -wDeltaX;
		pTracer->wDeltaTileX = -1;
		uwTraversedX = CEIL_TO_FACTOR(uwSourceX, MAP_TILE_SIZE) - uwSourceX;
	}
	else {
		pTracer->wDeltaTileX = 0;
		uwTraversedX = 0;
	}

	if(wDeltaY > 0) {
		pTracer->wDeltaTileY = 1;
		uwTraversedY = uwSourceY - FLOOR_TO_FACTOR(uwSourceY, MAP_TILE_SIZE);
	}
	else if(wDeltaY < 0) {
		wDeltaY = -wDeltaY;
		pTracer->wDeltaTileY = -1;
		uwTraversedY = CEIL_TO_FACTOR(uwSourceY, MAP_TILE_SIZE) - uwSourceY;
	}
	else {
		pTracer->wDeltaTileY = 0;
		uwTraversedY = 0;
	}

	UBYTE ubAngle = getAngleBetweenPoints(0, 0, wDeltaX, wDeltaY);
	fix16_t fSin = csin(ubAngle);
	fix16_t fCos = ccos(ubAngle);

	logWrite(
		"tracer start: %hu,%hu stop: %hu,%hu, angle: %hhu (%d)",
		uwSourceX, uwSourceY, uwTargetX, uwTargetY, ubAngle, (360 * ubAngle) / ANGLE_360
	);

	// Big enough number in case of division by zero
	static const fix16_t fInfinity = F16(32767) / 2;

	pTracer->fAccumulatorX = (fCos == 0) ? fInfinity : fix16_div(fix16_from_int(uwTraversedX), fCos); // LUT: 8k
	pTracer->fAccumulatorY = (fSin == 0) ? fInfinity : fix16_div(fix16_from_int(uwTraversedY), fSin); // LUT: 8k
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

#if defined(DEBUG_TRACER_DRAW_TRAJECTORY)
	tSimpleBufferManager *pBuffer = gameGetBuffer();
#endif

	for(UBYTE i = TRACER_ITERATIONS_PER_FRAME; i--;) {
		if(pTracer->fAccumulatorX < pTracer->fAccumulatorY) {
			pTracer->fAccumulatorX = fix16_add(pTracer->fAccumulatorX, pTracer->fAccumulatorDeltaX);
			pTracer->uwTileX += pTracer->wDeltaTileX;
		}
		else {
			pTracer->fAccumulatorY = fix16_add(pTracer->fAccumulatorY, pTracer->fAccumulatorDeltaY);
			pTracer->uwTileY += pTracer->wDeltaTileY;
		}

#if defined(DEBUG_TRACER_DRAW_TRAJECTORY)
		UWORD uwPosX = pTracer->uwTileX * MAP_TILE_SIZE + 3;
		UWORD uwPosY = pTracer->uwTileY * MAP_TILE_SIZE + 3;
		blitRect(pBuffer->pBack, uwPosX, uwPosY, 2, 2, 5);
		blitRect(pBuffer->pFront, uwPosX, uwPosY, 2, 2, 5);
#endif

		if(mapIsCollidingWithPortalProjectilesAt(pTracer->uwTileX, pTracer->uwTileY)) {
			pTracer->isActive = 0;
			mapTrySpawnSlipgate(pTracer->ubIndex, pTracer->uwTileX, pTracer->uwTileY);
			break;
		}
	}
}
