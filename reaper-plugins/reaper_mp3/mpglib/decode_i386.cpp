/*
 * Mpeg Layer-1,2,3 audio decoder
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 *
 * slighlty optimized for machines without autoincrement/decrement.
 * The performance is highly compiler dependend. Maybe
 * the decode.c version for 'normal' processor may be faster
 * even for Intel processors.
 */

/* $Id: decode_i386.c,v 1.1.1.1 2002/04/04 15:14:39 myers_carpenter Exp $ */


#include "StdAfx.h"

int synth_1to1_mono(PMPSTR mp, real *bandPtr,sample *samples,int *pnt)
{
  sample samples_tmp[64];
  sample *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_1to1(mp,bandPtr,0,samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<32;i++) {
    *( samples) = *tmp1;
    samples ++;
    tmp1 += 2;
  }
  *pnt += 32;

  return ret;
}

static void 
#ifdef _WIN32
  __cdecl 
#endif
  synth_internal_c(sample * samples,real * b0,real * window,int bo1)
{
	enum {step=2};
    int j;

    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=step)
    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b0[0x1];
      sum += window[0x2] * b0[0x2];
      sum -= window[0x3] * b0[0x3];
      sum += window[0x4] * b0[0x4];
      sum -= window[0x5] * b0[0x5];
      sum += window[0x6] * b0[0x6];
      sum -= window[0x7] * b0[0x7];
      sum += window[0x8] * b0[0x8];
      sum -= window[0x9] * b0[0x9];
      sum += window[0xA] * b0[0xA];
      sum -= window[0xB] * b0[0xB];
      sum += window[0xC] * b0[0xC];
      sum -= window[0xD] * b0[0xD];
      sum += window[0xE] * b0[0xE];
      sum -= window[0xF] * b0[0xF];

      *(samples) = (sample)(sum / (real)0x8000);
    }

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];

      *(samples) = (sample)(sum / (real)0x8000);

      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=step)
    {
      real sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b0[0x1];
      sum -= window[-0x3] * b0[0x2];
      sum -= window[-0x4] * b0[0x3];
      sum -= window[-0x5] * b0[0x4];
      sum -= window[-0x6] * b0[0x5];
      sum -= window[-0x7] * b0[0x6];
      sum -= window[-0x8] * b0[0x7];
      sum -= window[-0x9] * b0[0x8];
      sum -= window[-0xA] * b0[0x9];
      sum -= window[-0xB] * b0[0xA];
      sum -= window[-0xC] * b0[0xB];
      sum -= window[-0xD] * b0[0xC];
      sum -= window[-0xE] * b0[0xD];
      sum -= window[-0xF] * b0[0xE];
      sum -= window[-0x0] * b0[0xF];

      *(samples) = (sample)(sum / (real)0x8000);
    }
}

#ifdef MPGLIB_HAVE_ASM
extern "C" 
{
	float synth_sampleconv_scale[2] = {(float)(1.0 / (double)0x8000),0};
	float synth_sampleconv_scale_neg[2] = {(float)(- 1.0 / (double)0x8000),0};

	int __cdecl detect_3dnow_ex();

	void __cdecl synth_internal_3dnow(sample * samples,real * b0,real * window,int bo1);

	static void (__cdecl * synth_internal)(sample * samples,real * b0,real * window,int bo1) =
	detect_3dnow_ex() ? synth_internal_3dnow : 
	synth_internal_c;

}
#else
#define synth_internal synth_internal_c
#endif


int synth_1to1(PMPSTR mp, real *bandPtr,int channel,sample *out,int *pnt)
{
  int bo;
  sample *samples = out + *pnt;

  real *b0,(*buf)[0x110];
  int bo1;

  bo = mp->synth_bo;

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = mp->synth_buffs[0];
  }
  else {
    samples++;
    buf = mp->synth_buffs[1];
  }

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
  }

  mp->synth_bo = bo;

	synth_internal(samples,b0,decwin + 16 - bo1,bo1);

  *pnt += 64;

  return 0;
}

