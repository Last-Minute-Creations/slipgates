/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/system.h>
#include <ace/managers/key.h>
#include <ace/managers/mouse.h>
#include <ace/utils/palette.h>
#include <ace/generic/screen.h>
#include "fade.h"
#include "assets.h"
#include "slipgates.h"
// #include "color.h"
// #include "music.h"
#include "menu.h"

#define SLIDES_MAX 10
#define LINES_PER_SLIDE_MAX 8
#define SLIDE_SIZE 128
#define TEXT_LINE_HEIGHT (10 + 2)
#define CUTSCENE_HEIGHT (SLIDE_SIZE + 16 + LINES_PER_SLIDE_MAX * TEXT_LINE_HEIGHT)
#define SLIDE_POS_X ((SCREEN_PAL_WIDTH - SLIDE_SIZE) / 2)
#define SLIDE_POS_Y ((SCREEN_PAL_HEIGHT - CUTSCENE_HEIGHT) / 2)
#define TEXT_POS_X (SCREEN_PAL_WIDTH / 2)
#define TEXT_POS_Y (SLIDE_POS_Y + SLIDE_SIZE + 16)
#define SLIDE_ANIM_COLS 2
#define COLOR_TEXT 8

// Check with intellisense to see if it's positive and cutscene fits on screen
// static const BYTE bIntroMargin = (SLIDE_POS_Y);

#define GRAYSCALE(ubValue) ((ubValue) << 8 | (ubValue) << 4 | (ubValue))

typedef struct tSlide {
	tBitMap *pBitmap;
	UWORD pPalette[32];
} tSlide;

static const char *s_pLinesIntro[][LINES_PER_SLIDE_MAX] = {
	{
		"Rumors of ancient ruins were circulating among townsfolk.",
		"An earthquake revealed an entrance to ancient structure.",
		"Many adventurers went investigating, never to return again.",
		"But I will succeed and unravel the secrets stored within.",
		0,
	},
	{
		"Deep within tunnels, the floor crumbled beneath me,",
		"Having no way for escape, I delved further into ruins.",
		0,
	},
};

static const char *s_pLinesOutro[][LINES_PER_SLIDE_MAX] = {
	{
		"As I entered the innermost chamber, a ghastly revelation struck me.",
		"This was no trial, but an elaborate trap for a brilliant fool such as me.",
		"Inside awaited a living mass, its visage filling with disgust and horror.",
		"An instant cure for my clouded thoughts, alas, happened too late.",
		0,
	},
	{
		"The vile entity looked at me, its intent instantly resonated in my head:",
		"- come closer, show me how to set me free",
		"The pull was too strong, I couldn't resist the call.",
		"My legs were moving by themselves.",
		"Soon I found myself next to the hideous mass,",
		"and it began to devour me into itself.",
		0,
	},
	{
		"It fed upon my memories while filling me with agonizing pain.",
		"Made me remember all the chambers,",
		"then all my life - perhaps preying for the first time in centuries.",
		"That vile consumption flashed unknown scenes before my eyes,",
		"as it seemed that I took glimpses into creature's past as well.",
		0,
	},
	{
		"A civilization long past gone.",
		"A reckless deity, seated on an ancient pyramid.",
		"Revered by countless humans, bound to serve its cruel master.",
		"Some of them broke free and imprisoned the monster.",
		"Not able to destroy it, they locked it inside its grotesque monument.",
		"Locked behind countless traps and sealed walls to prevent its escape.",
		"They left a spell behind, allowing reaching the monster,",
		"if a way of destroying it is discovered.",
	},
	{
		"When I came back to my senses, I found myself on the surface,",
		"where the ruins had their entrance.",
		"The ancient structure was now completely destroyed and untraversable.",
		"No matter how many times I tried,",
		"I could not remember that ancient spell.",
		"The memories of chambers I visited were clouded,",
		"as if I only had scraps of what that entity devoured from my mind.",
		0,
	},
	{
		"I tried alerting nearby cities, but no one believed in any of my stories.",
		"Laughed off, called a madman, and kicked out of every premise I visited,",
		"I was left alone with knowledge of what horror I have unleashed.",
		"I had to find that creature.",
		"I long for power once discovered,",
		"the secrets of those ancient ruins,",
		"and greatness that was taken from me.",
		0,
	},
};

