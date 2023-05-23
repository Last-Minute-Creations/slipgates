/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_GAME_H
#define SLIPGATES_GAME_H

#include <ace/managers/state.h>
#include "tile_tracer.h"

extern tState g_sStateGame;
extern tTileTracer g_sTracerSlipgate;

void gameDrawTile(UBYTE ubTileX, UBYTE ubTileY);

tUwCoordYX gameGetCrossPosition(void);

#endif // SLIPGATES_GAME_H
