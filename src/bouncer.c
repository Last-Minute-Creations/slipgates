/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bouncer.h"

#define BOUNCER_LIFE_COOLDOWN 500
#define BOUNCER_SPAWN_COOLDOWN 75
#define BOUNCER_VELOCITY 2

typedef enum tBouncerState {
	BOUNCER_STATE_INVALID,
	BOUNCER_STATE_WAITING_FOR_SPAWN,
	BOUNCER_STATE_MOVING,
	BOUNCER_STATE_RECEIVER_REACHED,
} tBouncerState;

static tBodyBox s_sBodyBouncer;
static UBYTE s_hasBouncerNewVelocity;
static fix16_t s_fNewBouncerVelocityX;
static fix16_t s_fNewBouncerVelocityY;
static tBouncerState s_eBouncerState;
static UWORD s_uwBouncerCooldown;
static fix16_t s_fSpawnVelocityX;
static fix16_t s_fSpawnVelocityY;
static fix16_t s_fSpawnPositionX;
static fix16_t s_fSpawnPositionY;

//------------------------------------------------------------------ PRIVATE FNS

static void onBouncerCollided(
	tTile eTile, UNUSED_ARG UBYTE ubTileX, UNUSED_ARG UBYTE ubTileY,
	UNUSED_ARG void *pData
) {
	if(eTile == TILE_RECEIVER) {
		s_eBouncerState = BOUNCER_STATE_RECEIVER_REACHED;
	}
	else {
		s_hasBouncerNewVelocity = 1;
		s_fNewBouncerVelocityX = -s_sBodyBouncer.fVelocityX;
		s_fNewBouncerVelocityY = -s_sBodyBouncer.fVelocityY;
	}
}

//------------------------------------------------------------------- PUBLIC FNS

void bouncerInit(UBYTE ubSpawnerTileX, UBYTE ubSpawnerTileY) {
	if(ubSpawnerTileX == BOUNCER_TILE_INVALID || ubSpawnerTileY == BOUNCER_TILE_INVALID) {
		s_eBouncerState = BOUNCER_STATE_INVALID;
		return;
	}

	UWORD uwBouncerSpawnX = ubSpawnerTileX * MAP_TILE_SIZE;
	UWORD uwBouncerSpawnY = ubSpawnerTileY * MAP_TILE_SIZE;

	if(!mapIsSolidForBodiesAt(ubSpawnerTileX - 1, ubSpawnerTileY)) {
		uwBouncerSpawnX -= MAP_TILE_SIZE;
		s_fSpawnVelocityX = fix16_from_int(-BOUNCER_VELOCITY);
		s_fSpawnVelocityY = 0;
	}
	else if(!mapIsSolidForBodiesAt(ubSpawnerTileX + 1, ubSpawnerTileY)) {
		uwBouncerSpawnX += MAP_TILE_SIZE;
		s_fSpawnVelocityX = fix16_from_int(BOUNCER_VELOCITY);
		s_fSpawnVelocityY = 0;
	}
	else if(!mapIsSolidForBodiesAt(ubSpawnerTileX, ubSpawnerTileY - 1)) {
		uwBouncerSpawnY -= MAP_TILE_SIZE;
		s_fSpawnVelocityX = 0;
		s_fSpawnVelocityY = fix16_from_int(-BOUNCER_VELOCITY);
	}
	else if(!mapIsSolidForBodiesAt(ubSpawnerTileX, ubSpawnerTileY + 1)) {
		uwBouncerSpawnY += MAP_TILE_SIZE;
		s_fSpawnVelocityX = 0;
		s_fSpawnVelocityY = fix16_from_int(BOUNCER_VELOCITY);
	}
	s_fSpawnPositionX = fix16_from_int(uwBouncerSpawnX);
	s_fSpawnPositionY = fix16_from_int(uwBouncerSpawnY);

	bodyInit(&s_sBodyBouncer, s_fSpawnPositionX, s_fSpawnPositionY, 8, 8);
	s_sBodyBouncer.onCollided = onBouncerCollided;
	s_sBodyBouncer.fAccelerationY = 0;
	s_hasBouncerNewVelocity = 0;
	s_eBouncerState = BOUNCER_STATE_WAITING_FOR_SPAWN;
	s_uwBouncerCooldown = 1;
}

UBYTE bouncerProcess(void) {
	switch(s_eBouncerState) {
		case BOUNCER_STATE_INVALID:
			break;
		case BOUNCER_STATE_WAITING_FOR_SPAWN:
			if(--s_uwBouncerCooldown == 0) {
				s_sBodyBouncer.fPosX = s_fSpawnPositionX;
				s_sBodyBouncer.fPosY = s_fSpawnPositionY;
				s_sBodyBouncer.fVelocityX = s_fSpawnVelocityX;
				s_sBodyBouncer.fVelocityY = s_fSpawnVelocityY;
				s_uwBouncerCooldown = BOUNCER_LIFE_COOLDOWN;
				s_eBouncerState = BOUNCER_STATE_MOVING;
			}
			break;
		case BOUNCER_STATE_MOVING:
			if(--s_uwBouncerCooldown == 0) {
				s_uwBouncerCooldown = BOUNCER_SPAWN_COOLDOWN;
				s_eBouncerState = BOUNCER_STATE_WAITING_FOR_SPAWN;
			}
			else {
				bodySimulate(&s_sBodyBouncer);
				if(s_hasBouncerNewVelocity) {
					s_sBodyBouncer.fVelocityX = s_fNewBouncerVelocityX;
					s_sBodyBouncer.fVelocityY = s_fNewBouncerVelocityY;
					s_hasBouncerNewVelocity = 0;
				}
				return 1;
			}
			break;
		case BOUNCER_STATE_RECEIVER_REACHED:
			mapPressButtonIndex(3);
			break;
	}
	return 0;
}

tBodyBox *bouncerGetBody() {
	return &s_sBodyBouncer;
}
