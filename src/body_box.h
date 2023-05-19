#ifndef SLIPGATES_BODY_H
#define SLIPGATES_BODY_H

#include <fixmath/fix16.h>
#include <ace/managers/bob.h>

typedef struct tBodyBox {
	tBob sBob;
	fix16_t fPosX;
	fix16_t fPosY;
	fix16_t fVelocityX;
	fix16_t fVelocityY;
	fix16_t fAccelerationX;
	fix16_t fAccelerationY;
} tBodyBox;

void bodySimulate(tBodyBox *pBody);
void bodyInit(tBodyBox *pBody, fix16_t fPosX, fix16_t fPosY);

#endif // SLIPGATES_BODY_H
