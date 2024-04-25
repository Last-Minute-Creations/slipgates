/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vfx.h"
#include <ace/managers/bob.h>
#include "assets.h"
#include "anim_frame_def.h"

#define VFX_SLIP_COOLDOWN 2
#define VFX_SLIP_FRAME_COUNT 4
#define VFX_SLIP_FRAME_SIZE 16

static tBob s_pVfxSlipBobs[2];
static UBYTE s_ubVfxSlipFrame;
static UBYTE s_ubVfxSlipCooldown;
static UBYTE s_ubVfxSlipIndexSrc;
static tAnimFrameDef s_pVfxSlipOffsets[2][VFX_SLIP_FRAME_COUNT];

void vfxInit(void) {
	for(UBYTE i = 0; i < 2; ++i) {
		for(UBYTE f = 0; f < 4; ++f) {
			s_pVfxSlipOffsets[i][f].pFrame = bobCalcFrameAddress(
				g_pSlipVfx,
				VFX_SLIP_FRAME_SIZE * VFX_SLIP_FRAME_COUNT * i + VFX_SLIP_FRAME_SIZE * f
			);
			s_pVfxSlipOffsets[i][f].pMask = bobCalcFrameAddress(
				g_pSlipVfxMasks,
				VFX_SLIP_FRAME_SIZE * VFX_SLIP_FRAME_COUNT * i + VFX_SLIP_FRAME_SIZE * f
			);
		}
	}

	bobInit(&s_pVfxSlipBobs[0], VFX_SLIP_FRAME_SIZE, VFX_SLIP_FRAME_SIZE, 1, 0, 0, 0, 0);
	bobInit(&s_pVfxSlipBobs[1], VFX_SLIP_FRAME_SIZE, VFX_SLIP_FRAME_SIZE, 1, 0, 0, 0, 0);
	s_ubVfxSlipFrame = VFX_SLIP_FRAME_COUNT;
}

void vfxProcess(void) {
	if(s_ubVfxSlipFrame < VFX_SLIP_FRAME_COUNT) {
		if(!--s_ubVfxSlipCooldown) {
			if(++s_ubVfxSlipFrame >= VFX_SLIP_FRAME_COUNT) {
				return;
			}
			s_ubVfxSlipCooldown = VFX_SLIP_COOLDOWN;
			bobSetFrame(
				&s_pVfxSlipBobs[s_ubVfxSlipIndexSrc],
				s_pVfxSlipOffsets[s_ubVfxSlipIndexSrc][s_ubVfxSlipFrame].pFrame,
				s_pVfxSlipOffsets[s_ubVfxSlipIndexSrc][s_ubVfxSlipFrame].pMask
			);
			bobSetFrame(
				&s_pVfxSlipBobs[!s_ubVfxSlipIndexSrc],
				s_pVfxSlipOffsets[!s_ubVfxSlipIndexSrc][s_ubVfxSlipFrame].pFrame,
				s_pVfxSlipOffsets[!s_ubVfxSlipIndexSrc][s_ubVfxSlipFrame].pMask
			);
		}
		bobPush(&s_pVfxSlipBobs[0]);
		bobPush(&s_pVfxSlipBobs[1]);
	}
}

void vfxStartSlipgate(
	UBYTE ubIndexSrc, UWORD uwStartX, UWORD uwStartY, UWORD uwEndX, UWORD uwEndY
) {
	s_ubVfxSlipIndexSrc = ubIndexSrc;
	s_pVfxSlipBobs[ubIndexSrc].sPos.uwX = uwStartX;
	s_pVfxSlipBobs[ubIndexSrc].sPos.uwY = uwStartY;
	bobSetFrame(
		&s_pVfxSlipBobs[ubIndexSrc],
		s_pVfxSlipOffsets[ubIndexSrc][0].pFrame,
		s_pVfxSlipOffsets[ubIndexSrc][0].pMask
	);
	s_pVfxSlipBobs[!ubIndexSrc].sPos.uwX = uwEndX;
	s_pVfxSlipBobs[!ubIndexSrc].sPos.uwY = uwEndY;
	bobSetFrame(
		&s_pVfxSlipBobs[!ubIndexSrc],
		s_pVfxSlipOffsets[!ubIndexSrc][0].pFrame,
		s_pVfxSlipOffsets[!ubIndexSrc][0].pMask
	);
	s_ubVfxSlipFrame = 0;
	s_ubVfxSlipCooldown = VFX_SLIP_COOLDOWN;
}
