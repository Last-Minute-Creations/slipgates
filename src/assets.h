/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_ASSETS_H
#define SLIPGATES_ASSETS_H

#include <ace/utils/font.h>

void assetsGlobalCreate(void);

void assetsGlobalDestroy(void);

void assetsGameCreate(void);

void assetsGameDestroy(void);

//------------------------------------------------------------------ GLOBAL VARS

extern tBitMap *g_pPlayerFrames;
extern tBitMap *g_pPlayerMasks;
extern tBitMap *g_pArmLeftFrames;
extern tBitMap *g_pArmLeftMasks;
extern tBitMap *g_pArmRightFrames;
extern tBitMap *g_pArmRightMasks;
extern tBitMap *g_pBoxFrames;
extern tBitMap *g_pBoxMasks;
extern tBitMap *g_pBouncerFrames;
extern tBitMap *g_pBouncerMasks;
extern tBitMap *g_pSlipgateFramesA;
extern tBitMap *g_pSlipgateFramesB;
extern tBitMap *g_pSlipgateMasks;
extern tBitMap *g_pAim;
extern tBitMap *g_pAimMasks;
extern tBitMap *g_pBmCursor;
extern tBitMap *g_pBmTiles;
extern tBitMap *g_pPlayerWhiteFrame;
extern tFont *g_pFont;

#endif // SLIPGATES_ASSETS_H
