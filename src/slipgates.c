/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "slipgates.h"
#define GENERIC_MAIN_LOOP_CONDITION g_pGameStateManager->pCurrent
#include <ace/generic/main.h>
#include <ace/managers/key.h>
#include <ace/managers/joy.h>
#include <ace/managers/mouse.h>
#include "game.h"
#include "menu.h"
#include "assets.h"

tStateManager *g_pGameStateManager;

void genericCreate(void) {
	keyCreate();
	joyOpen();
	mouseCreate(MOUSE_PORT_1);
	g_pGameStateManager = stateManagerCreate();
	assetsGlobalCreate();
	statePush(g_pGameStateManager, &g_sStateMenu);
	// statePush(g_pGameStateManager, &g_sStateGame);
}

void genericProcess(void) {
	keyProcess();
	joyProcess();
	mouseProcess();
	stateProcess(g_pGameStateManager);
}

void genericDestroy(void) {
	assetsGlobalDestroy();
	stateManagerDestroy(g_pGameStateManager);
	keyDestroy();
	joyClose();
	mouseDestroy();
}
