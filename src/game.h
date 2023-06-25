/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_GAME_H
#define SLIPGATES_GAME_H

#include <ace/managers/state.h>
#include <ace/managers/viewport/simplebuffer.h>
#include "tile_tracer.h"
#include "body_box.h"
#include "player.h"

extern tState g_sStateGame;
extern tTileTracer g_sTracerSlipgate;

void gameDrawTile(UBYTE ubTileX, UBYTE ubTileY);

tUwCoordYX gameGetCrossPosition(void);

tBodyBox *gameGetBoxAt(UWORD uwX, UWORD uwY);

void gameMarkExitReached(UBYTE ubTileX, UBYTE ubTileY);

void gameDrawSlipgate(UBYTE ubIndex);

tPlayer *gameGetPlayer(void);

tSimpleBufferManager *gameGetBuffer(void);

#endif // SLIPGATES_GAME_H
