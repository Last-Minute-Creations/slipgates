/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "selurina.h"
#define GENERIC_MAIN_LOOP_CONDITION g_pGameStateManager->pCurrent
#include <ace/generic/main.h>
#include <ace/managers/key.h>
#include <ace/managers/joy.h>
#include "game.h"

tStateManager *g_pGameStateManager;

void genericCreate(void) {
	keyCreate();
	joyOpen();
	g_pGameStateManager = stateManagerCreate();
	statePush(g_pGameStateManager, &g_sStateGame);
}

void genericProcess(void) {
	keyProcess();
	joyProcess();
	stateProcess(g_pGameStateManager);
}

void genericDestroy(void) {
	stateManagerDestroy(g_pGameStateManager);
	keyDestroy();
	joyClose();
}
