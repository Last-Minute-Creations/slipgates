/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "credits.h"
#include <ace/managers/key.h>
#include <ace/managers/mouse.h>
#include <ace/utils/string.h>
#include "slipgates.h"
#include "menu.h"
#include "assets.h"

//------------------------------------------------------------------------ TYPES

//----------------------------------------------------------------- PRIVATE VARS

static tSimpleBufferManager *s_pBfr;

static const char *s_pCreditsLines[] = {
	"Slipgates by Last Minute Creations",
	"lastminutecreations.itch.io/slipgates",
	"  Graphics: Softiron",
	"  Sounds and music: Luc3k",
	"  Levels: Proxy",
	"  Code: KaiN",
	"Special thanks to: Paul S, Rav.En, Stefan",
	"",
	"Game source code is available on:",
	"  github.com/Last-Minute-Creations/slipgates",
	"",
	"Used third party code and assets:",
	"  Amiga C Engine, MPL2 license <github.com/AmigaPorts/ACE>",
	"  uni05_54 font by Craig Kroeger <minimal.com/fonts>",
	"",
	"Thanks for playing!"
};
#define CREDITS_LINE_COUNT (sizeof(s_pCreditsLines) / sizeof(s_pCreditsLines[0]))

//------------------------------------------------------------------ PRIVATE FNS

static void onCreditsFadeOut(void) {
	statePop(g_pGameStateManager);
}

//------------------------------------------------------------------- PUBLIC FNS

//-------------------------------------------------------------------- GAMESTATE

static void creditsGsCreate(void) {
	s_pBfr = menuGetBuffer();
	menuDrawBackground();

	UWORD uwY = 65;
	tTextBitMap *pTextBitmap = menuGetTextBitmap();
	for(UBYTE i = 0; i < CREDITS_LINE_COUNT; ++i) {
		if(!stringIsEmpty(s_pCreditsLines[i])) {
			fontDrawStr(
				g_pFont, s_pBfr->pBack, 10, uwY, s_pCreditsLines[i], 12,
				FONT_COOKIE | FONT_SHADOW, pTextBitmap
			);
		}

		uwY += g_pFont->uwHeight + 1;
	}

	fadeStart(menuGetFade(), FADE_STATE_IN, 15, 0, 0);
}

static void creditsGsLoop(void) {
	tFade *pFade = menuGetFade();
	tFadeState eFadeState = fadeProcess(pFade);
	if(eFadeState == FADE_STATE_IDLE) {
		if(keyUse(KEY_ESCAPE) || keyUse(KEY_F) || mouseUse(MOUSE_PORT_1, MOUSE_LMB)) {
			fadeStart(pFade, FADE_STATE_OUT, 15, 0, onCreditsFadeOut);
			return;
		}
	}
	else if(eFadeState == FADE_STATE_EVENT_FIRED) {
		return;
	}

	vPortWaitForEnd(s_pBfr->sCommon.pVPort);
	viewProcessManagers(s_pBfr->sCommon.pVPort->pView);
}

static void creditsGsDestroy(void) {
}

tState g_sStateCredits = {
	.cbCreate = creditsGsCreate, .cbLoop = creditsGsLoop, .cbDestroy = creditsGsDestroy
};
