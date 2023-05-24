/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "game.h"
#include <ace/generic/screen.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/system.h>
#include <ace/managers/blit.h>
#include <ace/managers/bob.h>
#include <ace/managers/sprite.h>
#include <ace/managers/mouse.h>
#include <ace/utils/palette.h>
#include <ace/utils/file.h>
#include <ace/managers/key.h>
#include "slipgates.h"
#include "body_box.h"
#include "map.h"
#include "game_math.h"
#include "tile_tracer.h"
#include "player.h"

// #define GAME_DRAW_GRID

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;

static tBitMap *s_pPlayerFrames;
static tBitMap *s_pPlayerMasks;
static tBitMap *s_pBoxFrames;
static tBitMap *s_pBoxMasks;
static tBitMap *s_pBmCursor;
static tPlayer s_sPlayer;
static tBodyBox s_sBodyBox;
static tSprite *s_pSpriteCrosshair;

static UWORD s_uwGameFrame;
// static char s_szPosX[13];
// static char s_szPosY[13];
// static char s_szVelocityX[13];
// static char s_szVelocityY[13];
// static char s_szAccelerationX[13];
// static char s_szAccelerationY[13];

tTileTracer g_sTracerSlipgate;

static UBYTE tileGetColor(tTile eTile) {
	switch (eTile)
	{
		case TILE_WALL_1: return 7;
		case TILE_WALL_NO_SLIPGATE_1: return 3;
		case TILE_SLIPGATE_1: return 8;
		case TILE_SLIPGATE_2: return 13;
		case TILE_FORCEFIELD_1: return 5;
		case TILE_DEATH_FIELD_1: return 2;
		default: return 16;
	}
}

// TODO: refactor and move to map.c?
static void drawMap(void) {
	for(UBYTE ubTileX = 0; ubTileX < MAP_TILE_WIDTH; ++ubTileX) {
		for(UBYTE ubTileY = 0; ubTileY < MAP_TILE_HEIGHT; ++ubTileY) {
			gameDrawTile(ubTileX, ubTileY);
		}
	}
}

static void loadLevel(UBYTE ubIndex) {
	viewLoad(0);
	s_uwGameFrame = 0;
	mapLoad(ubIndex);
	playerReset(&s_sPlayer, g_sCurrentLevel.fStartX, g_sCurrentLevel.fStartY);
	bodyInit(
		&s_sBodyBox,
		fix16_from_int(200),
		fix16_from_int(200),
		8, 8
	);
	tracerInit(&g_sTracerSlipgate);
	drawMap();
	viewLoad(s_pView);
}

static void saveLevel(UBYTE ubIndex) {
	mapCloseSlipgates();

	char szName[13];
	char cNewLine = '\n';
	sprintf(szName, "level%03hhu.dat", ubIndex);
	systemUse();
	tFile *pFile = fileOpen(szName, "wb");
	fileWrite(pFile, &s_sPlayer.sBody.fPosX, sizeof(s_sPlayer.sBody.fPosX));
	fileWrite(pFile, &s_sPlayer.sBody.fPosY, sizeof(s_sPlayer.sBody.fPosY));
	fileWrite(pFile, &cNewLine, sizeof(cNewLine));

	for(UBYTE ubY = 0; ubY < MAP_TILE_HEIGHT; ++ubY) {
		for(UBYTE ubX = 0; ubX < MAP_TILE_WIDTH; ++ubX) {
			UBYTE ubGlyph = '0' + mapGetTileAt(ubX, ubY);
			fileWrite(pFile, &ubGlyph, sizeof(ubGlyph));
		}
		fileWrite(pFile, &cNewLine, sizeof(cNewLine));
	}
	fileClose(pFile);
	systemUnuse();
}

static void gameGsCreate(void) {
	gameMathInit();

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
	s_pBoxFrames = bitmapCreateFromFile("data/box.bm", 0);
	s_pBoxMasks = bitmapCreateFromFile("data/box_mask.bm", 0);
	s_pBmCursor = bitmapCreateFromFile("data/cursor.bm", 0);

	bobManagerCreate(s_pBufferMain->pFront, s_pBufferMain->pBack, s_pBufferMain->uBfrBounds.uwY);
	bobInit(
		&s_sPlayer.sBody.sBob, 16, 16, 1,
		s_pPlayerFrames->Planes[0], s_pPlayerMasks->Planes[0],
		0, 0
	);
	bobInit(
		&s_sBodyBox.sBob, 16, 8, 1,
		s_pBoxFrames->Planes[0], s_pBoxMasks->Planes[0],
		0, 0
	);
	bobReallocateBgBuffers();

	spriteManagerCreate(s_pView, 0);
	s_pSpriteCrosshair = spriteAdd(0, s_pBmCursor);
	systemSetDmaBit(DMAB_SPRITE, 1);
	spriteProcessChannel(0);
	mouseSetBounds(MOUSE_PORT_1, 0, 0, SCREEN_PAL_WIDTH - 16, SCREEN_PAL_HEIGHT - 27);

	systemUnuse();
	loadLevel(0);
}

