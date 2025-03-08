/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "slipgates.h"
#define GENERIC_MAIN_NO_TIMER
#define GENERIC_MAIN_LOOP_CONDITION g_pGameStateManager->pCurrent
#include <ace/generic/main.h>
#include <ace/managers/key.h>
#include <ace/managers/mouse.h>
#include <ace/managers/ptplayer.h>
#include <ace/contrib/managers/audio_mixer.h>
#include "logo.h"
#include "menu.h"
#include "game.h"
#include "assets.h"
#include "config.h"

tStateManager *g_pGameStateManager;

void genericCreate(void) {
	ptplayerCreate(1);
	ptplayerSetChannelsForPlayer(0b0111);
	audioMixerCreate();
	keyCreate();
	mouseCreate(MOUSE_PORT_1);
	g_pGameStateManager = stateManagerCreate();
	assetsGlobalCreate();
	configLoad();
	ptplayerLoadMod(g_pMod, 0, 0);

	// statePush(g_pGameStateManager, &g_sStateLogo);
	statePush(g_pGameStateManager, &g_sStateMenu);
	// statePush(g_pGameStateManager, &g_sStateGame);
}

void genericProcess(void) {
	keyProcess();
	mouseProcess();
	stateProcess(g_pGameStateManager);
}

void genericDestroy(void) {
	assetsGlobalDestroy();
	stateManagerDestroy(g_pGameStateManager);
	keyDestroy();
	mouseDestroy();
	audioMixerDestroy();
	ptplayerDestroy();
}
