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

#define PLAYER_VELO_DELTA_X_GROUND 3
#define PLAYER_VELO_DELTA_X_AIR (fix16_one / 4)
#define TRACER_SLIPGATE_SPEED 7
#define TRACER_ITERATIONS_PER_FRAME 4

// Based on http://www.cse.yorku.ca/~amana/research/grid.pdf
typedef struct tTileTracer {
	fix16_t fAccumulatorX; // Helper accumulator for X direction.
	fix16_t fAccumulatorY; // Helper accumulator for Y direction.
	fix16_t fAccumulatorDeltaX;
	fix16_t fAccumulatorDeltaY;
	UWORD uwTileX; // Current tracer pos, in X direction.
	UWORD uwTileY; // Current tracer pos, in Y direction.
	WORD wDeltaTileX;
	WORD wDeltaTileY;
	UBYTE isActive;
	UBYTE ubIndex; // TODO: callback + data
} tTileTracer;

static tTileTracer s_sTracerSlipgate;
static fix16_t s_fPlayerJumpVeloY = F16(-3);

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;

static tBitMap *s_pPlayerFrames;
static tBitMap *s_pPlayerMasks;
static tBitMap *s_pBoxFrames;
static tBitMap *s_pBoxMasks;
static tBitMap *s_pBmCursor;
static tBodyBox s_sBodyPlayer;
static tBodyBox *s_pGrabbedBox;
static tBodyBox s_sBodyBox;
static tSprite *s_pSpriteCrosshair;

static UWORD s_uwGameFrame;
// static char s_szPosX[13];
// static char s_szPosY[13];
// static char s_szVelocityX[13];
// static char s_szVelocityY[13];
// static char s_szAccelerationX[13];
// static char s_szAccelerationY[13];

static UBYTE playerCanJump(void) {
	return s_sBodyPlayer.isOnGround;
}

static tUwCoordYX getCrossPosition(void) {
	UWORD uwMouseX = mouseGetX(MOUSE_PORT_1);
	UWORD uwMouseY = mouseGetY(MOUSE_PORT_1);
	tUwCoordYX sPos = {.uwX = uwMouseX + 8, .uwY = uwMouseY + 14};
	return sPos;
}

static UBYTE playerTryShootSlipgateAt(UBYTE ubIndex, UBYTE ubAngle) {
	tUwCoordYX sDestinationPos = getCrossPosition();
	UWORD uwSourceX = fix16_to_int(s_sBodyPlayer.fPosX) + s_sBodyPlayer.ubWidth / 2;
	UWORD uwSourceY = fix16_to_int(s_sBodyPlayer.fPosY) + s_sBodyPlayer.ubHeight / 2;
	WORD wDeltaX = sDestinationPos.uwX - uwSourceX;
	WORD wDeltaY = sDestinationPos.uwY - uwSourceY;

	if(s_sTracerSlipgate.isActive) {
		return 0;
	}

	s_sTracerSlipgate.wDeltaTileX = (wDeltaX > 0) - (wDeltaX < 0);
	s_sTracerSlipgate.wDeltaTileY = (wDeltaY > 0) - (wDeltaY < 0);
	UWORD uwNextTileX = uwSourceX / MAP_TILE_SIZE + 1;
	UWORD uwNextTileY = uwSourceY / MAP_TILE_SIZE + 1;
	s_sTracerSlipgate.fAccumulatorX = fix16_sub(fix16_from_int(uwNextTileX * MAP_TILE_SIZE), s_sBodyPlayer.fPosX);
	s_sTracerSlipgate.fAccumulatorY = fix16_sub(fix16_from_int(uwNextTileY * MAP_TILE_SIZE), s_sBodyPlayer.fPosY);

	fix16_t fSin = csin(ubAngle);
	fix16_t fCos = ccos(ubAngle);
	s_sTracerSlipgate.fAccumulatorDeltaX = (fCos == 0) ? fix16_from_int(32767) : fix16_abs(fix16_div(fix16_from_int(MAP_TILE_SIZE), ccos(ubAngle)));
	s_sTracerSlipgate.fAccumulatorDeltaY = (fSin == 0) ? fix16_from_int(32767) : fix16_abs(fix16_div(fix16_from_int(MAP_TILE_SIZE), csin(ubAngle)));

	s_sTracerSlipgate.uwTileX = uwSourceX / MAP_TILE_SIZE;
	s_sTracerSlipgate.uwTileY = uwSourceY / MAP_TILE_SIZE;
	s_sTracerSlipgate.isActive = 1;
	s_sTracerSlipgate.ubIndex = ubIndex;

	return 1;
}

static UBYTE tileGetColor(tTile eTile) {
	switch (eTile)
	{
		case TILE_WALL_1: return 7;
		case TILE_SLIPGATE_1: return 8;
		case TILE_SLIPGATE_2: return 13;
		default: return 16;
	}
}

