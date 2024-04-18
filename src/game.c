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
#include "debug.h"
#include "menu.h"
#include "cutscene.h"
#include "config.h"

#define GAME_BPP 5

// Hardcoded bitmap widths to prevent runtime multiplications
#define TILESET_BYTE_WIDTH (16 / 8)
#define BUFFER_BYTE_WIDTH (MAP_TILE_WIDTH * MAP_TILE_SIZE / 8)
#define SLIPGATE_FRAME_VERTICAL 0
#define SLIPGATE_FRAME_HORIZONTAL 1
#define SLIPGATE_FRAME_COUNT 2

#define SLIPGATE_FRAME_HEIGHT_VERTICAL 16
#define SLIPGATE_FRAME_HEIGHT_HORIZONTAL 4
#define GAME_COLOR_TEXT 25

// BUILD SWITCHES
#define GAME_EDITOR_ENABLED

typedef enum tExitState {
	EXIT_NONE,
	EXIT_RESTART,
	EXIT_NEXT,
	EXIT_HUB,
	EXIT_MENU,
} tExitState;

typedef enum tEditorTool {
	EDITOR_TOOL_WALL,
	EDITOR_TOOL_WALL_BLOCKED,
	EDITOR_TOOL_GRATE,
	EDITOR_TOOL_DEATH_FIELD,
	EDITOR_TOOL_EXIT,
	EDITOR_TOOL_DOOR,
	EDITOR_TOOL_RECEIVER,
	EDITOR_TOOL_WALL_TOGGLABLE,
	EDITOR_TOOL_PIPE,
	EDITOR_TOOL_EXIT_HUB,
	EDITOR_TOOL_BUTTON,
	EDITOR_TOOL_BOUNCER,
	EDITOR_TOOL_SPIKE,
	EDITOR_TOOL_TURRET_LEFT,
	EDITOR_TOOL_TURRET_RIGHT,
	EDITOR_TOOL_COUNT
} tEditorTool;

__attribute__((unused))
static const char *s_pToolNames[] = {
	"Wall",
	"Wall blocked",
	"Grate",
	"Death field",
	"Exit",
	"Door",
	"Receiver",
	"Wall togglable",
	"Pipe",
	"Exit hub",
	"Button",
	"Bouncer",
	"Spike",
	"Turret left",
	"Turret right",
};

static tView *s_pView;
static tVPort *s_pVpMain;
static tSimpleBufferManager *s_pBufferMain;
static tFade *s_pFade;

static tPlayer s_sPlayer;
static tBodyBox s_pBoxBodies[MAP_BOXES_MAX];
static tSprite *s_pSpriteCrosshair;
static tBob s_sBobAim;
static tTextBitMap *s_pTextBuffer;

static UWORD s_uwGameFrame;
static tExitState s_eExitState;
static BYTE s_bHubActiveDoors;
static UBYTE s_ubHubLevelTens;
static UWORD s_uwPrevButtonPresses;
static UWORD s_pPalettes[PLAYER_MAX_HEALTH + 1][1 << GAME_BPP];
static UBYTE s_ubCurrentPaletteIndex;

static UBYTE s_isEditorEnabled;
static UBYTE s_isEditorDrawGrid;
static UBYTE s_isEditorDrawInteractions;
static tEditorTool s_eEditorCurrentTool;
static tUbCoordYX s_sEditorPrevCursorTilePos;

static tState s_sStateTilePalette;

static const tBCoordYX s_pSlipgateOffsets[DIRECTION_COUNT] = {
	[DIRECTION_LEFT] = {.bX = -2, .bY = 0},
	[DIRECTION_RIGHT] = {.bX = 5, .bY = 0},
	[DIRECTION_UP] = {.bX = 0, .bY = -2},
	[DIRECTION_DOWN] = {.bX = 0, .bY = 6},
};

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
		GAME_COLOR_TEXT, FONT_COOKIE | FONT_HCENTER, s_pTextBuffer
	);
}

