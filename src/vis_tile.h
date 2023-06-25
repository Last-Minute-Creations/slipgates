/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SLIPGATES_VIS_TILE_H
#define SLIPGATES_VIS_TILE_H

typedef enum tVisTile {
	VIS_TILE_BG_1,
	VIS_TILE_BG_CONVEX_N,
	VIS_TILE_BG_CONVEX_NE,
	VIS_TILE_BG_CONVEX_E,
	VIS_TILE_BG_CONVEX_SE,
	VIS_TILE_BG_CONVEX_S,
	VIS_TILE_BG_CONVEX_SW,
	VIS_TILE_BG_CONVEX_W,
	VIS_TILE_BG_CONVEX_NW,
	VIS_TILE_BG_CONCAVE_NE,
	VIS_TILE_BG_CONCAVE_SE,
	VIS_TILE_BG_CONCAVE_SW,
	VIS_TILE_BG_CONCAVE_NW,
	VIS_TILE_BG_PIPE_N,
	VIS_TILE_BG_PIPE_E,
	VIS_TILE_BG_PIPE_S,
	VIS_TILE_BG_PIPE_W,
	VIS_TILE_WALL_1,
	VIS_TILE_WALL_CONVEX_N,
	VIS_TILE_WALL_CONVEX_NE,
	VIS_TILE_WALL_CONVEX_E,
	VIS_TILE_WALL_CONVEX_SE,
	VIS_TILE_WALL_CONVEX_S,
	VIS_TILE_WALL_CONVEX_SW,
	VIS_TILE_WALL_CONVEX_W,
	VIS_TILE_WALL_CONVEX_NW,
	VIS_TILE_WALL_CONCAVE_N,
	VIS_TILE_WALL_CONCAVE_NE,
	VIS_TILE_WALL_CONCAVE_E,
	VIS_TILE_WALL_CONCAVE_SE,
	VIS_TILE_WALL_CONCAVE_S,
	VIS_TILE_WALL_CONCAVE_SW,
	VIS_TILE_WALL_CONCAVE_W,
	VIS_TILE_WALL_CONCAVE_NW,
	VIS_TILE_WALL_PIPE_N,
	VIS_TILE_WALL_PIPE_E,
	VIS_TILE_WALL_PIPE_S,
	VIS_TILE_WALL_PIPE_W,

	VIS_TILE_BLOCKED_BG_CONVEX_N,
	VIS_TILE_BLOCKED_BG_CONVEX_NE,
	VIS_TILE_BLOCKED_BG_CONVEX_E,
	VIS_TILE_BLOCKED_BG_CONVEX_SE,
	VIS_TILE_BLOCKED_BG_CONVEX_S,
	VIS_TILE_BLOCKED_BG_CONVEX_SW,
	VIS_TILE_BLOCKED_BG_CONVEX_W,
	VIS_TILE_BLOCKED_BG_CONVEX_NW,
	VIS_TILE_BLOCKED_BG_CONCAVE_NE,
	VIS_TILE_BLOCKED_BG_CONCAVE_SE,
	VIS_TILE_BLOCKED_BG_CONCAVE_SW,
	VIS_TILE_BLOCKED_BG_CONCAVE_NW,
	VIS_TILE_BLOCKED_BG_PIPE_N,
	VIS_TILE_BLOCKED_BG_PIPE_E,
	VIS_TILE_BLOCKED_BG_PIPE_S,
	VIS_TILE_BLOCKED_BG_PIPE_W,
	VIS_TILE_BLOCKED_WALL_CONVEX_N,
	VIS_TILE_BLOCKED_WALL_CONVEX_NE,
	VIS_TILE_BLOCKED_WALL_CONVEX_E,
	VIS_TILE_BLOCKED_WALL_CONVEX_SE,
	VIS_TILE_BLOCKED_WALL_CONVEX_S,
	VIS_TILE_BLOCKED_WALL_CONVEX_SW,
	VIS_TILE_BLOCKED_WALL_CONVEX_W,
	VIS_TILE_BLOCKED_WALL_CONVEX_NW,
	VIS_TILE_BLOCKED_WALL_CONCAVE_N,
	VIS_TILE_BLOCKED_WALL_CONCAVE_NE,
	VIS_TILE_BLOCKED_WALL_CONCAVE_E,
	VIS_TILE_BLOCKED_WALL_CONCAVE_SE,
	VIS_TILE_BLOCKED_WALL_CONCAVE_S,
	VIS_TILE_BLOCKED_WALL_CONCAVE_SW,
	VIS_TILE_BLOCKED_WALL_CONCAVE_W,
	VIS_TILE_BLOCKED_WALL_CONCAVE_NW,
	VIS_TILE_BLOCKED_WALL_PIPE_N,
	VIS_TILE_BLOCKED_WALL_PIPE_E,
	VIS_TILE_BLOCKED_WALL_PIPE_S,
	VIS_TILE_BLOCKED_WALL_PIPE_W,

	VIS_TILE_PIPE_NS,
	VIS_TILE_PIPE_EW,
	VIS_TILE_PIPE_NESW,

	VIS_TILE_EXIT_BG_LEFT_TOP,
	VIS_TILE_EXIT_BG_LEFT_MID,
	VIS_TILE_EXIT_BG_LEFT_BOTTOM,
	VIS_TILE_EXIT_WALL_LEFT_TOP,
	VIS_TILE_EXIT_WALL_LEFT_MID,
	VIS_TILE_EXIT_WALL_LEFT_BOTTOM,
	VIS_TILE_EXIT_BG_RIGHT_TOP,
	VIS_TILE_EXIT_BG_RIGHT_MID,
	VIS_TILE_EXIT_BG_RIGHT_BOTTOM,
	VIS_TILE_EXIT_WALL_RIGHT_TOP,
	VIS_TILE_EXIT_WALL_RIGHT_MID,
	VIS_TILE_EXIT_WALL_RIGHT_BOTTOM,
	VIS_TILE_BUTTON_BG_LEFT_1,
	VIS_TILE_BUTTON_BG_LEFT_2,
	VIS_TILE_BUTTON_WALL_LEFT_1,
	VIS_TILE_BUTTON_WALL_LEFT_2,
	VIS_TILE_BUTTON_BG_RIGHT_1,
	VIS_TILE_BUTTON_BG_RIGHT_2,
	VIS_TILE_BUTTON_WALL_RIGHT_1,
	VIS_TILE_BUTTON_WALL_RIGHT_2,

	VIS_TILE_DOOR_LEFT_CLOSED_WALL_TOP,
	VIS_TILE_DOOR_LEFT_CLOSED_WALL_BOTTOM,
	VIS_TILE_DOOR_LEFT_CLOSED_TOP,
	VIS_TILE_DOOR_LEFT_CLOSED_MID,
	VIS_TILE_DOOR_LEFT_CLOSED_BOTTOM,
	VIS_TILE_DOOR_RIGHT_CLOSED_WALL_TOP,
	VIS_TILE_DOOR_RIGHT_CLOSED_WALL_BOTTOM,
	VIS_TILE_DOOR_RIGHT_CLOSED_TOP,
	VIS_TILE_DOOR_RIGHT_CLOSED_MID,
	VIS_TILE_DOOR_RIGHT_CLOSED_BOTTOM,
	VIS_TILE_DOOR_DOWN_CLOSED_WALL_LEFT,
	VIS_TILE_DOOR_DOWN_CLOSED_WALL_RIGHT,
	VIS_TILE_DOOR_DOWN_CLOSED_TOP_LEFT,
	VIS_TILE_DOOR_DOWN_CLOSED_TOP_MID,
	VIS_TILE_DOOR_DOWN_CLOSED_TOP_RIGHT,
	VIS_TILE_DOOR_DOWN_CLOSED_BOTTOM_LEFT,
	VIS_TILE_DOOR_DOWN_CLOSED_BOTTOM_MID,
	VIS_TILE_DOOR_DOWN_CLOSED_BOTTOM_RIGHT,

	VIS_TILE_DOOR_LEFT_OPEN_WALL_TOP,
	VIS_TILE_DOOR_LEFT_OPEN_WALL_BOTTOM,
	VIS_TILE_DOOR_LEFT_OPEN_TOP,
	VIS_TILE_DOOR_LEFT_OPEN_MID,
	VIS_TILE_DOOR_LEFT_OPEN_BOTTOM,
	VIS_TILE_DOOR_RIGHT_OPEN_WALL_TOP,
	VIS_TILE_DOOR_RIGHT_OPEN_WALL_BOTTOM,
	VIS_TILE_DOOR_RIGHT_OPEN_TOP,
	VIS_TILE_DOOR_RIGHT_OPEN_MID,
	VIS_TILE_DOOR_RIGHT_OPEN_BOTTOM,
	VIS_TILE_DOOR_DOWN_OPEN_WALL_LEFT,
	VIS_TILE_DOOR_DOWN_OPEN_WALL_RIGHT,
	VIS_TILE_DOOR_DOWN_OPEN_TOP_LEFT,
	VIS_TILE_DOOR_DOWN_OPEN_TOP_MID,
	VIS_TILE_DOOR_DOWN_OPEN_TOP_RIGHT,
	VIS_TILE_DOOR_DOWN_OPEN_BOTTOM_LEFT,
	VIS_TILE_DOOR_DOWN_OPEN_BOTTOM_MID,
	VIS_TILE_DOOR_DOWN_OPEN_BOTTOM_RIGHT,

	VIS_TILE_GRATE_LEFT_WALL_TOP,
	VIS_TILE_GRATE_LEFT_WALL_BOTTOM,
	VIS_TILE_GRATE_LEFT_TOP,
	VIS_TILE_GRATE_LEFT_MID,
	VIS_TILE_GRATE_LEFT_BOTTOM,
	VIS_TILE_GRATE_RIGHT_WALL_TOP,
	VIS_TILE_GRATE_RIGHT_WALL_BOTTOM,
	VIS_TILE_GRATE_RIGHT_TOP,
	VIS_TILE_GRATE_RIGHT_MID,
	VIS_TILE_GRATE_RIGHT_BOTTOM,
	VIS_TILE_GRATE_DOWN_WALL_LEFT,
	VIS_TILE_GRATE_DOWN_WALL_RIGHT,
	VIS_TILE_GRATE_DOWN_TOP_LEFT,
	VIS_TILE_GRATE_DOWN_TOP_MID,
	VIS_TILE_GRATE_DOWN_TOP_RIGHT,
	VIS_TILE_GRATE_DOWN_BOTTOM_LEFT,
	VIS_TILE_GRATE_DOWN_BOTTOM_MID,
	VIS_TILE_GRATE_DOWN_BOTTOM_RIGHT,

	VIS_TILE_DEATH_FIELD_LEFT_WALL_TOP,
	VIS_TILE_DEATH_FIELD_LEFT_WALL_BOTTOM,
	VIS_TILE_DEATH_FIELD_LEFT_TOP,
	VIS_TILE_DEATH_FIELD_LEFT_MID,
	VIS_TILE_DEATH_FIELD_LEFT_BOTTOM,
	VIS_TILE_DEATH_FIELD_RIGHT_WALL_TOP,
	VIS_TILE_DEATH_FIELD_RIGHT_WALL_BOTTOM,
	VIS_TILE_DEATH_FIELD_RIGHT_TOP,
	VIS_TILE_DEATH_FIELD_RIGHT_MID,
	VIS_TILE_DEATH_FIELD_RIGHT_BOTTOM,
	VIS_TILE_DEATH_FIELD_DOWN_WALL_LEFT,
	VIS_TILE_DEATH_FIELD_DOWN_WALL_RIGHT,
	VIS_TILE_DEATH_FIELD_DOWN_TOP_LEFT,
	VIS_TILE_DEATH_FIELD_DOWN_TOP_MID,
	VIS_TILE_DEATH_FIELD_DOWN_TOP_RIGHT,
	VIS_TILE_DEATH_FIELD_DOWN_BOTTOM_LEFT,
	VIS_TILE_DEATH_FIELD_DOWN_BOTTOM_MID,
	VIS_TILE_DEATH_FIELD_DOWN_BOTTOM_RIGHT,

	VIS_TILE_RECEIVER_BG_UP,
	VIS_TILE_RECEIVER_WALL_UP,
	VIS_TILE_RECEIVER_BG_DOWN,
	VIS_TILE_RECEIVER_WALL_DOWN,
	VIS_TILE_RECEIVER_BG_LEFT,
	VIS_TILE_RECEIVER_WALL_LEFT,
	VIS_TILE_RECEIVER_BG_RIGHT,
	VIS_TILE_RECEIVER_WALL_RIGHT,

	VIS_TILE_BOUNCER_SPAWNER_BG_UP,
	VIS_TILE_BOUNCER_SPAWNER_WALL_UP,
	VIS_TILE_BOUNCER_SPAWNER_BG_DOWN,
	VIS_TILE_BOUNCER_SPAWNER_WALL_DOWN,
	VIS_TILE_BOUNCER_SPAWNER_BG_LEFT,
	VIS_TILE_BOUNCER_SPAWNER_WALL_LEFT,
	VIS_TILE_BOUNCER_SPAWNER_BG_RIGHT,
	VIS_TILE_BOUNCER_SPAWNER_WALL_RIGHT,

	VIS_TILE_SLIPGATABLE_OFF_1,
	VIS_TILE_SLIPGATABLE_ON_1,

	VIS_TILE_SPIKES_OFF_FLOOR_1,
	VIS_TILE_SPIKES_ON_FLOOR_1,
	VIS_TILE_SPIKES_OFF_BG_1,
	VIS_TILE_SPIKES_ON_BG_1,

	VIS_TILE_TURRET_ACTIVE_LEFT,
	VIS_TILE_TURRET_ACTIVE_RIGHT,
	VIS_TILE_TURRET_INACTIVE_LEFT,
	VIS_TILE_TURRET_INACTIVE_RIGHT,
} tVisTile;

#endif // SLIPGATES_VIS_TILE_H
