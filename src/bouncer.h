/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_BOUNCER_H
#define SLIPGATES_BOUNCER_H

#include "body_box.h"

#define BOUNCER_TILE_INVALID 0

void bouncerInit(UBYTE ubSpawnerTileX, UBYTE ubSpawnerTileY);

UBYTE bouncerProcess(void);

tBodyBox *bouncerGetBody();

#endif // SLIPGATES_BOUNCER_H

