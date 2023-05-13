#include "game.h"
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/system.h>
#include <ace/managers/blit.h>
#include <ace/utils/palette.h>
#include <ace/managers/key.h>
#include "selurina.h"

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;

static tBitMap *s_pPlayerFrames;
static tBitMap *s_pPlayerMasks;

static tUwCoordYX s_pPositions[] = {
	{.uwX = 30, .uwY = 30},
	{.uwX = 130, .uwY = 30},
	{.uwX = 230, .uwY = 30},
	{.uwX = 30, .uwY = 100},
	{.uwX = 130, .uwY = 100},
	{.uwX = 230, .uwY = 100},
};

static void gameGsCreate(void) {
	s_pView = viewCreate(0,
		TAG_VIEW_COPLIST_MODE, COPPER_MODE_BLOCK,
		TAG_VIEW_GLOBAL_PALETTE, 1,
	TAG_END);

	s_pVpMain = vPortCreate(0,
		TAG_VPORT_BPP, 5,
		TAG_VPORT_VIEW, s_pView,
	TAG_END);

	s_pBufferMain = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR | BMF_INTERLEAVED,
		TAG_SIMPLEBUFFER_IS_DBLBUF, 1,
		TAG_SIMPLEBUFFER_VPORT, s_pVpMain,
	TAG_END);

	paletteLoad("data/selurina.plt", s_pVpMain->pPalette, 32);

	s_pPlayerFrames = bitmapCreateFromFile("data/player.bm", 0);
	s_pPlayerMasks = bitmapCreateFromFile("data/player_mask.bm", 0);

	systemUnuse();
	viewLoad(s_pView);
}

static void gameGsLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}

	// Undraw & save
	for(UBYTE i = 0; i < 6; ++i) {
		blitRect(s_pBufferMain->pBack, s_pPositions[i].uwX, s_pPositions[i].uwY, 32, 32, 0);
		blitRect(s_pBufferMain->pBack, s_pPositions[i].uwX, s_pPositions[i].uwY, 32, 32, 0);
	}

	// Draw
	for(UBYTE i = 0; i < 6; ++i ) {
		blitCopyMask(
			s_pPlayerFrames, 0, 0,
			s_pBufferMain->pBack, s_pPositions[i].uwX, s_pPositions[i].uwY,
			32, 32, (UWORD*)s_pPlayerMasks->Planes[0]
		);
	}

	viewProcessManagers(s_pView);
	copProcessBlocks();
	systemIdleBegin();
	vPortWaitForEnd(s_pVpMain);
	systemIdleEnd();
}

static void gameGsDestroy(void) {
	viewLoad(0);
	systemUse();

	viewDestroy(s_pView);
	bitmapDestroy(s_pPlayerFrames);
	bitmapDestroy(s_pPlayerMasks);
}

tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
