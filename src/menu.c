/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "menu.h"
#include <ace/generic/screen.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/key.h>
#include <ace/managers/mouse.h>
#include <ace/managers/system.h>
#include <ace/managers/blit.h>
#include <ace/managers/sprite.h>
#include <ace/utils/palette.h>
#include "slipgates.h"
#include "assets.h"
#include "game.h"
#include "credits.h"
#include "cutscene.h"

//------------------------------------------------------------------------ TYPES

typedef void (*tMenuOptionCallback)(void);

typedef struct tMenuOption {
	const char *szLabel;
	tMenuOptionCallback cbOnClicked;
	tUwAbsRect sRect;
	UBYTE isHovered;
	UBYTE isEnabled;
} tMenuOption;

typedef enum tMenuExit {
	MENU_EXIT_NONE,
	MENU_EXIT_NEW_GAME,
	MENU_EXIT_CONTINUE,
	MENU_EXIT_CREDITS,
	MENU_EXIT_WORKBENCH,
} tMenuExit;

//----------------------------------------------------------------- PRIVATE VARS

static tView *s_pView;
static tVPort *s_pVp;
static tSimpleBufferManager *s_pBfr;
static tBitMap *s_pBg;
static tTextBitMap *s_pTextBuffer;
static tMenuExit s_eMenuExit;
static tSprite *s_pSpriteCrosshair;
static UWORD s_pPalette[32];
static tFade *s_pFade;

static tMenuOption s_pOptions[5];
static UBYTE s_ubOptionCount;

//------------------------------------------------------------------ PRIVATE FNS

static void menuProcessExit(void) {
	switch(s_eMenuExit) {
		case MENU_EXIT_NEW_GAME:
			// gamePrepareNew();
			cutsceneSetup(0, &g_sStateGame);
			stateChange(g_pGameStateManager, &g_sStateCutscene);
			break;
		case MENU_EXIT_CONTINUE:
			// gamePrepareContinue();
			stateChange(g_pGameStateManager, &g_sStateGame);
			break;
		case MENU_EXIT_CREDITS:
			statePush(g_pGameStateManager, &g_sStateCredits);
			break;
		case MENU_EXIT_WORKBENCH:
			statePop(g_pGameStateManager);
			break;
		default:
			return;
	}

	s_eMenuExit = MENU_EXIT_NONE;
}

static void onFadeOut(void) {
	menuProcessExit();
}

static void onNewGame(void) {
	s_eMenuExit = MENU_EXIT_NEW_GAME;
	fadeStart(s_pFade, FADE_STATE_OUT, 15, 0, onFadeOut);
}

static void onContinue(void) {
	s_eMenuExit = MENU_EXIT_CONTINUE;
	fadeStart(s_pFade, FADE_STATE_OUT, 15, 0, onFadeOut);
}

static void onCredits(void) {
	s_eMenuExit = MENU_EXIT_CREDITS;
	fadeStart(s_pFade, FADE_STATE_OUT, 15, 0, onFadeOut);
}

static void onExit(void) {
	s_eMenuExit = MENU_EXIT_WORKBENCH;
	fadeStart(s_pFade, FADE_STATE_OUT, 15, 0, onFadeOut);
}

static void menuClearOptions(void) {
	s_ubOptionCount = 0;
}

static void menuAddOption(
	const char *szLabel, UBYTE isEnabled, tMenuOptionCallback cbOnClicked
) {
	UWORD uwTop = 150 + s_ubOptionCount * 16;
	UWORD uwWidth = fontMeasureText(g_pFont, szLabel).uwX;
	UWORD uwLeft = (SCREEN_PAL_WIDTH - uwWidth) / 2;
	s_pOptions[s_ubOptionCount++] = (tMenuOption) {
		.szLabel = szLabel,
		.cbOnClicked = cbOnClicked,
		.sRect = {
			.uwX1 = uwLeft,
			.uwY1 = uwTop,
			.uwX2 = uwLeft + uwWidth,
			.uwY2 = uwTop + g_pFont->uwHeight,
		},
		.isEnabled = isEnabled
	};
}

static void menuDrawPos(UBYTE ubIndex) {
	tMenuOption *pOption = &s_pOptions[ubIndex];

	UBYTE ubColor = 15;
	if(!pOption->isEnabled) {
		ubColor = 4;
	}
	else if(pOption->isHovered) {
		ubColor = 8;
	}

	fontDrawStr(
		g_pFont, s_pBfr->pBack, pOption->sRect.uwX1, pOption->sRect.uwY1,
		pOption->szLabel, ubColor, FONT_COOKIE, s_pTextBuffer
	);
}

static void menuDraw(void) {
	for(UBYTE i = 0; i < s_ubOptionCount; ++i) {
		menuDrawPos(i);
	}
}

static void menuProcess(UWORD uwMouseX, UWORD uwMouseY) {

	for(UBYTE i = 0; i < s_ubOptionCount; ++i) {
		tMenuOption *pOption = &s_pOptions[i];
		UBYTE wasHovered = pOption->isHovered;
		pOption->isHovered = 0;
		if(pOption->isEnabled) {
			if(
				pOption->sRect.uwX1 <= uwMouseX && uwMouseX <= pOption->sRect.uwX2 &&
				pOption->sRect.uwY1 <= uwMouseY && uwMouseY <= pOption->sRect.uwY2
			) {
				pOption->isHovered = 1;
			}

			if(pOption->isHovered != wasHovered) {
				menuDrawPos(i);
			}

			if(
				pOption->isHovered && mouseUse(MOUSE_PORT_1, MOUSE_LMB) &&
				pOption->cbOnClicked
			) {
				pOption->cbOnClicked();
			}
		}
	}
}

