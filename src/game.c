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
#include <ace/utils/font.h>
#include <ace/managers/key.h>
#include "slipgates.h"
#include "body_box.h"
#include "map.h"
#include "game_math.h"
#include "tile_tracer.h"
#include "player.h"
#include "bouncer.h"
#include "assets.h"

#define GAME_BPP 5

// Hardcoded bitmap widths to prevent runtime multiplications
#define TILESET_BYTE_WIDTH (16 / 8)
#define BUFFER_BYTE_WIDTH (MAP_TILE_WIDTH * MAP_TILE_SIZE / 8)

// DEBUG SWITCHES
// #define GAME_DRAW_GRID
#define GAME_EDITOR_ENABLED

typedef enum tExitState {
	EXIT_NONE,
	EXIT_RESTART,
	EXIT_NEXT,
} tExitState;

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;

static tPlayer s_sPlayer;
static tBodyBox s_pBoxBodies[MAP_BOXES_MAX];
static tSprite *s_pSpriteCrosshair;
static tTextBitMap *s_pTextBuffer;

static UWORD s_uwGameFrame;
static UBYTE s_ubCurrentLevelIndex;
static tExitState s_eExitState;
static BYTE s_bHubActiveDoors;
static UBYTE s_ubHubLevelTens;
static UBYTE s_ubUnlockedLevels;
static UWORD s_uwPrevButtonPresses;

// static char s_szPosX[13];
// static char s_szPosY[13];
// static char s_szVelocityX[13];
// static char s_szVelocityY[13];
// static char s_szAccelerationX[13];
// static char s_szAccelerationY[13];

tTileTracer g_sTracerSlipgate;

// TODO: refactor and move to map.c?
static void drawMap(void) {
	for(UBYTE ubTileX = 0; ubTileX < MAP_TILE_WIDTH; ++ubTileX) {
		for(UBYTE ubTileY = 0; ubTileY < MAP_TILE_HEIGHT; ++ubTileY) {
			gameDrawTile(ubTileX, ubTileY);
		}
	}

	fontDrawStr(
		g_pFont, s_pBufferMain->pBack, 320/2, 0,
		g_sCurrentLevel.szStoryText,
		13, FONT_COOKIE | FONT_HCENTER, s_pTextBuffer
	);
}

static void gameDrawInteractionTiles(const tInteraction *pInteraction) {
	for(UBYTE i = 0; i < pInteraction->ubTargetCount; ++i) {
		mapRequestTileDraw(
			pInteraction->pTargetTiles[i].sPos.ubX,
			pInteraction->pTargetTiles[i].sPos.ubY
		);
	}
}

static UBYTE boxCollisionHandler(
	tTile eTile, UBYTE ubTileX, UBYTE ubTileY, UNUSED_ARG void *pData,
	tDirection eBodyMovementDirection
) {
	UBYTE isColliding = mapTileIsCollidingWithBoxes(eTile);
	if(isColliding) {
		if(mapTileIsButton(eTile)) {
			mapPressButtonAt(ubTileX, ubTileY);
		}
		else if(
			mapTileIsActiveTurret(eTile) && eBodyMovementDirection == DIRECTION_DOWN
		) {
			mapDisableTurretAt(ubTileX, ubTileY);
		}
	}
	return isColliding;
}