static tView *s_pView;
static tVPort *s_pVp;
static tSimpleBufferManager *s_pBuffer;
static tFade *s_pFade;
static tTextBitMap *s_pTextBitmap;
static UBYTE s_ubSlideCount, s_ubCurrentSlide, s_ubCurrentLine;
static UBYTE s_isOutro;
static tState *s_pNextState;
static UWORD s_uwFontColorVal;
static UBYTE s_ubFadeStep;

static tSlide s_pSlides[SLIDES_MAX];

static tCopBlock *s_pBlockAboveLine, *s_pBlockBelowLine, *s_pBlockAfterLines;

static const char *getLine(UBYTE ubSlide, UBYTE ubLine) {
	if(ubLine >= LINES_PER_SLIDE_MAX) {
		return 0;
	}
	const char *pLine = (s_isOutro ? s_pLinesOutro : s_pLinesIntro)[ubSlide][ubLine];
	return pLine;
}

static void drawSlide(void) {
	// Debug display of bg
	// blitRect(s_pBuffer->pBack, 0, 0, 320, 128, 1);
	// blitRect(s_pBuffer->pBack, 0, 128, 320, 128, 1);

	// Draw slide
	// blitCopyAligned(
	// 	s_pSlides[s_ubCurrentSlide].pBitmap, 0, 0, s_pBuffer->pBack,
	// 	SLIDE_POS_X, SLIDE_POS_Y, SLIDE_SIZE, SLIDE_SIZE
	// );

	// Erase old text
	blitRect(
		s_pBuffer->pBack, 0, TEXT_POS_Y,
		SCREEN_PAL_WIDTH, LINES_PER_SLIDE_MAX * TEXT_LINE_HEIGHT, 0
	);

	// Load new palette
	fadeChangeRefPalette(s_pFade, s_pSlides[s_ubCurrentSlide].pPalette, 32);

	// Update the color used by copper to match the palette
	s_pBlockAfterLines->pCmds[0].sMove.bfValue = s_pSlides[s_ubCurrentSlide].pPalette[COLOR_TEXT];
}

static void initSlideText(void) {
	// Reset copblocks
	s_ubCurrentLine = 0;
	copBlockWait(
		s_pView->pCopList, s_pBlockAboveLine, 0,
		s_pView->ubPosY + TEXT_POS_Y + TEXT_LINE_HEIGHT * s_ubCurrentLine
	);
	copBlockWait(
		s_pView->pCopList, s_pBlockBelowLine, 0,
		s_pView->ubPosY + TEXT_POS_Y + TEXT_LINE_HEIGHT * (s_ubCurrentLine + 1) - 1
	);
	s_pBlockAboveLine->uwCurrCount = 0;
	copMove(
		s_pView->pCopList, s_pBlockAboveLine,
		&g_pCustom->color[COLOR_TEXT], 0x000
	);
	copProcessBlocks();
	vPortWaitForEnd(s_pVp);
	s_ubFadeStep = 0;

	// Draw text
	while(getLine(s_ubCurrentSlide, s_ubCurrentLine)) {
		const char *szLine = getLine(s_ubCurrentSlide, s_ubCurrentLine);
		// Draw next portion of text
		fontDrawStr(
			g_pFont, s_pBuffer->pBack, TEXT_POS_X,
			TEXT_POS_Y + TEXT_LINE_HEIGHT * s_ubCurrentLine, szLine,
			COLOR_TEXT, FONT_LAZY | FONT_HCENTER, s_pTextBitmap
		);
		++s_ubCurrentLine;
	}

	s_ubCurrentLine = 0;
}

