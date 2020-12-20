#ifndef DECODE_I386_H_INCLUDED
#define DECODE_I386_H_INCLUDED

#include "common.h"

int synth_1to1_mono(PMPSTR mp, real *bandPtr,sample *samples,int *pnt);
int synth_1to1(PMPSTR mp, real *bandPtr,int channel,sample *out,int *pnt);

#endif