static void loadLevel(UBYTE ubIndex, UBYTE isForce) {
	viewLoad(0);
	s_uwGameFrame = 0;
	s_eExitState = EXIT_NONE;
	if(ubIndex == s_ubCurrentLevelIndex && !isForce) {
		mapRestart();
	}
	else {
		s_ubCurrentLevelIndex = ubIndex;

		if(ubIndex != MAP_INDEX_HUB) {
			s_ubUnlockedLevels = MAX(ubIndex, s_ubUnlockedLevels);
			systemUse();
			tFile *pFile = fileOpen("save.dat", "wb");
			fileWrite(pFile, &s_ubUnlockedLevels, sizeof(s_ubUnlockedLevels));
			fileClose(pFile);
			systemUnuse();
		}

		mapLoad(ubIndex);
	}
	bobDiscardUndraw();
	playerReset(&s_sPlayer, g_sCurrentLevel.sSpawnPos.fX, g_sCurrentLevel.sSpawnPos.fY);
	for(UBYTE i = 0; i < MAP_BOXES_MAX; ++i) {
		bodyInit(&s_pBoxBodies[i], 0, 0, 8, 8);
		s_pBoxBodies[i].cbTileCollisionHandler = boxCollisionHandler;
	}
	for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
		s_pBoxBodies[i].fPosX = g_sCurrentLevel.pBoxSpawns[i].fX;
		s_pBoxBodies[i].fPosY = g_sCurrentLevel.pBoxSpawns[i].fY;
	}

	bouncerInit(
		g_sCurrentLevel.ubBouncerSpawnerTileX,
		g_sCurrentLevel.ubBouncerSpawnerTileY
	);
	tracerInit(&g_sTracerSlipgate);

	s_bHubActiveDoors = 0;
	s_uwPrevButtonPresses = 0;

	drawMap();
	blitCopyAligned(
		s_pBufferMain->pBack, 0, 0,
		s_pBufferMain->pFront, 0, 0,
		SCREEN_PAL_WIDTH, SCREEN_PAL_HEIGHT / 2
	);
	blitCopyAligned(
		s_pBufferMain->pBack, 0, SCREEN_PAL_HEIGHT / 2,
		s_pBufferMain->pFront, 0, SCREEN_PAL_HEIGHT / 2,
		SCREEN_PAL_WIDTH, SCREEN_PAL_HEIGHT / 2
	);
	viewLoad(s_pView);
}

static void saveLevel(UBYTE ubIndex) {
	g_sCurrentLevel.sSpawnPos.fX = s_sPlayer.sBody.fPosX;
	g_sCurrentLevel.sSpawnPos.fY = s_sPlayer.sBody.fPosY;

	for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
		g_sCurrentLevel.pBoxSpawns[i].fX = s_pBoxBodies[i].fPosX;
		g_sCurrentLevel.pBoxSpawns[i].fY = s_pBoxBodies[i].fPosY;
	}

	mapSave(ubIndex);
}

static void hubProcess(void) {
	if(s_ubCurrentLevelIndex != MAP_INDEX_HUB) {
		return;
	}

	static const UWORD uwHubButtonPressMask = BV(0) | BV(1) | BV(2);
	UWORD uwButtonPresses = mapGetButtonPresses() & uwHubButtonPressMask;
	if(uwButtonPresses != s_uwPrevButtonPresses) {
		if(uwButtonPresses == BV(0)) { // 0..9
			s_ubHubLevelTens = 0;
		}
		else if(uwButtonPresses == BV(1)) { // 10..19
			s_ubHubLevelTens = 10;
		}
		else if(uwButtonPresses == BV(2)) { // 20..29
			s_ubHubLevelTens = 20;
		}
		else if(uwButtonPresses == BV(3)) { // 20..39
			s_ubHubLevelTens = 30;
		}
		else {
			// Don't change anything
			// s_bHubActiveDoors = 0;
		}
		s_bHubActiveDoors = s_ubUnlockedLevels - s_ubHubLevelTens;
		s_bHubActiveDoors = CLAMP(s_bHubActiveDoors, 0, 10);
		s_uwPrevButtonPresses = uwButtonPresses;
	}

	for(UBYTE i = 0; i < s_bHubActiveDoors; ++i) {
		mapPressButtonIndex(4 + i);
	}
}