static void onFadeIn(void) {
	copBlockEnable(s_pView->pCopList, s_pBlockAboveLine);
	copBlockEnable(s_pView->pCopList, s_pBlockBelowLine);
	copBlockEnable(s_pView->pCopList, s_pBlockAfterLines);
	initSlideText();
}

static void cutsceneGsCreate(void) {
	s_pView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
	TAG_END);

	s_pVp = vPortCreate(0,
		TAG_VPORT_BPP, 5,
		TAG_VPORT_VIEW, s_pView,
	TAG_END);

	s_pBuffer = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR | BMF_INTERLEAVED,
		TAG_SIMPLEBUFFER_VPORT, s_pVp,
		TAG_SIMPLEBUFFER_IS_DBLBUF, 0,
	TAG_END);


	// Load palette
	UWORD pPalette[32];
	paletteLoad("data/slipgates.plt", pPalette, 32);
	s_pFade = fadeCreate(s_pView, pPalette, 32);
	s_pTextBitmap = fontCreateTextBitMap(320 + 16, g_pFont->uwHeight);

	s_uwFontColorVal = pPalette[COLOR_TEXT];
	s_pBlockAboveLine = copBlockCreate(s_pView->pCopList, 1, 0, 0);
	s_pBlockBelowLine = copBlockCreate(s_pView->pCopList, 1, 0, 0);
	s_pBlockAfterLines = copBlockCreate(
		s_pView->pCopList, 1, 0,
		s_pView->ubPosY + TEXT_POS_Y + LINES_PER_SLIDE_MAX * TEXT_LINE_HEIGHT
	);
	copMove(
		s_pView->pCopList, s_pBlockBelowLine,
		&g_pCustom->color[COLOR_TEXT], 0x000
	);
	copMove(
		s_pView->pCopList, s_pBlockAfterLines,
		&g_pCustom->color[COLOR_TEXT], s_uwFontColorVal
	);
	copBlockDisable(s_pView->pCopList, s_pBlockAboveLine);
	copBlockDisable(s_pView->pCopList, s_pBlockBelowLine);
	copBlockDisable(s_pView->pCopList, s_pBlockAfterLines);

	// Load slides
	// char szPath[30];
	s_ubSlideCount = s_isOutro ?
		(sizeof(s_pLinesOutro) / sizeof(s_pLinesOutro[0])) :
		(sizeof(s_pLinesIntro) / sizeof(s_pLinesIntro[0]));
	for(UBYTE ubSlideIndex = 0; ubSlideIndex < SLIDES_MAX; ++ubSlideIndex) {
		// sprintf(szPath, "data/%s/%hhu.bm", s_isOutro ? "outro" : "intro", ubSlideIndex);
		// s_pSlides[ubSlideIndex].pBitmap = bitmapCreateFromFile(szPath, 0);
		// if(!s_pSlides[ubSlideIndex].pBitmap) {
		// 	s_ubSlideCount = ubSlideIndex;
		// 	break;
		// }
		// sprintf(szPath, "data/%s/%hhu.plt", s_isOutro ? "outro" : "intro", ubSlideIndex);
		// paletteLoad(szPath, s_pSlides[ubSlideIndex].pPalette, 32);
		paletteLoad("data/slipgates.plt", s_pSlides[ubSlideIndex].pPalette, 32);
		s_pSlides[ubSlideIndex].pPalette[COLOR_TEXT] = s_uwFontColorVal;
	}

	systemUnuse();
	// musicLoadPreset(s_isOutro ? MUSIC_PRESET_OUTRO : MUSIC_PRESET_INTRO);

	// Draw first slide
	drawSlide();
	fadeStart(s_pFade, FADE_STATE_IN, 50, 0, onFadeIn);
	viewLoad(s_pView);
}