static void gameRequestInteractionTilesDraw(const tInteraction *pInteraction) {
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
	if(ubIndex == g_sConfig.ubCurrentLevel && !isForce) {
		mapRestart();
	}
	else {
		g_sConfig.ubCurrentLevel = ubIndex;

		if(ubIndex != MAP_INDEX_HUB) {
			g_sConfig.ubUnlockedLevels = MAX(ubIndex, g_sConfig.ubUnlockedLevels);
			configSave();
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

	s_ubCurrentPaletteIndex = PLAYER_MAX_HEALTH;

	viewLoad(s_pView);
	fadeChangeRefPalette(s_pFade, s_pPalettes[s_ubCurrentPaletteIndex], 1 << GAME_BPP);
	fadeStart(s_pFade, FADE_STATE_IN, 15, 0, 0);
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
	if(g_sConfig.ubCurrentLevel != MAP_INDEX_HUB) {
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
		s_bHubActiveDoors = g_sConfig.ubUnlockedLevels - s_ubHubLevelTens;
		s_bHubActiveDoors = CLAMP(s_bHubActiveDoors, 0, 10);
		s_uwPrevButtonPresses = uwButtonPresses;
	}

	for(UBYTE i = 0; i < s_bHubActiveDoors; ++i) {
		mapPressButtonIndex(4 + i);
	}
}

static void gameTileRefreshAll(void) {
	for(UBYTE ubTileX = 0; ubTileX < MAP_TILE_WIDTH; ++ubTileX) {
		for(UBYTE ubTileY = 0; ubTileY < MAP_TILE_HEIGHT; ++ubTileY) {
			mapRequestTileDraw(ubTileX, ubTileY);
		}
	}
	bobDiscardUndraw();
	bobSetCurrentBuffer(s_pBufferMain->pBack);
}

static void gameToggleEditorOption(UBYTE *pOption) {
	*pOption = !*pOption;
	gameTileRefreshAll();
}

static void paletteToRed(
	const UWORD *pPaletteIn, UWORD *pPaletteRed, UBYTE ubColorCount
) {
	for(UBYTE i = 0; i < ubColorCount; ++i) {
		pPaletteRed[i] = pPaletteIn[i] & 0xF00;
	}
}

static void paletteInterpolate(
	const UWORD *pPaletteA, const UWORD *pPaletteB, UWORD *pPaletteOut,
	UBYTE ubColorCount, UBYTE ubRatioNum, UBYTE ubRatioDen
) {
	for(UBYTE i = 0; i < ubColorCount; ++i) {
		BYTE bRA = (pPaletteA[i] >> 8);
		BYTE bGA = (pPaletteA[i] >> 4) & 0xF;
		BYTE bBA = (pPaletteA[i] >> 0) & 0xF;
		BYTE bRB = (pPaletteB[i] >> 8);
		BYTE bGB = (pPaletteB[i] >> 4) & 0xF;
		BYTE bBB = (pPaletteB[i] >> 0) & 0xF;
		UBYTE ubR = bRA + ((bRB - bRA) * ubRatioNum) / ubRatioDen;
		UBYTE ubG = bGA + ((bGB - bGA) * ubRatioNum) / ubRatioDen;
		UBYTE ubB = bBA + ((bBB - bBA) * ubRatioNum) / ubRatioDen;
		pPaletteOut[i] = (ubR << 8) | (ubG << 4) | ubB;
	}
}

void gameProcessExit(void) {
	if(s_eExitState != EXIT_NONE) {
		if(s_eExitState == EXIT_NEXT) {
			if(g_sConfig.ubCurrentLevel == MAP_INDEX_LAST) {
				cutsceneSetup(1, &g_sStateMenu);
				stateChange(g_pGameStateManager, &g_sStateCutscene);
			}
			else {
				loadLevel(g_sConfig.ubCurrentLevel + 1, 1);
			}
		}
		else if(s_eExitState == EXIT_HUB) {
			loadLevel(MAP_INDEX_HUB, 1);
		}
		else if(s_eExitState == EXIT_RESTART) {
			loadLevel(g_sConfig.ubCurrentLevel, 0);
		}
		else if(s_eExitState == EXIT_MENU) {
			stateChange(g_pGameStateManager, &g_sStateMenu);
		}
		return;
	}
}

static void onGameFadeOut(void) {
	gameProcessExit();
}

static void gameTransitionToExit(tExitState eExitState) {
	if(s_eExitState != EXIT_NONE) {
		return;
	}

	s_eExitState = eExitState;
	fadeChangeRefPalette(s_pFade, s_pPalettes[s_ubCurrentPaletteIndex], 1 << GAME_BPP);
	fadeStart(s_pFade, FADE_STATE_OUT, 15, 0, onGameFadeOut);
}

static UBYTE gameProcessEditor(void) {
	tUwCoordYX sPosCross = gameGetCrossPosition();
	UWORD uwCursorTileX = sPosCross.uwX / MAP_TILE_SIZE;
	UWORD uwCursorTileY = sPosCross.uwY / MAP_TILE_SIZE;

	if(uwCursorTileX != s_sEditorPrevCursorTilePos.ubX || uwCursorTileY != s_sEditorPrevCursorTilePos.ubY) {
		mapRequestTileDraw(s_sEditorPrevCursorTilePos.ubX, s_sEditorPrevCursorTilePos.ubY);
		s_sEditorPrevCursorTilePos.ubX = uwCursorTileX;
		s_sEditorPrevCursorTilePos.ubY = uwCursorTileY;
	}

	blitRect(s_pBufferMain->pBack, uwCursorTileX * MAP_TILE_SIZE, uwCursorTileY * MAP_TILE_SIZE, MAP_TILE_SIZE, 1, 15);
	blitRect(s_pBufferMain->pBack, uwCursorTileX * MAP_TILE_SIZE, uwCursorTileY * MAP_TILE_SIZE, 1, MAP_TILE_SIZE, 15);
	blitRect(s_pBufferMain->pBack, uwCursorTileX * MAP_TILE_SIZE + MAP_TILE_SIZE - 1, uwCursorTileY * MAP_TILE_SIZE, 1, MAP_TILE_SIZE, 15);
	blitRect(s_pBufferMain->pBack, uwCursorTileX * MAP_TILE_SIZE, uwCursorTileY * MAP_TILE_SIZE + MAP_TILE_SIZE - 1, MAP_TILE_SIZE, 1, 15);

	if(keyUse(KEY_C)) {
		statePush(g_pGameStateManager, &s_sStateTilePalette);
		return 1;
	}

	tTile *pTileUnderCursor = &g_sCurrentLevel.pTiles[uwCursorTileX][uwCursorTileY];
	if(keyCheck(KEY_Z)) {
		*pTileUnderCursor = TILE_BG;
		mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
	}
	else if(keyCheck(KEY_X)) {
		switch(s_eEditorCurrentTool) {
			case EDITOR_TOOL_WALL:
				*pTileUnderCursor = TILE_WALL;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_WALL_BLOCKED:
				*pTileUnderCursor = TILE_WALL_BLOCKED;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_GRATE:
				*pTileUnderCursor = TILE_GRATE;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_DEATH_FIELD:
				*pTileUnderCursor = TILE_DEATH_FIELD;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_EXIT:
				*pTileUnderCursor = TILE_EXIT;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_DOOR:
				*pTileUnderCursor = TILE_DOOR_CLOSED;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_RECEIVER:
				*pTileUnderCursor = TILE_RECEIVER;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_WALL_TOGGLABLE:
				*pTileUnderCursor = TILE_WALL_TOGGLABLE_OFF;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_PIPE:
				*pTileUnderCursor = TILE_PIPE;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_EXIT_HUB:
				*pTileUnderCursor = TILE_EXIT_HUB;
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;

			case EDITOR_TOOL_BUTTON:
				if(mapTileIsButton(*pTileUnderCursor)) {
					if(keyUse(KEY_X)) {
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
					keyUse(KEY_X); // prevent double-processing of same tile
					*pTileUnderCursor = TILE_BUTTON_A;
					mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				}
				break;
			case EDITOR_TOOL_BOUNCER:
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
				break;
			case EDITOR_TOOL_SPIKE:
				mapAddOrRemoveSpikeTile(uwCursorTileX, uwCursorTileY);
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY - 1);
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_TURRET_LEFT:
				mapAddOrRemoveTurret(uwCursorTileX, uwCursorTileY, DIRECTION_LEFT);
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			case EDITOR_TOOL_TURRET_RIGHT:
				mapAddOrRemoveTurret(uwCursorTileX, uwCursorTileY, DIRECTION_RIGHT);
				mapRecalculateVisTilesNearTileAt(uwCursorTileX, uwCursorTileY);
				break;
			default:
				break;
		}
	}

	for(UBYTE i = 0; i < MAP_USER_INTERACTIONS_MAX; ++i) {
		static const UBYTE pInteractionKeys[] = {
			KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
			KEY_MINUS, KEY_EQUALS, KEY_BACKSPACE, KEY_RETURN
		};
		if(keyUse(pInteractionKeys[i])) {
			if(keyCheck(KEY_CONTROL)) {
				tInteraction *pOldInteraction = mapGetInteractionByTile(uwCursorTileX, uwCursorTileY);
				tInteraction *pInteraction = mapGetInteractionByIndex(i);

				if(pOldInteraction) {
					gameRequestInteractionTilesDraw(pOldInteraction);
				}

				if(*pTileUnderCursor == TILE_DOOR_CLOSED) {
					mapSetOrRemoveDoorInteractionAt(i, uwCursorTileX, uwCursorTileY);
				}
				else if(*pTileUnderCursor == TILE_WALL_TOGGLABLE_OFF || *pTileUnderCursor == TILE_WALL_TOGGLABLE_ON) {
					interactionChangeOrRemoveTile(
						pOldInteraction, pInteraction,
						uwCursorTileX, uwCursorTileY, INTERACTION_KIND_SLIPGATABLE,
						TILE_WALL_TOGGLABLE_ON, TILE_WALL_TOGGLABLE_OFF,
						VIS_TILE_SLIPGATABLE_ON_1, VIS_TILE_SLIPGATABLE_OFF_1
					);
				}
				gameRequestInteractionTilesDraw(pInteraction);
			}
			else {
				// change interaction group's activation mask
				tInteraction *pInteraction = mapGetInteractionByTile(
					uwCursorTileX, uwCursorTileY
				);
				if(pInteraction) {
					pInteraction->uwButtonMask ^= BV(i);
					gameRequestInteractionTilesDraw(pInteraction);
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
		if(g_sCurrentLevel.ubBoxCount && !s_sPlayer.pGrabbedBox) {
			--g_sCurrentLevel.ubBoxCount;
		}
	}

	return 0;
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

void tilePaletteDraw(tBitMap *pDestination, UWORD uwElementCount) {
	const UWORD uwCols = 4;
	const UWORD uwRows = (uwElementCount + uwCols - 1) / uwCols;
	const UWORD uwWidth = 320 - 64;
	const UWORD uwHeight = 256 - 64;
	const UWORD uwButtonSpaceX = uwWidth / uwCols;
	const UWORD uwButtonSpaceY = uwHeight / uwRows;
	blitRect(pDestination, 32, 32, uwWidth, uwHeight, 0);

	for(UBYTE i = 0; i < uwElementCount; ++i) {
		UWORD uwLeft = 32 + uwButtonSpaceX * (i % uwCols);
		UWORD uwTop = 32 + uwButtonSpaceY * (i / uwCols);
		blitRect(
			pDestination,
			uwLeft + 8,
			uwTop + 4,
			uwButtonSpaceX - 16,
			uwButtonSpaceY - 8,
			5
		);
		fontDrawStr(
			g_pFont,
			pDestination,
			uwLeft + uwButtonSpaceX / 2,
			uwTop + uwButtonSpaceY / 2,
			s_pToolNames[i],
			15,
			FONT_COOKIE | FONT_SHADOW | FONT_CENTER,
			s_pTextBuffer
		);
	}
}

//-------------------------------------------------------------------- GAMESTATE

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
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_INTERLEAVED,
		TAG_SIMPLEBUFFER_IS_DBLBUF, 1,
		TAG_SIMPLEBUFFER_VPORT, s_pVpMain,
	TAG_END);

	paletteLoad("data/slipgates.plt", s_pPalettes[PLAYER_MAX_HEALTH], 32);
	UBYTE ubPaletteIndexLast = PLAYER_MAX_HEALTH;
	paletteToRed(s_pPalettes[ubPaletteIndexLast], s_pPalettes[0], 32);
	for(UBYTE i = 0; i < ubPaletteIndexLast; ++i) {
		paletteInterpolate(
			s_pPalettes[0], s_pPalettes[ubPaletteIndexLast],
			s_pPalettes[i], 32,
			i, ubPaletteIndexLast
		);
	}

	s_pFade = fadeCreate(s_pView, s_pPalettes[PLAYER_MAX_HEALTH], 1 << GAME_BPP);

	assetsGameCreate();
	s_pTextBuffer = fontCreateTextBitMap(336, g_pFont->uwHeight * 2);
	playerManagerInit();

	bobManagerCreate(s_pBufferMain->pFront, s_pBufferMain->pBack, s_pBufferMain->uBfrBounds.uwY);
	bobInit(
		&s_sPlayer.sBody.sBob, 16, 16, 1,
		g_pPlayerFrames->Planes[0], g_pPlayerMasks->Planes[0], 0, 0
	);
	bobInit(&s_sBobAim, 16, 16, 1, 0, 0, 0, 0);

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
	mouseSetBounds(MOUSE_PORT_1, 0, 0, SCREEN_PAL_WIDTH - 17, SCREEN_PAL_HEIGHT - 27);

	s_isEditorEnabled = 0;
	s_isEditorDrawGrid = 0;
	s_isEditorDrawInteractions = 0;
	s_sEditorPrevCursorTilePos.uwYX = 0;
	s_eEditorCurrentTool = EDITOR_TOOL_WALL;

	systemUnuse();
	loadLevel(g_sConfig.ubCurrentLevel, 1);
}

static void gameGsLoop(void) {
	tFadeState eFadeState = fadeProcess(s_pFade);
	if(eFadeState == FADE_STATE_EVENT_FIRED) {
		return;
	}

	debugSetColor(0xF89);
 	bobBegin(s_pBufferMain->pBack);

	if(keyUse(KEY_ESCAPE)) {
		gameTransitionToExit(EXIT_MENU);
	}
	if(s_isEditorEnabled) {
		if(keyUse(KEY_F10)) {
			loadLevel(MAP_INDEX_DEVELOP, 1);
		}
		else {
			for(UBYTE i = 0; i < 9; ++i) {
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
		if(keyUse(KEY_RBRACKET)) {
			gameToggleEditorOption(&s_isEditorDrawGrid);
		}
		if(keyUse(KEY_P)) {
			gameToggleEditorOption(&s_isEditorDrawInteractions);
		}
	}

	if(keyUse(KEY_LBRACKET)) {
		debugToggle();
	}
#if defined(GAME_EDITOR_ENABLED)
	if(keyUse(KEY_O)) {
		gameToggleEditorOption(&s_isEditorEnabled);
	}
#endif

	// Custom hub button presses after all bodies have been simulated
	// and before mapProcess() have erased button press states.
	hubProcess();

	mapProcess();
	tUwCoordYX sPosCross = gameGetCrossPosition();
	s_pSpriteCrosshair->wX = sPosCross.uwX - 8;
	s_pSpriteCrosshair->wY = sPosCross.uwY - 14;

	// Level editor
	if(s_isEditorEnabled) {
		if(gameProcessEditor()) {
			return;
		}
	}

	spriteProcess(s_pSpriteCrosshair);
	playerProcess(&s_sPlayer);

	if(keyUse(KEY_R) || (!s_sPlayer.bHealth && mouseUse(MOUSE_PORT_1, MOUSE_LMB))) {
		gameTransitionToExit(EXIT_RESTART);
	}

	if(g_pSlipgates[SLIPGATE_AIM].eNormal != DIRECTION_NONE && !s_sPlayer.pGrabbedBox) {
		bobPush(&s_sBobAim);
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
	debugSetColor(0x9F8);
	systemIdleBegin();
	vPortWaitForEnd(s_pVpMain);
	systemIdleEnd();

	if(eFadeState == FADE_STATE_IDLE) {
		if(s_ubCurrentPaletteIndex != s_sPlayer.bHealth) {
			s_ubCurrentPaletteIndex = s_sPlayer.bHealth;
			for(UBYTE i = 0; i < (1 << GAME_BPP); ++i) {
				g_pCustom->color[i] = s_pPalettes[s_ubCurrentPaletteIndex][i];
			}
		}
	}
}

static void gameGsDestroy(void) {
	viewLoad(0);
	systemUse();

	fadeDestroy(s_pFade);
	systemSetDmaBit(DMAB_SPRITE, 0);
	spriteManagerDestroy();
	viewDestroy(s_pView);
	bobManagerDestroy();

	fontDestroyTextBitMap(s_pTextBuffer);
	assetsGameDestroy();
}

static void tilePaletteGsCreate(void) {
	tilePaletteDraw(s_pBufferMain->pBack, EDITOR_TOOL_COUNT);
	tilePaletteDraw(s_pBufferMain->pFront, EDITOR_TOOL_COUNT);
}

static void tilePaletteGsLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		statePop(g_pGameStateManager);
		return;
	}

	const UWORD uwElementCount = EDITOR_TOOL_COUNT;
	const UWORD uwCols = 4;
	const UWORD uwRows = (uwElementCount + uwCols - 1) / uwCols;
	const UWORD uwWidth = 320 - 64;
	const UWORD uwHeight = 256 - 64;
	const UWORD uwButtonSpaceX = uwWidth / uwCols;
	const UWORD uwButtonSpaceY = uwHeight / uwRows;

	tUwCoordYX sPosCross = gameGetCrossPosition();
	UBYTE isClick = mouseUse(MOUSE_PORT_1, MOUSE_LMB);

	if(isClick) {
		for(UBYTE i = 0; i < uwElementCount; ++i) {
			UWORD uwLeft = 32 + uwButtonSpaceX * (i % uwCols);
			UWORD uwTop = 32 + uwButtonSpaceY * (i / uwCols);
			if(
				uwLeft + 8 <= sPosCross.uwX && sPosCross.uwX <= uwLeft + uwButtonSpaceX - 16 &&
				uwTop + 4 <= sPosCross.uwY && sPosCross.uwY <= uwTop + uwButtonSpaceY - 8
			) {
				s_eEditorCurrentTool = i;
				statePop(g_pGameStateManager);
				return;
			}
		}
	}

	s_pSpriteCrosshair->wX = sPosCross.uwX - 8;
	s_pSpriteCrosshair->wY = sPosCross.uwY - 14;
	spriteProcess(s_pSpriteCrosshair);

	viewProcessManagers(s_pView);
	copProcessBlocks();
	systemIdleBegin();
	vPortWaitForEnd(s_pVpMain);
	systemIdleEnd();
}

static void tilePaletteGsDestroy(void) {
	gameTileRefreshAll();
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

	if(s_isEditorEnabled) {
		if(s_isEditorDrawInteractions) {
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
		}

		if(s_isEditorDrawGrid) {
			blitLine(
				s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
				(ubTileX + 1) * MAP_TILE_SIZE - 1, ubTileY * MAP_TILE_SIZE, 11, 0xAAAA, 0
			);
			blitLine(
				s_pBufferMain->pBack, ubTileX * MAP_TILE_SIZE, ubTileY * MAP_TILE_SIZE,
				ubTileX * MAP_TILE_SIZE, (ubTileY + 1) * MAP_TILE_SIZE - 1, 11, 0xAAAA, 0
			);
		}
	}
}

tUwCoordYX gameGetCrossPosition(void) {
	UWORD uwMouseX = mouseGetX(MOUSE_PORT_1);
	UWORD uwMouseY = mouseGetY(MOUSE_PORT_1);
	tUwCoordYX sPos = {.uwX = uwMouseX + 8, .uwY = uwMouseY + 14};
	return sPos;
}

void gameMarkExitReached(UBYTE ubTileX, UBYTE ubTileY, UBYTE isHub) {
	if(g_sConfig.ubCurrentLevel == MAP_INDEX_HUB) {
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
		g_sConfig.ubCurrentLevel = s_ubHubLevelTens + ubHubLevelOnes - 1;
	}
	gameTransitionToExit(isHub ? EXIT_HUB : EXIT_NEXT);
}

void gameDrawSlipgate(UBYTE ubIndex) {
	UBYTE ubFrame = (g_pSlipgates[ubIndex].eNormal < DIRECTION_LEFT);

	UWORD uwX = g_pSlipgates[ubIndex].sTilePositions[0].ubX * MAP_TILE_SIZE + s_pSlipgateOffsets[g_pSlipgates[ubIndex].eNormal].bX;
	UWORD uwY = g_pSlipgates[ubIndex].sTilePositions[0].ubY * MAP_TILE_SIZE + s_pSlipgateOffsets[g_pSlipgates[ubIndex].eNormal].bY;
	tBitMap *pFrames = ubIndex ? g_pSlipgateFramesB : g_pSlipgateFramesA;
	blitCopyMask(
		pFrames, 0, ubFrame ? SLIPGATE_FRAME_HEIGHT_VERTICAL : 0,
		s_pBufferMain->pBack, uwX, uwY, 16,
		ubFrame ? SLIPGATE_FRAME_HEIGHT_HORIZONTAL : SLIPGATE_FRAME_HEIGHT_VERTICAL,
		g_pSlipgateMasks->Planes[0]
	);
}

void gameUpdateAim(void) {
	UBYTE ubFrame = (g_pSlipgates[SLIPGATE_AIM].eNormal < DIRECTION_LEFT);
	s_sBobAim.sPos.uwX = g_pSlipgates[SLIPGATE_AIM].sTilePositions[0].ubX * MAP_TILE_SIZE + s_pSlipgateOffsets[g_pSlipgates[SLIPGATE_AIM].eNormal].bX;
	s_sBobAim.sPos.uwY = g_pSlipgates[SLIPGATE_AIM].sTilePositions[0].ubY * MAP_TILE_SIZE + s_pSlipgateOffsets[g_pSlipgates[SLIPGATE_AIM].eNormal].bY;
	UBYTE* pOffsFrame = bobCalcFrameAddress(g_pAim, ubFrame ? SLIPGATE_FRAME_HEIGHT_VERTICAL : 0);
	UBYTE* pOffsMask = bobCalcFrameAddress(g_pAimMasks, ubFrame ? SLIPGATE_FRAME_HEIGHT_VERTICAL : 0);
	bobSetFrame(&s_sBobAim, pOffsFrame, pOffsMask);
}

tPlayer *gameGetPlayer(void) {
	return &s_sPlayer;
}

tSimpleBufferManager *gameGetBuffer(void) {
	return s_pBufferMain;
}

UWORD gameGetFrameIndex(void) {
	return s_uwGameFrame;
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

//-------------------------------------------------------------------- GAMESTATE

static tState s_sStateTilePalette = { .cbCreate = tilePaletteGsCreate, .cbLoop = tilePaletteGsLoop, .cbDestroy = tilePaletteGsDestroy };
tState g_sStateGame = { .cbCreate = gameGsCreate, .cbLoop = gameGsLoop, .cbDestroy = gameGsDestroy };