static void gameGsCreate(void) {
	gameMathInit();

	s_pView = viewCreate(0,
		TAG_VIEW_COPLIST_MODE, COPPER_MODE_BLOCK,
		TAG_VIEW_GLOBAL_PALETTE, 1,
	TAG_END);

	s_pVpMain = vPortCreate(0,
		TAG_VPORT_BPP, GAME_BPP,
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

	assetsGameCreate();
	s_pTextBuffer = fontCreateTextBitMap(336, g_pFont->uwHeight * 2);
	playerManagerInit();

	bobManagerCreate(s_pBufferMain->pFront, s_pBufferMain->pBack, s_pBufferMain->uBfrBounds.uwY);
	bobInit(
		&s_sPlayer.sBody.sBob, 16, 16, 1,
		g_pPlayerFrames->Planes[0], g_pPlayerMasks->Planes[0], 0, 0
	);
	for(UBYTE i = 0; i < MAP_BOXES_MAX; ++i) {
		bobInit(
			&s_pBoxBodies[i].sBob, 16, 8, 1,
			g_pBoxFrames->Planes[0], g_pBoxMasks->Planes[0], 0, 0
		);
	}
	bobInit(
		&bouncerGetBody()->sBob, 16, 8, 1,
		g_pBouncerFrames->Planes[0], g_pBouncerMasks->Planes[0], 0, 0
	);

	bobReallocateBgBuffers();

	spriteManagerCreate(s_pView, 0);
	s_pSpriteCrosshair = spriteAdd(0, g_pBmCursor);
	systemSetDmaBit(DMAB_SPRITE, 1);
	spriteProcessChannel(0);
	mouseSetBounds(MOUSE_PORT_1, 0, 0, SCREEN_PAL_WIDTH - 16, SCREEN_PAL_HEIGHT - 27);

	s_ubCurrentLevelIndex = 255;
	s_ubUnlockedLevels = 1;
	tFile *pFile = fileOpen("save.dat", "rb");
	if(pFile) {
		fileRead(pFile, &s_ubUnlockedLevels, sizeof(s_ubUnlockedLevels));
		fileClose(pFile);
	}
	s_ubUnlockedLevels = 15;

	systemUnuse();
	loadLevel(0, 1);
}

static void gameGsLoop(void) {
	g_pCustom->color[0] = 0xF89;

	if(s_eExitState != EXIT_NONE) {
		if(s_eExitState == EXIT_NEXT) {
			loadLevel(++s_ubCurrentLevelIndex, 1);
		}
		else if(s_eExitState == EXIT_RESTART) {
			loadLevel(s_ubCurrentLevelIndex, 0);
		}
		return;
	}

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
					loadLevel(1 + i, 1);
				}
				break;
			}
		}
	}

 	bobBegin(s_pBufferMain->pBack);

	// Custom hub button presses after all bodies have been simulated
	// and before mapProcess() have erased button press states.
	hubProcess();

	mapProcess();
	tUwCoordYX sPosCross = gameGetCrossPosition();
	s_pSpriteCrosshair->wX = sPosCross.uwX - 8;
	s_pSpriteCrosshair->wY = sPosCross.uwY - 14;

	// Level editor
