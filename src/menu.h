/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_MENU_H
#define SLIPGATES_MENU_H

#include <ace/managers/state.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/font.h>
#include "fade.h"

extern tState g_sStateMenu;

void menuDrawBackground(void);

tSimpleBufferManager *menuGetBuffer(void);

tTextBitMap *menuGetTextBitmap(void);

tFade *menuGetFade(void);

#endif // SLIPGATES_MENU_H