static void drawTile(UBYTE ubTileX, UBYTE ubTileY) {
	UBYTE ubColor = tileGetColor(mapGetTileAt(ubTileX, ubTileY));
	blitRect(
		s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		MAP_TILE_SIZE, MAP_TILE_SIZE, ubColor
	);
	blitRect(
		s_pBufferMain->pFront, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		MAP_TILE_SIZE, MAP_TILE_SIZE, ubColor
	);

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
}

// TODO: refactor and move to map.c?
static void drawMap(void) {
	for(UBYTE ubTileX = 0; ubTileX < MAP_TILE_WIDTH; ++ubTileX) {
		for(UBYTE ubTileY = 0; ubTileY < MAP_TILE_HEIGHT; ++ubTileY) {
			drawTile(ubTileX, ubTileY);
		}
	}
}

static void projectileSlipgateProcess(void) {
	if(!s_sTracerSlipgate.isActive) {
		return;
	}

	for(UBYTE i = TRACER_ITERATIONS_PER_FRAME; i--;) {
		if(s_sTracerSlipgate.fAccumulatorX < s_sTracerSlipgate.fAccumulatorY) {
			s_sTracerSlipgate.fAccumulatorX = fix16_add(s_sTracerSlipgate.fAccumulatorX, s_sTracerSlipgate.fAccumulatorDeltaX);
			s_sTracerSlipgate.uwTileX += s_sTracerSlipgate.wDeltaTileX;
		} else {
			s_sTracerSlipgate.fAccumulatorY = fix16_add(s_sTracerSlipgate.fAccumulatorY, s_sTracerSlipgate.fAccumulatorDeltaY);
			s_sTracerSlipgate.uwTileY += s_sTracerSlipgate.wDeltaTileY;
		}

		UWORD uwPosX = s_sTracerSlipgate.uwTileX * MAP_TILE_SIZE + 3;
		UWORD uwPosY = s_sTracerSlipgate.uwTileY * MAP_TILE_SIZE + 3;

		// Debug draw of tracer trajectory
		// blitRect(s_pBufferMain->pBack, uwPosX, uwPosY, 2, 2, 8);
		// blitRect(s_pBufferMain->pFront, uwPosX, uwPosY, 2, 2, 8);

		UWORD uwTileX = uwPosX / MAP_TILE_SIZE;
		UWORD uwTileY = uwPosY / MAP_TILE_SIZE;

		if(mapIsTileSolid(uwTileX, uwTileY)) {
			s_sTracerSlipgate.isActive = 0;
			UWORD uwSlipgateOldTile1X = g_pSlipgates[s_sTracerSlipgate.ubIndex].uwTileX;
			UWORD uwSlipgateOldTile1Y = g_pSlipgates[s_sTracerSlipgate.ubIndex].uwTileY;
			UWORD uwSlipgateOldTile2X = g_pSlipgates[s_sTracerSlipgate.ubIndex].uwOtherTileX;
			UWORD uwSlipgateOldTile2Y = g_pSlipgates[s_sTracerSlipgate.ubIndex].uwOtherTileY;
			// TODO: tile 2
			if(mapTrySpawnSlipgate(s_sTracerSlipgate.ubIndex, uwTileX, uwTileY)) {
				drawTile(uwSlipgateOldTile1X, uwSlipgateOldTile1Y);
				drawTile(uwSlipgateOldTile2X, uwSlipgateOldTile2Y);
				drawTile(g_pSlipgates[s_sTracerSlipgate.ubIndex].uwTileX, g_pSlipgates[s_sTracerSlipgate.ubIndex].uwTileY);
				drawTile(g_pSlipgates[s_sTracerSlipgate.ubIndex].uwOtherTileX, g_pSlipgates[s_sTracerSlipgate.ubIndex].uwOtherTileY);
				break;
			}
		}
	}
}