#if defined(GAME_EDITOR_ENABLED)
	UWORD uwCursorTileX = sPosCross.uwX / MAP_TILE_SIZE;
	UWORD uwCursorTileY = sPosCross.uwY / MAP_TILE_SIZE;
	tTile *pTileUnderCursor = &g_sCurrentLevel.pTiles[uwCursorTileX][uwCursorTileY];
	if(keyCheck(KEY_Z)) {
		*pTileUnderCursor = TILE_BG;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_X)) {
		*pTileUnderCursor = TILE_WALL;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_C)) {
		*pTileUnderCursor = TILE_WALL_NO_SLIPGATE;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_V)) {
		*pTileUnderCursor = TILE_GRATE;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_B)) {
		*pTileUnderCursor = TILE_DEATH_FIELD;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_N)) {
		*pTileUnderCursor = TILE_EXIT;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_M)) {
		if(mapTileIsButton(*pTileUnderCursor)) {
			if(keyUse(KEY_M)) {
				if(*pTileUnderCursor == TILE_BUTTON_H) {
					*pTileUnderCursor = TILE_BUTTON_A;
				}
				else {
					++*pTileUnderCursor;
				}
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
			}
		}
		else {
			keyUse(KEY_M); // prevent double-processing of same tile
			*pTileUnderCursor = TILE_BUTTON_A;
			mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
		}
	}
	else if(keyCheck(KEY_COMMA)) {
		*pTileUnderCursor = TILE_DOOR_CLOSED;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyUse(KEY_PERIOD)) {
		*pTileUnderCursor = TILE_RECEIVER;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyUse(KEY_SLASH)) {
		if(g_sCurrentLevel.ubBouncerSpawnerTileX != BOUNCER_TILE_INVALID) {
			g_sCurrentLevel.pTiles[g_sCurrentLevel.ubBouncerSpawnerTileX][g_sCurrentLevel.ubBouncerSpawnerTileY] = TILE_WALL;
			mapRecalculateVisTilesNearTileAt(
				g_sCurrentLevel.ubBouncerSpawnerTileX,
				g_sCurrentLevel.ubBouncerSpawnerTileY
			);
		}

		*pTileUnderCursor = TILE_BOUNCER_SPAWNER;
		g_sCurrentLevel.ubBouncerSpawnerTileX = uwCursorTileX;
		g_sCurrentLevel.ubBouncerSpawnerTileY = uwCursorTileY;
		bouncerInit(
			g_sCurrentLevel.ubBouncerSpawnerTileX,
			g_sCurrentLevel.ubBouncerSpawnerTileY
		);
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_L)) {
		*pTileUnderCursor = TILE_SLIPGATABLE_OFF;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyUse(KEY_K)) {
		mapAddOrRemoveSpikeTile(uwCursorTileX, uwCursorTileY);
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY - 1);
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyUse(KEY_H)) {
		mapAddOrRemoveTurret(uwCursorTileX, uwCursorTileY, DIRECTION_LEFT);
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyUse(KEY_J)) {
		mapAddOrRemoveTurret(uwCursorTileX, uwCursorTileY, DIRECTION_RIGHT);
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}

	for(UBYTE i = 0; i < MAP_USER_INTERACTIONS_MAX; ++i) {
		static const UBYTE pInteractionKeys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUALS, KEY_BACKSPACE, KEY_RETURN};
		if(keyUse(pInteractionKeys[i])) {
			if(keyCheck(KEY_CONTROL)) {
				// Remove tile from current interaction if it's already assigned
				tInteraction *pOldInteraction = mapGetInteractionByTile(uwCursorTileX, uwCursorTileY);
				if(pOldInteraction) {
					interactionAddOrRemoveTile(
						pOldInteraction, uwCursorTileX, uwCursorTileY, INTERACTION_KIND_GATE,
						TILE_DOOR_OPEN, TILE_DOOR_CLOSED
					);
				}

				// Reassign to interaction group if other
				tInteraction *pInteraction = mapGetInteractionByIndex(i);
				if(pInteraction && pInteraction != pOldInteraction) {
					if(*pTileUnderCursor == TILE_DOOR_OPEN || *pTileUnderCursor == TILE_DOOR_CLOSED) {
						interactionAddOrRemoveTile(
							pInteraction, uwCursorTileX, uwCursorTileY, INTERACTION_KIND_GATE,
							TILE_DOOR_OPEN, TILE_DOOR_CLOSED
						);
					}
					else if(*pTileUnderCursor == TILE_SLIPGATABLE_OFF || *pTileUnderCursor == TILE_SLIPGATABLE_ON) {
						interactionAddOrRemoveTile(
							pInteraction, uwCursorTileX, uwCursorTileY, INTERACTION_KIND_SLIPGATABLE,
							TILE_SLIPGATABLE_ON, TILE_SLIPGATABLE_OFF
						);
					}
					gameDrawInteractionTiles(pInteraction);
				}
			}
			else {
				// change interaction group's activation mask
				tInteraction *pInteraction = mapGetInteractionByTile(
					uwCursorTileX, uwCursorTileY
				);
				if(pInteraction) {
					pInteraction->uwButtonMask ^= BV(i);
					gameDrawInteractionTiles(pInteraction);
				}
			}

			// Could be no longer part of interaction
			mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
			break;
		}
	}

	// Debug stuff
	if(keyUse(KEY_T)) {
		bodyTeleport(&s_sPlayer.sBody, sPosCross.uwX, sPosCross.uwY);
	}
	if(keyUse(KEY_Y)) {
		if(g_sCurrentLevel.ubBoxCount < MAP_BOXES_MAX) {
			bodyTeleport(&s_pBoxBodies[g_sCurrentLevel.ubBoxCount++], sPosCross.uwX, sPosCross.uwY);
		}
	}
	if(keyUse(KEY_U)) {
		bodyTeleport(bouncerGetBody(), sPosCross.uwX, sPosCross.uwY);
	}
