/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_TILE_TRACER_H
#define SLIPGATES_TILE_TRACER_H

#include <fixmath/fix16.h>
#include <ace/types.h>

// Based on http://www.cse.yorku.ca/~amana/research/grid.pdf
typedef struct tTileTracer {
	fix16_t fAccumulatorX; // Helper accumulator for X direction.
	fix16_t fAccumulatorY; // Helper accumulator for Y direction.
	fix16_t fAccumulatorDeltaX;
	fix16_t fAccumulatorDeltaY;
	UWORD uwTileX; // Current tracer pos, in X direction.
	UWORD uwTileY; // Current tracer pos, in Y direction.
	WORD wDeltaTileX;
	WORD wDeltaTileY;
	UBYTE isActive;
	UBYTE ubIndex; // TODO: callback + data
} tTileTracer;

void tracerInit(tTileTracer *pTracer);

// angle passed separately to prevent duplicating atan2 calc
void tracerStart(
	tTileTracer *pTracer, UWORD uwSourceX, UWORD uwSourceY,
	UWORD uwTargetX, UWORD uwTargetY, UBYTE ubAngle, UBYTE ubIndex
);

void tracerProcess(tTileTracer *pTracer);

#endif // SLIPGATES_TILE_TRACER_H
