/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assets.h"
#include <ace/managers/system.h>

void assetsGameCreate(void) {
	systemUse();
	g_pPlayerFrames = bitmapCreateFromFile("data/player.bm", 0);
	g_pPlayerMasks = bitmapCreateFromFile("data/player_mask.bm", 0);
	g_pBoxFrames = bitmapCreateFromFile("data/box.bm", 0);
	g_pBoxMasks = bitmapCreateFromFile("data/box_mask.bm", 0);
	g_pBouncerFrames = bitmapCreateFromFile("data/bouncer.bm", 0);
	g_pBouncerMasks = bitmapCreateFromFile("data/bouncer_mask.bm", 0);
	g_pSlipgateFramesA = bitmapCreateFromFile("data/slipgate_a.bm", 0);
	g_pSlipgateFramesB = bitmapCreateFromFile("data/slipgate_b.bm", 0);
	g_pSlipgateMasks = bitmapCreateFromFile("data/slipgates_mask.bm", 0);
	g_pBmCursor = bitmapCreateFromFile("data/cursor.bm", 0);
	g_pBmTiles = bitmapCreateFromFile("data/tiles.bm", 0);

	g_pFont = fontCreate("data/uni54.fnt");
	systemUnuse();
}

void assetsGameDestroy(void) {
	systemUse();
	bitmapDestroy(g_pPlayerFrames);
	bitmapDestroy(g_pPlayerMasks);
	bitmapDestroy(g_pBoxFrames);
	bitmapDestroy(g_pBoxMasks);
	bitmapDestroy(g_pBouncerFrames);
	bitmapDestroy(g_pBouncerMasks);
	bitmapDestroy(g_pSlipgateFramesA);
	bitmapDestroy(g_pSlipgateFramesB);
	bitmapDestroy(g_pSlipgateMasks);
	bitmapDestroy(g_pBmCursor);
	bitmapDestroy(g_pBmTiles);

	fontDestroy(g_pFont);
	systemUnuse();
}

//------------------------------------------------------------------ GLOBAL VARS

tBitMap *g_pPlayerFrames;
tBitMap *g_pPlayerMasks;
tBitMap *g_pBoxFrames;
tBitMap *g_pBoxMasks;
tBitMap *g_pBouncerFrames;
tBitMap *g_pBouncerMasks;
tBitMap *g_pSlipgateFramesA;
tBitMap *g_pSlipgateFramesB;
tBitMap *g_pSlipgateMasks;
tBitMap *g_pBmCursor;
tBitMap *g_pBmTiles;
tFont *g_pFont;
