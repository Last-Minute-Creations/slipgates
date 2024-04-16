/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "twister.h"
#include <ace/generic/screen.h>
#include <ace/managers/rand.h>
#include <ace/managers/blit.h>
#include <ace/utils/chunky.h>

#define TWISTER_BITMAP_WIDTH (320)
#define CLIP_MARGIN_X 32
#define CLIP_MARGIN_Y 16
#define TWISTER_CENTER_X (160)
#define TWISTER_CENTER_Y (100)
#define TWISTER_CENTER_RADIUS 2
#define TWISTER_BLOCK_SIZE 32
#define TWISTER_MIN_BLOCK_X (-(((TWISTER_CENTER_X + TWISTER_BLOCK_SIZE - 1) / TWISTER_BLOCK_SIZE) + 2))
#define TWISTER_MAX_BLOCK_X (+((((320 - TWISTER_CENTER_X) + TWISTER_BLOCK_SIZE - 1) / TWISTER_BLOCK_SIZE) + 0))
#define TWISTER_MIN_BLOCK_Y (-(((TWISTER_CENTER_Y + TWISTER_BLOCK_SIZE - 1) / TWISTER_BLOCK_SIZE) + 2))
#define TWISTER_MAX_BLOCK_Y (+((((256 - TWISTER_CENTER_Y) + TWISTER_BLOCK_SIZE - 1) / TWISTER_BLOCK_SIZE) + 0))
#define TWISTER_BLOCKS_X (TWISTER_MAX_BLOCK_X - TWISTER_MIN_BLOCK_X)
#define TWISTER_BLOCKS_Y (TWISTER_MAX_BLOCK_Y - TWISTER_MIN_BLOCK_Y)

static tRandManager s_sRand;
static UBYTE s_ps;
static tBitMap s_sFront;
static tBitMap s_sBack;

