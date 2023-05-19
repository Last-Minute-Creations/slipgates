#ifndef SLIPGATES_SLIPGATE_H
#define SLIPGATES_SLIPGATE_H

#include <ace/types.h>
#include "direction.h"

typedef struct tSlipgate {
	UWORD uwTileX; // left tile
	UWORD uwTileY; // top tile
	tDirection eNormal; // set to DIRECTION_NONE when is off
} tSlipgate;

#endif // SLIPGATES_SLIPGATE_H