static void gameGsLoop(void) {
	g_pCustom->color[0] = 0xF89;

	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}
	else {
		for(UBYTE i = 0; i < 4; ++i) {
			if(keyUse(KEY_F1 + i)) {
				if(keyCheck(KEY_CONTROL)) {
					saveLevel(1 + i);
				}
				else {
					loadLevel(1 + i);
				}
				break;
			}
		}
	}

 	bobBegin(s_pBufferMain->pBack);

	tUwCoordYX sPosCross = gameGetCrossPosition();
	s_pSpriteCrosshair->wX = sPosCross.uwX - 8;
	s_pSpriteCrosshair->wY = sPosCross.uwY - 14;

	// Level editor
	if(keyCheck(KEY_Z)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_BG_1;
		gameDrawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	else if(keyCheck(KEY_X)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_WALL_1;
		gameDrawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	else if(keyCheck(KEY_C)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_WALL_NO_SLIPGATE_1;
		gameDrawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	else if(keyCheck(KEY_V)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_FORCEFIELD_1;
		gameDrawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	else if(keyCheck(KEY_B)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_DEATH_FIELD_1;
		gameDrawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	spriteProcess(s_pSpriteCrosshair);

	playerProcess(&s_sPlayer);

	// Debug stuff
	if(keyUse(KEY_T)) {
		bodyTeleport(&s_sPlayer.sBody, sPosCross.uwX, sPosCross.uwY);
	}
	if(keyUse(KEY_Y)) {
		bodyTeleport(&s_sBodyBox, sPosCross.uwX, sPosCross.uwY);
	}

	bodySimulate(&s_sBodyBox);
	bobPush(&s_sBodyBox.sBob);
	bodySimulate(&s_sPlayer.sBody);
	bobPush(&s_sPlayer.sBody.sBob);
	bobPushingDone();
	bobEnd();

	// fix16_to_str(s_sPlayer.sBody.fPosX, s_szPosX, 2);
	// fix16_to_str(s_sPlayer.sBody.fPosY, s_szPosY, 2);
	// fix16_to_str(s_sPlayer.sBody.fVelocityX, s_szVelocityX, 2);
	// fix16_to_str(s_sPlayer.sBody.fVelocityY, s_szVelocityY, 2);
	// fix16_to_str(s_sPlayer.sBody.fAccelerationX, s_szAccelerationX, 2);
	// fix16_to_str(s_sPlayer.sBody.fAccelerationY, s_szAccelerationY, 2);

	// logWrite(
	// 	"GF %hu end, pos %s,%s v %s,%s a %s,%s",
	// 	s_uwGameFrame,
	// 	s_szPosX, s_szPosY,
	// 	s_szVelocityX, s_szVelocityY,
	// 	s_szAccelerationX, s_szAccelerationY
	// );

	++s_uwGameFrame;
	viewProcessManagers(s_pView);
	copProcessBlocks();
	g_pCustom->color[0] = 0x9F8;
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
	bitmapDestroy(s_pBoxFrames);
	bitmapDestroy(s_pBoxMasks);
	bitmapDestroy(s_pBmCursor);
}

//------------------------------------------------------------------- PUBLIC FNS

// TODO: replace with tile draw queue
void gameDrawTile(UBYTE ubTileX, UBYTE ubTileY) {
	UBYTE ubColor = tileGetColor(mapGetTileAt(ubTileX, ubTileY));
	blitRect(
		s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		MAP_TILE_SIZE, MAP_TILE_SIZE, ubColor
	);
	blitRect(
		s_pBufferMain->pFront, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		MAP_TILE_SIZE, MAP_TILE_SIZE, ubColor
	);

#if defined(GAME_DRAW_GRID)
	blitLine(
		s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		(ubTileX + 1) * MAP_TILE_SIZE - 1, ubTileY * MAP_TILE_SIZE, 11, 0xAAAA, 0
	);
	blitLine(
		s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		ubTileX * MAP_TILE_SIZE, (ubTileY + 1) * MAP_TILE_SIZE - 1, 11, 0xAAAA, 0
	);
	blitLine(
		s_pBufferMain->pFront, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		(ubTileX + 1) * MAP_TILE_SIZE - 1, ubTileY * MAP_TILE_SIZE, 11, 0xAAAA, 0
	);
	blitLine(
		s_pBufferMain->pFront, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		ubTileX * MAP_TILE_SIZE, (ubTileY + 1) * MAP_TILE_SIZE - 1, 11, 0xAAAA, 0
	);
#endif
}

tUwCoordYX gameGetCrossPosition(void) {
	UWORD uwMouseX = mouseGetX(MOUSE_PORT_1);
	UWORD uwMouseY = mouseGetY(MOUSE_PORT_1);
	tUwCoordYX sPos = {.uwX = uwMouseX + 8, .uwY = uwMouseY + 14};
	return sPos;
}

tBodyBox *gameGetBox(void) {
	return &s_sBodyBox;
}

//-------------------------------------------------------------------- GAMESTATE

tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
