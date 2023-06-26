/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_TILE_H
#define SLIPGATES_TILE_H

#include <ace/macros.h>

#define TILE_LAYER_WALLS BV(15)
#define TILE_LAYER_GRATES BV(14)
#define TILE_LAYER_LETHALS BV(13)
#define TILE_LAYER_SLIPGATES BV(12)
#define TILE_LAYER_SLIPGATABLE BV(11)
#define TILE_LAYER_EXIT BV(10)
#define TILE_LAYER_BUTTON BV(9)
#define TILE_LAYER_ACTIVE_TURRET BV(8)
#define MAP_TILE_INDEX_MASK ~( \
	TILE_LAYER_WALLS | TILE_LAYER_GRATES | TILE_LAYER_LETHALS | \
	TILE_LAYER_SLIPGATES | TILE_LAYER_SLIPGATABLE | TILE_LAYER_EXIT | \
	TILE_LAYER_BUTTON | TILE_LAYER_ACTIVE_TURRET \
)

typedef enum tTile {
	TILE_BG                    = 0,
	TILE_SLIPGATE_A            = 1  | TILE_LAYER_SLIPGATES | TILE_LAYER_SLIPGATABLE,
	TILE_SLIPGATE_B            = 2  | TILE_LAYER_SLIPGATES | TILE_LAYER_SLIPGATABLE,
	TILE_WALL_BLOCKED          = 3  | TILE_LAYER_WALLS,
	TILE_WALL                  = 4  | TILE_LAYER_WALLS | TILE_LAYER_SLIPGATABLE,
	TILE_GRATE                 = 5  | TILE_LAYER_GRATES,
	TILE_DEATH_FIELD           = 6  | TILE_LAYER_LETHALS,
	TILE_EXIT                  = 7  | TILE_LAYER_WALLS | TILE_LAYER_EXIT,
	TILE_BUTTON_A              = 8  | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_B              = 9  | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_C              = 10 | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_D              = 11 | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_E              = 12 | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_F              = 13 | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_G              = 14 | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_BUTTON_H              = 15 | TILE_LAYER_WALLS | TILE_LAYER_BUTTON,
	TILE_DOOR_CLOSED           = 16 | TILE_LAYER_WALLS,
	TILE_DOOR_OPEN             = 17,
	TILE_RECEIVER              = 18 | TILE_LAYER_WALLS,
	TILE_BOUNCER_SPAWNER       = 19 | TILE_LAYER_WALLS,
	TILE_WALL_TOGGLABLE_OFF    = 20 | TILE_LAYER_WALLS,
	TILE_WALL_TOGGLABLE_ON     = 21 | TILE_LAYER_WALLS | TILE_LAYER_SLIPGATABLE,
	TILE_SPIKES_OFF_FLOOR      = 22 | TILE_LAYER_WALLS,
	TILE_SPIKES_ON_FLOOR       = 23 | TILE_LAYER_WALLS | TILE_LAYER_LETHALS,
	TILE_SPIKES_OFF_BG         = 24,
	TILE_SPIKES_ON_BG          = 25,
	TILE_TURRET_ACTIVE_LEFT    = 26 | TILE_LAYER_WALLS | TILE_LAYER_ACTIVE_TURRET,
	TILE_TURRET_ACTIVE_RIGHT   = 27 | TILE_LAYER_WALLS | TILE_LAYER_ACTIVE_TURRET,
	TILE_TURRET_INACTIVE_LEFT  = 28 | TILE_LAYER_WALLS,
	TILE_TURRET_INACTIVE_RIGHT = 29 | TILE_LAYER_WALLS,
	TILE_PIPE                  = 30 | TILE_LAYER_GRATES,
	TILE_EXIT_HUB              = 31 | TILE_LAYER_WALLS | TILE_LAYER_EXIT,
} tTile;

#endif // SLIPGATES_TILE_H
