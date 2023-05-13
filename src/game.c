#include "game.h"
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/system.h>
#include <ace/managers/blit.h>
#include <ace/managers/bob.h>
#include <ace/utils/palette.h>
#include <ace/managers/key.h>
#include "selurina.h"

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;

static tBitMap *s_pPlayerFrames;
static tBitMap *s_pPlayerMasks;

static tBob s_pBobs[6];

static tUwCoordYX s_pPositions[6] = {
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

 bobManagerCreate(s_pBufferMain->pFront, s_pBufferMain->pBack, s_pBufferMain->uBfrBounds.uwY);
 for(UBYTE i = 0; i < 6; ++i) {
 	bobInit(
		&s_pBobs[i], 32, 32, 1,
		s_pPlayerFrames->Planes[0], s_pPlayerMasks->Planes[0],
		s_pPositions[i].uwX, s_pPositions[i].uwY
	);
 }
 bobReallocateBgBuffers();

	systemUnuse();
	viewLoad(s_pView);
}

static void gameGsLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}

 	bobBegin(s_pBufferMain->pBack);
	for(UBYTE i = 0; i < 6; ++i) {
		bobPush(&s_pBobs[i]);
	}
	bobPushingDone();
	bobEnd();

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
	bobManagerDestroy();
	bitmapDestroy(s_pPlayerFrames);
	bitmapDestroy(s_pPlayerMasks);
}

tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
