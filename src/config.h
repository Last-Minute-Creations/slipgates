/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_CONFIG_H
#define SLIPGATES_CONFIG_H

#include <ace/types.h>

typedef struct tConfig {
	UBYTE ubUnlockedLevels;
	UBYTE ubCurrentLevel;
} tConfig;

void configLoad(void);

void configSave(void);

void configResetProgress(void);

extern tConfig g_sConfig;

#endif // SLIPGATES_CONFIG_H
