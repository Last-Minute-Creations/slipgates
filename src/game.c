#include "game.h"
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/system.h>
#include <ace/managers/blit.h>
#include <ace/managers/bob.h>
#include <ace/managers/sprite.h>
#include <ace/managers/mouse.h>
#include <ace/utils/palette.h>
#include <ace/managers/key.h>
#include "slipgates.h"
#include "body_box.h"

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;

static tBitMap *s_pPlayerFrames;
static tBitMap *s_pPlayerMasks;
static tBitMap *s_pBmCursor;
static tBodyBox s_sBodyPlayer;
static tSprite *s_pSpriteCrosshair;

UBYTE g_pTiles[20][16] = { // x,y
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
	{1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};


static UWORD s_uwGameFrame;
static char s_szPosX[13];
static char s_szPosY[13];
static char s_szVelocityX[13];
static char s_szVelocityY[13];
static char s_szAccelerationX[13];
static char s_szAccelerationY[13];

static UBYTE tileGetColor(UBYTE ubTile) {
	switch (ubTile)
	{
		case 1: return 7;
		case 2: return 8;
		case 3: return 13;
		default: return 16;
	}
}

static void drawMap(void) {
	for(UBYTE x = 0; x < 20; ++x) {
		for(UBYTE y = 0; y < 16; ++y) {
			blitRect(s_pBufferMain->pBack, x * 16, y * 16, 16, 16, tileGetColor(g_pTiles[x][y]));
			blitRect(s_pBufferMain->pFront, x * 16, y * 16, 16, 16, tileGetColor(g_pTiles[x][y]));
		}
	}
}

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

	paletteLoad("data/slipgates.plt", s_pVpMain->pPalette, 32);
	s_pVpMain->pPalette[17] = 0xA86;
	s_pVpMain->pPalette[18] = 0x27D;
	s_pVpMain->pPalette[19] = 0xE96;

	s_pPlayerFrames = bitmapCreateFromFile("data/player.bm", 0);
	s_pPlayerMasks = bitmapCreateFromFile("data/player_mask.bm", 0);
	s_pBmCursor = bitmapCreateFromFile("data/cursor.bm", 0);

	bobManagerCreate(s_pBufferMain->pFront, s_pBufferMain->pBack, s_pBufferMain->uBfrBounds.uwY);
	bobInit(
		&s_sBodyPlayer.sBob, 16, 32, 1,
		s_pPlayerFrames->Planes[0], s_pPlayerMasks->Planes[0],
		0, 0
	);
	bodyInit(&s_sBodyPlayer, fix16_from_int(100), fix16_from_int(150));
	bobReallocateBgBuffers();

	spriteManagerCreate(s_pView, 0);
	s_pSpriteCrosshair = spriteAdd(0, s_pBmCursor);
	systemSetDmaBit(DMAB_SPRITE, 1);
	spriteProcessChannel(0);
	mouseSetBounds(MOUSE_PORT_1, 0, 0, 320 - 16, 256 - 27);

	s_uwGameFrame = 0;

	systemUnuse();
	drawMap();
	viewLoad(s_pView);
}

static void gameGsLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}

 	bobBegin(s_pBufferMain->pBack);

	if(keyCheck(KEY_W)) {
		s_sBodyPlayer.fVelocityY = fix16_from_int(-3);
	}

	s_sBodyPlayer.fVelocityX = 0;
	if(keyCheck(KEY_A)) {
		s_sBodyPlayer.fVelocityX = fix16_from_int(-1);
	}
	else if(keyCheck(KEY_D)) {
		s_sBodyPlayer.fVelocityX = fix16_from_int(1);
	}

	bodySimulate(&s_sBodyPlayer);
	bobPush(&s_sBodyPlayer.sBob);
	bobPushingDone();
	bobEnd();

	s_pSpriteCrosshair->wX = mouseGetX(MOUSE_PORT_1);
	s_pSpriteCrosshair->wY = mouseGetY(MOUSE_PORT_1);
	spriteProcess(s_pSpriteCrosshair);

	fix16_to_str(s_sBodyPlayer.fPosX, s_szPosX, 2);
	fix16_to_str(s_sBodyPlayer.fPosY, s_szPosY, 2);
	fix16_to_str(s_sBodyPlayer.fVelocityX, s_szVelocityX, 2);
	fix16_to_str(s_sBodyPlayer.fVelocityY, s_szVelocityY, 2);
	fix16_to_str(s_sBodyPlayer.fAccelerationX, s_szAccelerationX, 2);
	fix16_to_str(s_sBodyPlayer.fAccelerationY, s_szAccelerationY, 2);

	logWrite(
		"GF %hu end, pos %s,%s v %s,%s a %s,%s",
		s_uwGameFrame,
		s_szPosX, s_szPosY,
		s_szVelocityX, s_szVelocityY,
		s_szAccelerationX, s_szAccelerationY
	);

	++s_uwGameFrame;
	viewProcessManagers(s_pView);
	copProcessBlocks();
	systemIdleBegin();
	vPortWaitForEnd(s_pVpMain);
	systemIdleEnd();
}

static void gameGsDestroy(void) {
	viewLoad(0);
	systemUse();

	systemSetDmaBit(DMAB_SPRITE, 0);
	spriteManagerDestroy();
	viewDestroy(s_pView);
	bobManagerDestroy();
	bitmapDestroy(s_pPlayerFrames);
	bitmapDestroy(s_pPlayerMasks);
	bitmapDestroy(s_pBmCursor);
}

tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
