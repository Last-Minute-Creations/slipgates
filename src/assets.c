/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assets.h"
#include <ace/managers/system.h>

#define COLOR_WHITE 15

void assetsGlobalCreate(void) {
	systemUse();
	g_pFont = fontCreate("data/uni54.fnt");
	g_pBmCursor = bitmapCreateFromFile("data/cursor.bm", 0);
	systemUnuse();
}

void assetsGlobalDestroy(void) {
	systemUse();
	bitmapDestroy(g_pBmCursor);
	fontDestroy(g_pFont);
	systemUnuse();
}

void assetsGameCreate(void) {
	systemUse();
	g_pPlayerFrames = bitmapCreateFromFile("data/player.bm", 0);
	g_pPlayerMasks = bitmapCreateFromFile("data/player_mask.bm", 0);
	g_pArmLeftFrames = bitmapCreateFromFile("data/arm_left.bm", 0);
	g_pArmLeftMasks = bitmapCreateFromFile("data/arm_left_mask.bm", 0);
	g_pArmRightFrames = bitmapCreateFromFile("data/arm_right.bm", 0);
	g_pArmRightMasks = bitmapCreateFromFile("data/arm_right_mask.bm", 0);
	g_pBoxFrames = bitmapCreateFromFile("data/box.bm", 0);
	g_pBoxMasks = bitmapCreateFromFile("data/box_mask.bm", 0);
	g_pBouncerFrames = bitmapCreateFromFile("data/bouncer.bm", 0);
	g_pBouncerMasks = bitmapCreateFromFile("data/bouncer_mask.bm", 0);
	g_pSlipgateFramesA = bitmapCreateFromFile("data/slipgate_a.bm", 0);
	g_pSlipgateFramesB = bitmapCreateFromFile("data/slipgate_b.bm", 0);
	g_pSlipgateMasks = bitmapCreateFromFile("data/slipgates_mask.bm", 0);
	g_pAim = bitmapCreateFromFile("data/aim.bm", 0);
	g_pAimMasks = bitmapCreateFromFile("data/aim_mask.bm", 0);
	g_pBmTiles = bitmapCreateFromFile("data/tiles.bm", 0);

	g_pPlayerWhiteFrame = bitmapCreate(16, 16, 5, BMF_INTERLEAVED);
	blitRect(g_pPlayerWhiteFrame, 0, 0, 16, 16, COLOR_WHITE);

	systemUnuse();
}

void assetsGameDestroy(void) {
	systemUse();
	bitmapDestroy(g_pPlayerFrames);
	bitmapDestroy(g_pPlayerMasks);
	bitmapDestroy(g_pArmLeftFrames);
	bitmapDestroy(g_pArmLeftMasks);
	bitmapDestroy(g_pArmRightFrames);
	bitmapDestroy(g_pArmRightMasks);
	bitmapDestroy(g_pBoxFrames);
	bitmapDestroy(g_pBoxMasks);
	bitmapDestroy(g_pBouncerFrames);
	bitmapDestroy(g_pBouncerMasks);
	bitmapDestroy(g_pSlipgateFramesA);
	bitmapDestroy(g_pSlipgateFramesB);
	bitmapDestroy(g_pSlipgateMasks);
	bitmapDestroy(g_pAim);
	bitmapDestroy(g_pAimMasks);
	bitmapDestroy(g_pBmTiles);
	bitmapDestroy(g_pPlayerWhiteFrame);
	systemUnuse();
}

//------------------------------------------------------------------ GLOBAL VARS

tBitMap *g_pPlayerFrames;
tBitMap *g_pPlayerMasks;
tBitMap *g_pArmLeftFrames;
tBitMap *g_pArmLeftMasks;
tBitMap *g_pArmRightFrames;
tBitMap *g_pArmRightMasks;
tBitMap *g_pBoxFrames;
tBitMap *g_pBoxMasks;
tBitMap *g_pBouncerFrames;
tBitMap *g_pBouncerMasks;
tBitMap *g_pSlipgateFramesA;
tBitMap *g_pSlipgateFramesB;
tBitMap *g_pSlipgateMasks;
tBitMap *g_pAim;
tBitMap *g_pAimMasks;
tBitMap *g_pBmCursor;
tBitMap *g_pBmTiles;
tBitMap *g_pPlayerWhiteFrame;
tFont *g_pFont;