#endif

	spriteProcess(s_pSpriteCrosshair);
	playerProcess(&s_sPlayer);

	if(keyUse(KEY_G)) {
		s_eExitState = EXIT_RESTART;
	}

	for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
		bodySimulate(&s_pBoxBodies[i]);
		bobPush(&s_pBoxBodies[i].sBob);
	}

	if(bouncerProcess()) {
		bobPush(&bouncerGetBody()->sBob);
	}

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

	fontDestroyTextBitMap(s_pTextBuffer);
	assetsGameDestroy();
}

static void gameDrawTileInteractionMask(UBYTE ubTileX, UBYTE ubTileY, UWORD uwMask) {
	UBYTE ubBitIndex = 0;
	blitRect(s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE + 1, ubTileY * MAP_TILE_SIZE + 1, 6, 6, 15);
	while(uwMask) {
		if(uwMask & 1) {
			UBYTE ubColor = (ubBitIndex >= 9) ? 8 : 1;
			UBYTE ubIndicatorX = 1 + (ubBitIndex % 3) * 2;
			UBYTE ubIndicatorY = 1 + ((ubBitIndex % 9) / 3) * 2;
			blitRect(
				s_pBufferMain->pBack,
				ubTileX * MAP_TILE_SIZE + ubIndicatorX,
				ubTileY * MAP_TILE_SIZE + ubIndicatorY,
				1, 1, ubColor
			);
		}

		++ubBitIndex;
		uwMask >>= 1;
	}
}

tBodyBox *gameGetBoxAt(UWORD uwX, UWORD uwY) {
	for(UBYTE i = 0; i < g_sCurrentLevel.ubBoxCount; ++i) {
		tBodyBox *pBox = &s_pBoxBodies[i];
		UWORD uwBoxX = fix16_to_int(pBox->fPosX);
		UWORD uwBoxY = fix16_to_int(pBox->fPosY);
		if(
			uwBoxX < uwX && uwX < uwBoxX + pBox->ubWidth &&
			uwBoxY < uwY && uwY < uwBoxY + pBox->ubHeight
		) {
			return pBox;
		}
	}
	return 0;
}

//------------------------------------------------------------------- PUBLIC FNS

