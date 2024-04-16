/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_TWISTER_H
#define SLIPGATES_TWISTER_H

#include <ace/managers/viewport/simplebuffer.h>

void twisterInit(UWORD *pPalette);

void twisterLoop(tSimpleBufferManager *pBfr);

#endif // SLIPGATES_TWISTER_H
