/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_CUTSCENE_H
#define SLIPGATES_CUTSCENE_H

#include <ace/managers/state.h>

void cutsceneSetup(UBYTE isOutro, tState *pNextState);

extern tState g_sStateCutscene;

#endif // SLIPGATES_CUTSCENE_H
