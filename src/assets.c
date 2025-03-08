/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assets.h"
#include <ace/managers/system.h>

#define COLOR_WHITE 15

void assetsGlobalCreate(void) {
	systemUse();
	g_pFont = fontCreateFromPath("data/uni54.fnt");
	g_pBmCursor = bitmapCreateFromPath("data/cursor.bm", 0);
	g_pMod = ptplayerModCreateFromPath("data/slip2.mod");
	systemUnuse();
}

void assetsGlobalDestroy(void) {
	systemUse();
	ptplayerModDestroy(g_pMod);
	bitmapDestroy(g_pBmCursor);
	fontDestroy(g_pFont);
	systemUnuse();
}

void assetsGameCreate(void) {
	systemUse();
	g_pPlayerFrames = bitmapCreateFromPath("data/player.bm", 0);
	g_pPlayerMasks = bitmapCreateFromPath("data/player_mask.bm", 0);
	g_pArmFrames = bitmapCreateFromPath("data/arm.bm", 0);
	g_pArmMasks = bitmapCreateFromPath("data/arm_mask.bm", 0);
	g_pBoxFrames = bitmapCreateFromPath("data/box.bm", 0);
	g_pBoxMasks = bitmapCreateFromPath("data/box_mask.bm", 0);
	g_pBouncerFrames = bitmapCreateFromPath("data/bouncer.bm", 0);
	g_pBouncerMasks = bitmapCreateFromPath("data/bouncer_mask.bm", 0);
	g_pSlipgateFramesA = bitmapCreateFromPath("data/slipgate_a.bm", 0);
	g_pSlipgateFramesB = bitmapCreateFromPath("data/slipgate_b.bm", 0);
	g_pSlipgateMasks = bitmapCreateFromPath("data/slipgates_mask.bm", 0);
	g_pAim = bitmapCreateFromPath("data/aim.bm", 0);
	g_pAimMasks = bitmapCreateFromPath("data/aim_mask.bm", 0);
	g_pSlipVfx = bitmapCreateFromPath("data/slip_vfx.bm", 0);
	g_pSlipVfxMasks = bitmapCreateFromPath("data/slip_vfx_mask.bm", 0);

	g_pBmTiles = bitmapCreateFromPath("data/tiles.bm", 0);

	g_pPlayerWhiteFrame = bitmapCreate(16, 16, 5, BMF_INTERLEAVED);
	blitRect(g_pPlayerWhiteFrame, 0, 0, 16, 16, COLOR_WHITE);

	systemUnuse();
}

void assetsGameDestroy(void) {
	systemUse();
	bitmapDestroy(g_pPlayerFrames);
	bitmapDestroy(g_pPlayerMasks);
	bitmapDestroy(g_pArmFrames);
	bitmapDestroy(g_pArmMasks);
	bitmapDestroy(g_pBoxFrames);
	bitmapDestroy(g_pBoxMasks);
	bitmapDestroy(g_pBouncerFrames);
	bitmapDestroy(g_pBouncerMasks);
	bitmapDestroy(g_pSlipgateFramesA);
	bitmapDestroy(g_pSlipgateFramesB);
	bitmapDestroy(g_pSlipgateMasks);
	bitmapDestroy(g_pAim);
	bitmapDestroy(g_pAimMasks);
	bitmapDestroy(g_pSlipVfx);
	bitmapDestroy(g_pSlipVfxMasks);
	bitmapDestroy(g_pBmTiles);
	bitmapDestroy(g_pPlayerWhiteFrame);
	systemUnuse();
}

//------------------------------------------------------------------ GLOBAL VARS

tBitMap *g_pPlayerFrames;
tBitMap *g_pPlayerMasks;
tBitMap *g_pArmFrames;
tBitMap *g_pArmMasks;
tBitMap *g_pBoxFrames;
tBitMap *g_pBoxMasks;
tBitMap *g_pBouncerFrames;
tBitMap *g_pBouncerMasks;
tBitMap *g_pSlipgateFramesA;
tBitMap *g_pSlipgateFramesB;
tBitMap *g_pSlipgateMasks;
tBitMap *g_pAim;
tBitMap *g_pAimMasks;
tBitMap *g_pSlipVfx;
tBitMap *g_pSlipVfxMasks;
tBitMap *g_pBmCursor;
tBitMap *g_pBmTiles;
tBitMap *g_pPlayerWhiteFrame;
tFont *g_pFont;
tPtplayerMod *g_pMod;
