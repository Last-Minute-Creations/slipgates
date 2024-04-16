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
#include "config.h"
#include "twister.h"

// #define MENU_PREP_BG

#define MENU_COLOR_MAIN 0b1000
#define MENU_COLOR_INACTIVE 0b0010
#define MENU_COLOR_HOVERED 0b1010

#define MENU_LAYER_COLOR_MAIN 0b10
#define MENU_LAYER_COLOR_INACTIVE 0b01
#define MENU_LAYER_COLOR_HOVERED 0b11

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

static tView *s_pMenuView;
static tVPort *s_pMenuVp;
static tSimpleBufferManager *s_pMenuBfr;
static tTextBitMap *s_pTextBuffer;
static tMenuExit s_eMenuExit;
static tSprite *s_pSpriteCrosshair;
static UWORD s_pPalette[32];
static tFade *s_pFade;

static tMenuOption s_pOptions[5];
static UBYTE s_ubOptionCount;

static tBitMap s_sMenuLayerBuffers[2] = {
	[0] = {
		.BytesPerRow = 320 / 8 * 4,
		.Rows = 256 + 32,
		.Depth = 2,
		.Flags = BMF_INTERLEAVED,
	},
	[1] = {
		.BytesPerRow = 320 / 8 * 4,
		.Rows = 256 + 32,
		.Depth = 2,
		.Flags = BMF_INTERLEAVED,
	}
};
static tBitMap *s_pMenuLayerFront, *s_pMenuLayerBack;

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
	configResetProgress();
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

static void menuDrawStr(
	const tFont *pFont, UWORD uwX, UWORD uwY,
	const char *szText, UBYTE ubColor, UBYTE ubFlags
) {
	fontFillTextBitMap(g_pFont, s_pTextBuffer, szText);
	fontDrawTextBitMap(s_pMenuLayerBack, s_pTextBuffer, uwX, uwY, ubColor, ubFlags);
	fontDrawTextBitMap(s_pMenuLayerFront, s_pTextBuffer, uwX, uwY, ubColor, ubFlags);
}

static void menuDrawPos(UBYTE ubIndex) {
#if !defined(MENU_PREP_BG)
	tMenuOption *pOption = &s_pOptions[ubIndex];

	UBYTE ubColor = MENU_LAYER_COLOR_MAIN;
	if(!pOption->isEnabled) {
		ubColor = MENU_LAYER_COLOR_INACTIVE;
	}
	else if(pOption->isHovered) {
		ubColor = MENU_LAYER_COLOR_HOVERED;
	}

	menuDrawStr(
		g_pFont, pOption->sRect.uwX1, pOption->sRect.uwY1,
		pOption->szLabel, ubColor, FONT_COOKIE
	);
#endif
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
	menuDraw();

#if !defined(MENU_PREP_BG)
	static const char szVersion[15] = "V." GAME_VERSION;
	menuDrawStr(
		g_pFont, 320, 50, szVersion,
		MENU_LAYER_COLOR_INACTIVE, FONT_RIGHT | FONT_COOKIE
	);

	menuDrawStr(
		g_pFont, SCREEN_PAL_WIDTH / 2, SCREEN_PAL_HEIGHT,
		"A game by Last Minute Creations",
		MENU_LAYER_COLOR_INACTIVE, FONT_COOKIE | FONT_HCENTER | FONT_BOTTOM
	);
#endif
}

//------------------------------------------------------------------- PUBLIC FNS

tSimpleBufferManager *menuGetBuffer(void) {
	return s_pMenuBfr;
}

tTextBitMap *menuGetTextBitmap(void) {
	return s_pTextBuffer;
}

tFade *menuGetFade(void) {
	return s_pFade;
}

//-------------------------------------------------------------------- GAMESTATE