static void menuRedrawAll(void) {
	menuDrawBackground();
	menuDraw();

	static const char szVersion[15] = "V." GAME_VERSION;
	fontDrawStr(
		g_pFont, s_pBfr->pBack, 320, 50, szVersion,
		12, FONT_RIGHT | FONT_COOKIE, s_pTextBuffer
	);

	fontDrawStr(
		g_pFont, s_pBfr->pBack, SCREEN_PAL_WIDTH / 2, SCREEN_PAL_HEIGHT,
		"A game by Last Minute Creations",
		12, FONT_COOKIE | FONT_HCENTER | FONT_BOTTOM, s_pTextBuffer
	);
}

//------------------------------------------------------------------- PUBLIC FNS

void menuDrawBackground(void) {
	blitCopyAligned(s_pBg, 0, 0, s_pBfr->pBack, 0, 0, SCREEN_PAL_WIDTH, SCREEN_PAL_HEIGHT  / 2);
	blitCopyAligned(
		s_pBg, 0, SCREEN_PAL_HEIGHT  / 2, s_pBfr->pBack, 0, SCREEN_PAL_HEIGHT  / 2,
		SCREEN_PAL_WIDTH, SCREEN_PAL_HEIGHT  / 2
	);
}

tSimpleBufferManager *menuGetBuffer(void) {
	return s_pBfr;
}

tTextBitMap *menuGetTextBitmap(void) {
	return s_pTextBuffer;
}

tFade *menuGetFade(void) {
	return s_pFade;
}

//-------------------------------------------------------------------- GAMESTATE

static void menuGsCreate(void) {
	s_pView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
		TAG_VIEW_COPLIST_MODE, COPPER_MODE_BLOCK,
	TAG_END);

	s_pVp = vPortCreate(0,
		TAG_VPORT_VIEW, s_pView,
		TAG_VPORT_BPP, 5,
	TAG_END);

	s_pBfr = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pVp,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_INTERLEAVED,
		TAG_SIMPLEBUFFER_USE_X_SCROLLING, 0,
		TAG_SIMPLEBUFFER_IS_DBLBUF, 0,
	TAG_END);

	paletteLoad("data/slipgates.plt", s_pPalette, 32);
	s_pPalette[17] = 0xA86;
	s_pPalette[18] = 0x27D;
	s_pPalette[19] = 0xE96;
	s_pFade = fadeCreate(s_pView, s_pPalette, 32);

	s_pBg = bitmapCreateFromFile("data/menu_bg.bm", 0);

	s_pTextBuffer = fontCreateTextBitMap(336, g_pFont->uwHeight);

	spriteManagerCreate(s_pView, 0);
	s_pSpriteCrosshair = spriteAdd(0, g_pBmCursor);
	systemSetDmaBit(DMAB_SPRITE, 1);
	spriteProcessChannel(0);
	mouseSetBounds(MOUSE_PORT_1, 7, 14, SCREEN_PAL_WIDTH - 8, SCREEN_PAL_HEIGHT - 13);

	systemUnuse();

	s_eMenuExit = MENU_EXIT_NONE;

	menuClearOptions();
	UBYTE isContinueEnabled = 0;
	menuAddOption("NEW GAME", 1, onNewGame);
	menuAddOption("CONTINUE", isContinueEnabled, onContinue);
	menuAddOption("CREDITS", 1, onCredits);
	menuAddOption("EXIT", 1, onExit);

	menuRedrawAll();
	fadeStart(s_pFade, FADE_STATE_IN, 15, 0, 0);
	viewLoad(s_pView);
}

static void menuGsLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}

	if(fadeProcess(s_pFade) == FADE_STATE_EVENT_FIRED) {
		return;
	}

	UWORD uwMouseX = mouseGetX(MOUSE_PORT_1);
	UWORD uwMouseY = mouseGetY(MOUSE_PORT_1);
	s_pSpriteCrosshair->wX = uwMouseX - 8;
	s_pSpriteCrosshair->wY = uwMouseY - 14;
	spriteProcess(s_pSpriteCrosshair);

	menuProcess(uwMouseX, uwMouseY);

	vPortWaitForEnd(s_pVp);
	viewProcessManagers(s_pView);
}

static void menuGsDestroy(void) {
	viewLoad(0);
	systemSetDmaBit(DMAB_SPRITE, 0);

	systemUse();

	fadeDestroy(s_pFade);
	spriteManagerDestroy();
	fontDestroyTextBitMap(s_pTextBuffer);
	bitmapDestroy(s_pBg);
	viewDestroy(s_pView);
}

static void menuGsResume(void) {
	menuRedrawAll();
	systemSetDmaBit(DMAB_SPRITE, 1);
	fadeStart(s_pFade, FADE_STATE_IN, 15, 0, 0);
}

static void menuGsSuspend(void) {
	systemSetDmaBit(DMAB_SPRITE, 0);
}

tState g_sStateMenu = {
	.cbCreate = menuGsCreate, .cbLoop = menuGsLoop, .cbDestroy = menuGsDestroy,
	.cbResume = menuGsResume, .cbSuspend = menuGsSuspend
};