static void twisterBlitCopy(
	const tBitMap *pSrc, WORD wSrcX, WORD wSrcY,
	tBitMap *pDst, WORD wDstX, WORD wDstY, WORD wWidth, WORD wHeight,
	UBYTE ubMinterm
) {
	// Helper vars
	UWORD uwBlitWords, uwBlitWidth;
	ULONG ulSrcOffs, ulDstOffs;
	UBYTE ubShift, ubMaskFShift, ubMaskLShift;
	// Blitter register values
	UWORD uwBltCon0, uwBltCon1, uwFirstMask, uwLastMask;
	WORD wSrcModulo, wDstModulo;

	UBYTE ubSrcDelta = wSrcX & 0xF;
	UBYTE ubDstDelta = wDstX & 0xF;
	UBYTE ubWidthDelta = (ubSrcDelta + wWidth) & 0xF;
	UWORD uwBufferByteWidth = TWISTER_BITMAP_WIDTH / 8 * 2; // skip 2 bitplanes with one line
	UWORD uwBufferBytesPerRow = TWISTER_BITMAP_WIDTH / 8 * 4;

	if(ubSrcDelta > ubDstDelta || ((wWidth+ubDstDelta+15) & 0xFFF0)-(wWidth+ubSrcDelta) > 16) {
		uwBlitWidth = (wWidth+(ubSrcDelta>ubDstDelta?ubSrcDelta:ubDstDelta)+15) & 0xFFF0;
		uwBlitWords = uwBlitWidth >> 4;

		ubMaskFShift = ((ubWidthDelta+15)&0xF0)-ubWidthDelta;
		ubMaskLShift = uwBlitWidth - (wWidth+ubMaskFShift);
		uwFirstMask = 0xFFFF << ubMaskFShift;
		uwLastMask = 0xFFFF >> ubMaskLShift;
		if(ubMaskLShift > 16) { // Fix for 2-word blits
			uwFirstMask &= 0xFFFF >> (ubMaskLShift-16);
		}

		ubShift = uwBlitWidth - (ubDstDelta+wWidth+ubMaskFShift);
		uwBltCon1 = (ubShift << BSHIFTSHIFT) | BLITREVERSE;

		// Position on the end of last row of the bitmap.
		// For interleaved, position on the last row of last bitplane.
		// TODO: fix duplicating bitmapIsInterleaved() check inside bitmapGetByteWidth()
		ulSrcOffs = uwBufferBytesPerRow * (wSrcY + wHeight) - uwBufferByteWidth + ((wSrcX + wWidth + ubMaskFShift - 1) / 16) * 2;
		ulDstOffs = uwBufferBytesPerRow * (wDstY + wHeight) - uwBufferByteWidth + ((wDstX + wWidth + ubMaskFShift - 1) / 16) * 2;
	}
	else {
		uwBlitWidth = (wWidth+ubDstDelta+15) & 0xFFF0;
		uwBlitWords = uwBlitWidth >> 4;

		ubMaskFShift = ubSrcDelta;
		ubMaskLShift = uwBlitWidth-(wWidth+ubSrcDelta);

		uwFirstMask = 0xFFFF >> ubMaskFShift;
		uwLastMask = 0xFFFF << ubMaskLShift;

		ubShift = ubDstDelta-ubSrcDelta;
		uwBltCon1 = ubShift << BSHIFTSHIFT;

		ulSrcOffs = uwBufferBytesPerRow * wSrcY + (wSrcX >> 3);
		ulDstOffs = uwBufferBytesPerRow * wDstY + (wDstX >> 3);
	}

	uwBltCon0 = (ubShift << ASHIFTSHIFT) | USEB|USEC|USED | ubMinterm;

	wHeight *= 2;
	wSrcModulo = uwBufferByteWidth - uwBlitWords * 2;
	wDstModulo = uwBufferByteWidth - uwBlitWords * 2;

	blitWait(); // Don't modify registers when other blit is in progress
	g_pCustom->bltcon0 = uwBltCon0;
	g_pCustom->bltcon1 = uwBltCon1;
	g_pCustom->bltafwm = uwFirstMask;
	g_pCustom->bltalwm = uwLastMask;
	g_pCustom->bltbmod = wSrcModulo;
	g_pCustom->bltcmod = wDstModulo;
	g_pCustom->bltdmod = wDstModulo;
	g_pCustom->bltadat = 0xFFFF;
	g_pCustom->bltbpt = &pSrc->Planes[0][ulSrcOffs];
	g_pCustom->bltcpt = &pDst->Planes[0][ulDstOffs];
	g_pCustom->bltdpt = &pDst->Planes[0][ulDstOffs];
	g_pCustom->bltsize = (wHeight << HSIZEBITS) | uwBlitWords;
}

void twisterChunkyToPlanar(UBYTE ubColor, UWORD uwX, UWORD uwY, tBitMap *pOut) {
	ULONG ulOffset = uwY * ((TWISTER_BITMAP_WIDTH / 8 * 4) / 2) + (uwX / 16);
	UWORD uwMask = BV(15) >> (uwX & 0xF);
	for(UBYTE ubPlane = 0; ubPlane < 2; ++ubPlane) {
		UWORD *pPlane = (UWORD*)pOut->Planes[ubPlane];
		if(ubColor & 1) {
			pPlane[ulOffset] |= uwMask;
		}
		else {
			pPlane[ulOffset] &= ~uwMask;
		}
		ubColor >>= 1;
	}
}

void twisterInit(UWORD *pPalette) {
	// Init stuff
	s_ps = 0;
	pPalette[0b0001] = 0x303; // 0x707;
	pPalette[0b0100] = 0x634; // 0xD69;
	pPalette[0b0101] = 0x501; // 0xB03;

	s_sFront.BytesPerRow = TWISTER_BITMAP_WIDTH / 8 * 4;
	s_sFront.Rows = 256 + 32;
	s_sFront.Depth = 2;
	s_sFront.Flags = BMF_INTERLEAVED;

	s_sBack.BytesPerRow = TWISTER_BITMAP_WIDTH / 8 * 4;
	s_sBack.Rows = 256 + 32;
	s_sBack.Depth = 2;
	s_sBack.Flags = BMF_INTERLEAVED;

	randInit(&s_sRand, 1911, 2184);
}