static void loadLevel(UBYTE ubIndex) {
	viewLoad(0);
	s_uwGameFrame = 0;
	mapLoad(ubIndex);
	bodyInit(
		&s_sBodyPlayer,
		g_sCurrentLevel.fStartX,
		g_sCurrentLevel.fStartY,
		8, 16
	);
	s_pGrabbedBox = 0;
	bodyInit(
		&s_sBodyBox,
		fix16_from_int(200),
		fix16_from_int(200),
		8, 8
	);
	s_sTracerSlipgate.isActive = 0;
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
	fileWrite(pFile, &s_sBodyPlayer.fPosX, sizeof(s_sBodyPlayer.fPosX));
	fileWrite(pFile, &s_sBodyPlayer.fPosY, sizeof(s_sBodyPlayer.fPosY));
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
		&s_sBodyPlayer.sBob, 16, 16, 1,
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

	tUwCoordYX sPosCross = getCrossPosition();
	s_pSpriteCrosshair->wX = sPosCross.uwX - 8;
	s_pSpriteCrosshair->wY = sPosCross.uwY - 14;
	if(s_pGrabbedBox) {
		s_pGrabbedBox->fPosX = fix16_from_int(sPosCross.uwX - s_pGrabbedBox->ubWidth / 2);
		s_pGrabbedBox->fPosY = fix16_from_int(sPosCross.uwY - s_pGrabbedBox->ubHeight / 2);
	}
	UBYTE ubAimAngle = getAngleBetweenPoints(
		fix16_to_int(s_sBodyPlayer.fPosX), fix16_to_int(s_sBodyPlayer.fPosY),
		sPosCross.uwX, sPosCross.uwY
	);
	if(mouseUse(MOUSE_PORT_1, MOUSE_LMB)) {
		playerTryShootSlipgateAt(0, ubAimAngle);
	}
	else if(mouseUse(MOUSE_PORT_1, MOUSE_RMB)) {
		playerTryShootSlipgateAt(1, ubAimAngle);
	}
	projectileSlipgateProcess();

	// Level editor
	if(keyCheck(KEY_Z)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_WALL_1;
		drawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	else if(keyCheck(KEY_X)) {
		g_sCurrentLevel.pTiles[sPosCross.uwX / MAP_TILE_SIZE][sPosCross.uwY / MAP_TILE_SIZE] = TILE_BG_1;
		drawTile(sPosCross.uwX / MAP_TILE_SIZE, sPosCross.uwY / MAP_TILE_SIZE);
	}
	spriteProcess(s_pSpriteCrosshair);

	// Player
	if(keyUse(KEY_W) && playerCanJump()) {
		s_sBodyPlayer.fVelocityY = s_fPlayerJumpVeloY;
	}

	if(s_sBodyPlayer.isOnGround) {
		s_sBodyPlayer.fVelocityX = 0;
		if(keyCheck(KEY_A)) {
			s_sBodyPlayer.fVelocityX = fix16_from_int(-PLAYER_VELO_DELTA_X_GROUND);
		}
		else if(keyCheck(KEY_D)) {
			s_sBodyPlayer.fVelocityX = fix16_from_int(PLAYER_VELO_DELTA_X_GROUND);
		}
	}
	else {
		if(keyCheck(KEY_A)) {
			if(s_sBodyPlayer.fVelocityX >= 0) {
				s_sBodyPlayer.fVelocityX = fix16_sub(
					s_sBodyPlayer.fVelocityX,
					PLAYER_VELO_DELTA_X_AIR
				);
			}
		}
		else if(keyCheck(KEY_D)) {
			if(s_sBodyPlayer.fVelocityX <= 0) {
				s_sBodyPlayer.fVelocityX = fix16_add(
					s_sBodyPlayer.fVelocityX,
					PLAYER_VELO_DELTA_X_AIR
				);
			}
		}
	}

	if(keyUse(KEY_T)) {
		s_sBodyPlayer.fPosX = fix16_from_int(sPosCross.uwX);
		s_sBodyPlayer.fPosY = fix16_from_int(sPosCross.uwY);
	}
	if(keyUse(KEY_Y)) {
		s_sBodyBox.fPosX = fix16_from_int(sPosCross.uwX);
		s_sBodyBox.fPosY = fix16_from_int(sPosCross.uwY);
	}

	if(keyUse(KEY_F)) {
		UWORD uwBoxX = fix16_to_int(s_sBodyBox.fPosX);
		UWORD uwBoxY = fix16_to_int(s_sBodyBox.fPosY);
		if(s_pGrabbedBox) {
			s_pGrabbedBox = 0;
		}
		else {
			if(
				uwBoxX <= sPosCross.uwX && sPosCross.uwX < uwBoxX + s_sBodyBox.ubWidth &&
				uwBoxY <= sPosCross.uwY && sPosCross.uwY < uwBoxY + s_sBodyBox.ubHeight
			) {
				s_pGrabbedBox = &s_sBodyBox;
			}
		}
	}

	bodySimulate(&s_sBodyBox);
	bobPush(&s_sBodyBox.sBob);
	bodySimulate(&s_sBodyPlayer);
	bobPush(&s_sBodyPlayer.sBob);
	bobPushingDone();
	bobEnd();

	// fix16_to_str(s_sBodyPlayer.fPosX, s_szPosX, 2);
	// fix16_to_str(s_sBodyPlayer.fPosY, s_szPosY, 2);
	// fix16_to_str(s_sBodyPlayer.fVelocityX, s_szVelocityX, 2);
	// fix16_to_str(s_sBodyPlayer.fVelocityY, s_szVelocityY, 2);
	// fix16_to_str(s_sBodyPlayer.fAccelerationX, s_szAccelerationX, 2);
	// fix16_to_str(s_sBodyPlayer.fAccelerationY, s_szAccelerationY, 2);

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

tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