// TODO: move to map.c?
void gameDrawTile(UBYTE ubTileX, UBYTE ubTileY) {
	tTile eTile = mapGetTileAt(ubTileX, ubTileY);
	UWORD uwTileIndex = mapGetVisTileAt(ubTileX, ubTileY);

	// A: tile mask, B: tile source, C/D: bg
	UWORD uwHeight = MAP_TILE_SIZE * g_pBmTiles->Depth;
	UWORD uwBlitWords = 1;
	WORD wSrcModulo = TILESET_BYTE_WIDTH - (uwBlitWords<<1);
	WORD wDstModulo = BUFFER_BYTE_WIDTH - (uwBlitWords<<1);
	ULONG ulSrcOffs = uwTileIndex * MAP_TILE_SIZE * TILESET_BYTE_WIDTH * GAME_BPP; // + (wSrcX>>3);
	ULONG ulDstOffs = ubTileY * MAP_TILE_SIZE * BUFFER_BYTE_WIDTH * GAME_BPP + ((ubTileX * MAP_TILE_SIZE) >>3);
	UBYTE ubShift = (ubTileX & 1) ? 8 : 0;
	UWORD uwBltCon0 = (ubShift << ASHIFTSHIFT) | USEB|USEC|USED | MINTERM_COOKIE;
	UWORD uwBltCon1 = ubShift << BSHIFTSHIFT;

	blitWait(); // Don't modify registers when other blit is in progress
	g_pCustom->bltcon0 = uwBltCon0;
	g_pCustom->bltcon1 = uwBltCon1;
	g_pCustom->bltafwm = 0xFF00;
	g_pCustom->bltalwm = 0xFF00;
	g_pCustom->bltbmod = wSrcModulo;
	g_pCustom->bltcmod = wDstModulo;
	g_pCustom->bltdmod = wDstModulo;
	g_pCustom->bltadat = 0xFF00;

	// TODO: all of above regs could be calculated/set once before triggering
	// batch tiles draw, but only if editor/debug mode overlays are not needed.
	g_pCustom->bltbpt = (UBYTE*)((ULONG)g_pBmTiles->Planes[0] + ulSrcOffs);
	g_pCustom->bltcpt = (UBYTE*)((ULONG)s_pBufferMain->pBack->Planes[0] + ulDstOffs);
	g_pCustom->bltdpt = (UBYTE*)((ULONG)s_pBufferMain->pBack->Planes[0] + ulDstOffs);
	g_pCustom->bltsize = (uwHeight << HSIZEBITS) | uwBlitWords;

#if defined(GAME_EDITOR_ENABLED)
	if(mapTileIsButton(eTile)) {
		UBYTE ubButtonIndex = (eTile & MAP_TILE_INDEX_MASK) - (TILE_BUTTON_A & MAP_TILE_INDEX_MASK);
		gameDrawTileInteractionMask(ubTileX, ubTileY, BV(ubButtonIndex));
	}
	else if(eTile == TILE_RECEIVER) {
		gameDrawTileInteractionMask(ubTileX, ubTileY, BV(MAP_BOUNCER_BUTTON_INDEX));
	}
	else {
		tInteraction *pInteraction = mapGetInteractionByTile(ubTileX, ubTileY);
		if(pInteraction) {
			gameDrawTileInteractionMask(ubTileX, ubTileY, pInteraction->uwButtonMask);
		}
	}
#endif

#if defined(GAME_DRAW_GRID)
	blitLine(
		s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
		(ubTileX + 1) * MAP_TILE_SIZE - 1, ubTileY * MAP_TILE_SIZE, 11, 0xAAAA, 0
	);
	blitLine(
		s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
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

void gameMarkExitReached(UBYTE ubTileX, UBYTE ubTileY) {
	if(s_ubCurrentLevelIndex == MAP_INDEX_HUB) {
		UBYTE ubHubLevelOnes = 0;
		if(ubTileX == 2) { // leftmost: 0, 2, 4, 6
			if(ubTileY <= 5) {
				ubHubLevelOnes = 6;
			}
			else if(ubTileY <= 11) {
				ubHubLevelOnes = 4;
			}
			else if(ubTileY <= 17) {
				ubHubLevelOnes = 2;
			}
			else if(ubTileY <= 23) {
				ubHubLevelOnes = 0;
			}
		}
		else if(ubTileX == 14) { // middle left
				ubHubLevelOnes = 8;
		}
		else if(ubTileX == 25) { // middle right
				ubHubLevelOnes = 9;
		}
		else if(ubTileX == 37) { // rightmost: 1, 3, 5, 7
			if(ubTileY <= 5) {
				ubHubLevelOnes = 7;
			}
			else if(ubTileY <= 11) {
				ubHubLevelOnes = 5;
			}
			else if(ubTileY <= 17) {
				ubHubLevelOnes = 3;
			}
			else if(ubTileY <= 23) {
				ubHubLevelOnes = 1;
			}
		}

		// Subtract level index so that exit transition will increment to to proper one
		s_ubCurrentLevelIndex = s_ubHubLevelTens + ubHubLevelOnes - 1;
	}
	s_eExitState = EXIT_NEXT;
}

tPlayer *gameGetPlayer(void) {
	return &s_sPlayer;
}

tSimpleBufferManager *gameGetBuffer(void) {
	return s_pBufferMain;
}

//-------------------------------------------------------------------- GAMESTATE

tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
