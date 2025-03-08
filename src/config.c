/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "config.h"
#include <ace/managers/system.h>
#include <ace/utils/disk_file.h>

void configResetProgress(void) {
	g_sConfig.ubCurrentLevel = 1;
	g_sConfig.ubUnlockedLevels = 1;
}

void configLoad(void) {
	// Initialize with default values
	configResetProgress();

	systemUse();
	tFile *pFile = diskFileOpen("save.dat", "rb");
	if(!pFile) {
		pFile = diskFileOpen("save.bak", "rb");
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
	diskFileMove("save.dat", "save.bak");
	tFile *pFile = diskFileOpen("save.dat", "wb");
	if(pFile) {
		fileWrite(pFile, &g_sConfig.ubUnlockedLevels, sizeof(g_sConfig.ubUnlockedLevels));
		fileWrite(pFile, &g_sConfig.ubCurrentLevel, sizeof(g_sConfig.ubCurrentLevel));
		fileClose(pFile);
	}
	diskFileDelete("save.bak");
	systemUnuse();
}

tConfig g_sConfig;
