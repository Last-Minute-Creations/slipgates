/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_VFX_H
#define SLIPGATES_VFX_H

#include <ace/types.h>

void vfxInit(void);

void vfxProcess(void);

void vfxStartSlipgate(
	UBYTE ubIndexSrc, UWORD uwStartX, UWORD uwStartY, UWORD uwEndX, UWORD uwEndY
);

#endif // SLIPGATES_VFX_H