static void menuGsCreate(void) {
	s_pMenuView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
		TAG_VIEW_COPLIST_MODE, COPPER_MODE_RAW,
		TAG_VIEW_COPLIST_RAW_COUNT, 16 + simpleBufferGetRawCopperlistInstructionCount(4),
	TAG_END);

	s_pMenuVp = vPortCreate(0,
		TAG_VPORT_VIEW, s_pMenuView,
		TAG_VPORT_BPP, 4,
	TAG_END);

	s_pMenuBfr = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pMenuVp,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_INTERLEAVED,
		TAG_SIMPLEBUFFER_BOUND_WIDTH, 320,
		TAG_SIMPLEBUFFER_BOUND_HEIGHT, 256 + 32,
		TAG_SIMPLEBUFFER_USE_X_SCROLLING, 0,
		TAG_SIMPLEBUFFER_IS_DBLBUF, 1,
		TAG_SIMPLEBUFFER_COPLIST_OFFSET, 16,
	TAG_END);

	bitmapLoadFromFile(s_pMenuBfr->pFront, "data/menu_bg.bm", 0, 0);
	blitCopyAligned(s_pMenuBfr->pFront, 0, 0, s_pMenuBfr->pBack, 0, 0, 320, 128);
	blitCopyAligned(s_pMenuBfr->pFront, 0, 128, s_pMenuBfr->pBack, 0, 128, 320, 128);

	twisterInit();

	s_pPalette[0b0001] = 0x303; // 0x707;
	s_pPalette[0b0100] = 0x634; // 0xD69;
	s_pPalette[0b0101] = 0x501; // 0xB03;

	s_pPalette[MENU_COLOR_INACTIVE] = 0x888;
	s_pPalette[MENU_COLOR_INACTIVE | 0b001] = paletteColorMix(s_pPalette[MENU_COLOR_INACTIVE], s_pPalette[0b001], 12);
	s_pPalette[MENU_COLOR_INACTIVE | 0b100] = paletteColorMix(s_pPalette[MENU_COLOR_INACTIVE], s_pPalette[0b100], 12);
	s_pPalette[MENU_COLOR_INACTIVE | 0b101] = paletteColorMix(s_pPalette[MENU_COLOR_INACTIVE], s_pPalette[0b101], 12);

	s_pPalette[MENU_COLOR_MAIN] = 0xAAA;
	s_pPalette[MENU_COLOR_MAIN | 0b001] = paletteColorMix(s_pPalette[MENU_COLOR_MAIN], s_pPalette[0b001], 12);
	s_pPalette[MENU_COLOR_MAIN | 0b100] = paletteColorMix(s_pPalette[MENU_COLOR_MAIN], s_pPalette[0b100], 12);
	s_pPalette[MENU_COLOR_MAIN | 0b101] = paletteColorMix(s_pPalette[MENU_COLOR_MAIN], s_pPalette[0b101], 12);

	s_pPalette[MENU_COLOR_HOVERED] = 0xEEE;
	s_pPalette[MENU_COLOR_HOVERED | 0b001] = paletteColorMix(s_pPalette[MENU_COLOR_HOVERED], s_pPalette[0b001], 12);
	s_pPalette[MENU_COLOR_HOVERED | 0b100] = paletteColorMix(s_pPalette[MENU_COLOR_HOVERED], s_pPalette[0b100], 12);
	s_pPalette[MENU_COLOR_HOVERED | 0b101] = paletteColorMix(s_pPalette[MENU_COLOR_HOVERED], s_pPalette[0b101], 12);

	s_pPalette[17] = 0xD69;
	s_pPalette[18] = 0x707;
	s_pPalette[19] = 0xB03;

	s_pFade = fadeCreate(s_pMenuView, s_pPalette, 32);
	// paletteSave(s_pPalette, 16, "menu.plt");

	s_pMenuLayerFront = &s_sMenuLayerBuffers[0];
	s_pMenuLayerBack = &s_sMenuLayerBuffers[1];
	s_pMenuLayerFront->Planes[0] = s_pMenuBfr->pFront->Planes[1];
	s_pMenuLayerFront->Planes[1] = s_pMenuBfr->pFront->Planes[3];
	s_pMenuLayerBack->Planes[0] = s_pMenuBfr->pBack->Planes[1];
	s_pMenuLayerBack->Planes[1] = s_pMenuBfr->pBack->Planes[3];

	s_pTextBuffer = fontCreateTextBitMap(336, g_pFont->uwHeight);

	spriteManagerCreate(s_pMenuView, 0);
	s_pSpriteCrosshair = spriteAdd(0, g_pBmCursor);
#if !defined(MENU_PREP_BG)
	systemSetDmaBit(DMAB_SPRITE, 1);
#endif
	mouseSetBounds(MOUSE_PORT_1, 7, 14, SCREEN_PAL_WIDTH - 8, SCREEN_PAL_HEIGHT - 13);

	systemUnuse();

	s_eMenuExit = MENU_EXIT_NONE;

	menuClearOptions();
	UBYTE isContinueEnabled = g_sConfig.ubUnlockedLevels > 1;
	menuAddOption("NEW GAME", 1, onNewGame);
	menuAddOption("CONTINUE", isContinueEnabled, onContinue);
	menuAddOption("CREDITS", 1, onCredits);
	menuAddOption("EXIT", 1, onExit);

	menuRedrawAll();
	fadeStart(s_pFade, FADE_STATE_IN, 15, 0, 0);
	viewLoad(s_pMenuView);
}

static void menuGsLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}

	if(fadeProcess(s_pFade) == FADE_STATE_EVENT_FIRED) {
		return;
	}

	twisterLoop(s_pMenuBfr);

	UWORD uwMouseX = mouseGetX(MOUSE_PORT_1);
	UWORD uwMouseY = mouseGetY(MOUSE_PORT_1);
	s_pSpriteCrosshair->wX = uwMouseX - 8;
	s_pSpriteCrosshair->wY = uwMouseY - 14;
	spriteProcess(s_pSpriteCrosshair);
	spriteProcessChannel(0);

	menuProcess(uwMouseX, uwMouseY);

	s_pMenuBfr->sCommon.process(&s_pMenuBfr->sCommon);
	copProcessBlocks();

	tBitMap *pTmp = s_pMenuLayerFront;
	s_pMenuLayerFront = s_pMenuLayerBack;
	s_pMenuLayerBack = pTmp;

	systemIdleBegin();
	vPortWaitUntilEnd(s_pMenuVp);
	systemIdleEnd();
}

static void menuGsDestroy(void) {
	viewLoad(0);
	systemSetDmaBit(DMAB_SPRITE, 0);

	systemUse();

	fadeDestroy(s_pFade);
	spriteManagerDestroy();
	fontDestroyTextBitMap(s_pTextBuffer);
	viewDestroy(s_pMenuView);
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