void twisterLoop(tSimpleBufferManager *pBfr) {
	++s_ps;
	s_sFront.Planes[0] = pBfr->pFront->Planes[0];
	s_sFront.Planes[1] = pBfr->pFront->Planes[2];
	s_sBack.Planes[0] = pBfr->pBack->Planes[0];
	s_sBack.Planes[1] = pBfr->pBack->Planes[2];

	UWORD uwShift = 0;
	// for(UBYTE i = 0; i <= 4; ++i) {
		uwShift = (uwShift << 1) | ((s_ps >> 0) & 1);
		uwShift = (uwShift << 1) | ((s_ps >> 1) & 1);
		uwShift = (uwShift << 1) | ((s_ps >> 2) & 1);
		uwShift = (uwShift << 1) | ((s_ps >> 3) & 1);
		uwShift = (uwShift << 1) | ((s_ps >> 4) & 1);
	// }

	for(BYTE y = TWISTER_MIN_BLOCK_Y + 2; y < TWISTER_MAX_BLOCK_Y; ++y) {
		WORD yy = TWISTER_CENTER_Y + y * TWISTER_BLOCK_SIZE + uwShift;
		for(BYTE x = TWISTER_MIN_BLOCK_X + 1; x < TWISTER_MAX_BLOCK_X; ++x) {
			WORD xx = TWISTER_CENTER_X + x * TWISTER_BLOCK_SIZE + uwShift;

			WORD wSrcX = xx - (y + 1) - (x + 1);
			WORD wSrcY = yy - (y + 1) + (x + 1);
			WORD wDstX = xx;
			WORD wDstY = yy;
			WORD wWidth = TWISTER_BLOCK_SIZE;
			WORD wHeight = TWISTER_BLOCK_SIZE;

			if(wDstX < 0) {
				WORD wDelta = -wDstX;
				wSrcX += wDelta;
				wWidth -= wDelta;
				wDstX = 0;
			}
			else if(wDstX + wWidth > SCREEN_PAL_WIDTH) {
				wWidth = SCREEN_PAL_WIDTH - wDstX;
			}
			if(wSrcX < 0) {
				WORD wDelta = -wSrcX;
				wDstX += wDelta;
				wWidth -= wDelta;
				wSrcX = 0;
			}
			else if(wSrcX + wWidth > SCREEN_PAL_WIDTH) {
				wWidth = SCREEN_PAL_WIDTH - wSrcX;
			}

			if(wDstY < 0) {
				WORD wDelta = -wDstY;
				wSrcY += wDelta;
				wHeight -= wDelta;
				wDstY = 0;
			}
			else if(wDstY + wHeight > 0 + SCREEN_PAL_HEIGHT) {
				wHeight = 0 + SCREEN_PAL_HEIGHT - wDstY;
			}
			if(wSrcY < 0) {
				WORD wDelta = -wSrcY;
				wDstY += wDelta;
				wWidth -= wDelta;
				wSrcY = 0;
			}
			else if(wSrcY + wHeight > 0 + SCREEN_PAL_HEIGHT) {
				wHeight = 0 + SCREEN_PAL_HEIGHT - wSrcY;
			}

			if(wWidth <= 0 || wHeight <= 0) {
				continue;
			}

			twisterBlitCopy(
				&s_sFront, wSrcX, wSrcY,
				&s_sBack, wDstX, wDstY, wWidth, wHeight, MINTERM_COOKIE
			);
		}
	}

	for(UWORD y = TWISTER_CENTER_Y - TWISTER_CENTER_RADIUS; y <= TWISTER_CENTER_Y + TWISTER_CENTER_RADIUS; ++y) {
		for(UWORD x = TWISTER_CENTER_X - TWISTER_CENTER_RADIUS; x <= TWISTER_CENTER_X + TWISTER_CENTER_RADIUS; ++x) {
			UBYTE ubColor = randUw(&s_sRand) & 3;
			twisterChunkyToPlanar(ubColor, x, y, &s_sBack);
		}
	}
}