static void onCutsceneFadeOut(void) {
	if(s_isOutro) {
		// menuStartWithCredits();
	}

	// Pop to previous state or change to next state
	if(!s_pNextState) {
		statePop(g_pGameStateManager);
	}
	else {
		stateChange(g_pGameStateManager, s_pNextState);
	}
}

static void onFadeOutSlide(void) {
	drawSlide();
	fadeStart(s_pFade, FADE_STATE_IN, 15, 0, onFadeIn);
}

static void cutsceneGsLoop(void) {
	tFadeState eFadeState = fadeProcess(s_pFade);
	if(eFadeState != FADE_STATE_IDLE) {
		return;
	}

	vPortWaitForEnd(s_pVp);
	if(s_ubFadeStep <= 0x10) {
		// Process text fade-in
		// Increment color
		s_pBlockAboveLine->uwCurrCount = 0;
		copMove(
			s_pView->pCopList, s_pBlockAboveLine, &g_pCustom->color[COLOR_TEXT],
			s_ubFadeStep < 0x10 ? GRAYSCALE(s_ubFadeStep) : s_uwFontColorVal
		);
		++s_ubFadeStep;

		// Refresh copperlist
		copProcessBlocks();
	}
	else if(getLine(s_ubCurrentSlide, s_ubCurrentLine)) {
		// Start fade-in for next line
		++s_ubCurrentLine;
		if(getLine(s_ubCurrentSlide, s_ubCurrentLine)) {
			// Draw next portion of text - move copBlocks and reset fadeStep
			copBlockWait(
				s_pView->pCopList, s_pBlockAboveLine, 0,
				s_pView->ubPosY + TEXT_POS_Y + TEXT_LINE_HEIGHT * s_ubCurrentLine
			);
			copBlockWait(
				s_pView->pCopList, s_pBlockBelowLine, 0,
				s_pView->ubPosY + TEXT_POS_Y + TEXT_LINE_HEIGHT * (s_ubCurrentLine + 1) - 1
			);
			s_pBlockAboveLine->uwCurrCount = 0;
			copMove(
				s_pView->pCopList, s_pBlockAboveLine,
				&g_pCustom->color[COLOR_TEXT], 0x000
			);
			copProcessBlocks();
			s_ubFadeStep = 0;
		}
		else {
			// Last line was displayed - copblocks are no longer needed
			copBlockDisable(s_pView->pCopList, s_pBlockAboveLine);
			copBlockDisable(s_pView->pCopList, s_pBlockBelowLine);
			copBlockDisable(s_pView->pCopList, s_pBlockAfterLines);
			copProcessBlocks();
		}
	}
	else if(
		keyUse(KEY_RETURN) || keyUse(KEY_F) || mouseUse(MOUSE_PORT_1, MOUSE_LMB)
	) {
		if(++s_ubCurrentSlide < s_ubSlideCount) {
			// Draw next slide
			fadeStart(s_pFade, FADE_STATE_OUT, 15, 0, onFadeOutSlide);
		}
		else {
			// Quit the cutscene
			fadeStart(s_pFade, FADE_STATE_OUT, 50, 0, onCutsceneFadeOut);
		}
	}
}

static void cutsceneGsDestroy(void) {
	viewLoad(0);
	systemUse();
	viewDestroy(s_pView);
	fadeDestroy(s_pFade);

	// Destroy slides
	for(UBYTE i = 0; i < s_ubSlideCount; ++i) {
		bitmapDestroy(s_pSlides[i].pBitmap);
	}

	// Destroy text array

}

void cutsceneSetup(UBYTE isOutro, tState *pNextState) {
	s_isOutro = isOutro;
	s_ubCurrentSlide = 0;
	s_ubCurrentLine = 0;
	s_pNextState = pNextState;
}

tState g_sStateCutscene = {
	.cbCreate = cutsceneGsCreate, .cbLoop = cutsceneGsLoop,
	.cbDestroy = cutsceneGsDestroy
};
