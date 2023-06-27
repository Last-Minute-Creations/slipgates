/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debug.h"
#include <ace/utils/custom.h>

static UBYTE s_isDebugEnabled;

void debugSetColor(UWORD uwColor) {
	if(s_isDebugEnabled) {
		g_pCustom->color[0] = uwColor;
	}
}

void debugToggle(void) {
	s_isDebugEnabled = !s_isDebugEnabled;
	if(!s_isDebugEnabled) {
		g_pCustom->color[0] = 0;
	}
}
