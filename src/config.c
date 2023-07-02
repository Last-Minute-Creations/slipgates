/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "config.h"
#include <ace/managers/system.h>
#include <ace/utils/file.h>

void configResetProgress(void) {
	g_sConfig.ubCurrentLevel = 1;
	g_sConfig.ubUnlockedLevels = 1;
}

void configLoad(void) {
	// Initialize with default values
	configResetProgress();

	systemUse();
	tFile *pFile = fileOpen("save.dat", "rb");
	if(!pFile) {
		pFile = fileOpen("save.bak", "rb");
	}
	if(pFile) {
		fileRead(pFile, &g_sConfig.ubUnlockedLevels, sizeof(g_sConfig.ubUnlockedLevels));
		fileRead(pFile, &g_sConfig.ubCurrentLevel, sizeof(g_sConfig.ubCurrentLevel));
		fileClose(pFile);
	}
	systemUnuse();
}

void configSave(void) {
	systemUse();
	fileMove("save.dat", "save.bak");
	tFile *pFile = fileOpen("save.dat", "wb");
	if(pFile) {
		fileWrite(pFile, &g_sConfig.ubUnlockedLevels, sizeof(g_sConfig.ubUnlockedLevels));
		fileWrite(pFile, &g_sConfig.ubCurrentLevel, sizeof(g_sConfig.ubCurrentLevel));
		fileClose(pFile);
	}
	fileDelete("save.bak");
	systemUnuse();
}

tConfig g_sConfig;
