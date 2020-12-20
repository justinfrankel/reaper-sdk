#include "StdAfx.h"




const int tabsel_123 [2] [3] [16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

const int freqs[9] = { 44100, 48000, 32000,
                        22050, 24000, 16000,
                        11025, 12000,  8000 };


#define HDRCMPMASK 0xfffffd00



int head_check(unsigned int head,int check_layer)
{
  /*
    look for a valid header.  
    if check_layer > 0, then require that
    nLayer = check_layer.  
   */

  /* bits 13-14 = layer 3 */
  int nLayer=4-((head>>17)&3);

  if( (head & 0xffe00000) != 0xffe00000) {
    /* syncword */
	return FALSE;
  }
#if 0
  if(!((head>>17)&3)) {
    /* bits 13-14 = layer 3 */
	return FALSE;
  }
#endif

  if (3 !=  nLayer) 
  {
	#if defined (USE_LAYER_1) || defined (USE_LAYER_2)
	  if (4==nLayer)
		  return FALSE;
	#else
		return FALSE;
    #endif
  }

  if (check_layer>0) {
      if (nLayer != check_layer) return FALSE;
  }

  if( ((head>>12)&0xf) == 0xf) {
    /* bits 16,17,18,19 = 1111  invalid bitrate */
    return FALSE;
  }
  if( ((head>>10)&0x3) == 0x3 ) {
    /* bits 20,21 = 11  invalid sampling freq */
    return FALSE;
  }
  return TRUE;
}


/*
 * the code a header and write the information
 * into the frame structure
 */
int decode_header(struct frame *fr,unsigned int newhead)
{
	if ((newhead & 0xFFE00000) != 0xFFE00000) return 0;//sync


    if( newhead & (1<<20) ) {
      fr->lsf = (newhead & (1<<19)) ? 0x0 : 0x1;
      fr->mpeg25 = 0;
    }
    else {
      fr->lsf = 1;
      fr->mpeg25 = 1;
    }

    
    fr->lay = 4-((newhead>>17)&3);
    if( ((newhead>>10)&0x3) == 0x3) {
//      fprintf(stderr,"Stream error\n");
		return 0;
//      exit(1);
    }
    if(fr->mpeg25) {
      fr->sampling_frequency = 6 + ((newhead>>10)&0x3);
    }
    else
      fr->sampling_frequency = ((newhead>>10)&0x3) + (fr->lsf*3);

    fr->error_protection = ((newhead>>16)&0x1)^0x1;
    fr->bitrate_index = ((newhead>>12)&0xf);
    fr->padding   = ((newhead>>9)&0x1);
    fr->extension = ((newhead>>8)&0x1);
    fr->mode      = ((newhead>>6)&0x3);
    fr->mode_ext  = ((newhead>>4)&0x3);
    fr->copyright = ((newhead>>3)&0x1);
    fr->original  = ((newhead>>2)&0x1);
    fr->emphasis  = newhead & 0x3;

    fr->stereo    = (fr->mode == MPG_MD_MONO) ? 1 : 2;

	if (freqs[fr->sampling_frequency]==0) return 0;
	if (//fr->bitrate_index!=0xF &&
		tabsel_123[fr->lsf][0][fr->bitrate_index]==0) return 0;

    switch(fr->lay)
    {
#ifdef USE_LAYER_1
      case 1:
		fr->framesize  = (int) tabsel_123[fr->lsf][0][fr->bitrate_index] * 12000;
		fr->framesize /= freqs[fr->sampling_frequency];
		fr->framesize  = ((fr->framesize+fr->padding)<<2)-4;
		fr->down_sample=0;
		fr->down_sample_sblimit = SBLIMIT>>(fr->down_sample);
        break;
#endif
#ifdef USE_LAYER_2
      case 2:
		fr->framesize = (int) tabsel_123[fr->lsf][1][fr->bitrate_index] * 144000;
		fr->framesize /= freqs[fr->sampling_frequency];
		fr->framesize += fr->padding - 4;
		fr->down_sample=0;
		fr->down_sample_sblimit = SBLIMIT>>(fr->down_sample);
        break;
#endif
      case 3:

	if (fr->bitrate_index==0)
	  fr->framesize=0;
	else{
          fr->framesize  = (int) tabsel_123[fr->lsf][2][fr->bitrate_index] * 144000;
          fr->framesize /= freqs[fr->sampling_frequency]<<(fr->lsf);
          fr->framesize = fr->framesize + fr->padding - 4;
	}
        break; 
      default:
//        fprintf(stderr,"Sorry, layer %d not supported\n",fr->lay); 
        return (0);
    }
    /*    print_header(fr); */

	if (fr->framesize<=0) return 0;

    return 1;
}

unsigned get_frame_size_dword(unsigned int newhead)
{//this is used really a lot, so lets put a "fast" version
#if 0
	{
		frame fr;
		memset(&fr,0,sizeof(fr));
		if (!decode_header(&fr,newhead)) return 0;
		return fr.framesize+4;
	}
#endif

	

	if ((newhead & 0xFFE00000) != 0xFFE00000) return 0;//sync

    int lsf;
    int mpeg25;
    int lay;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int framesize;




    if( newhead & (1<<20) ) {
      lsf = (newhead & (1<<19)) ? 0x0 : 0x1;
      mpeg25 = 0;
    }
    else {
      lsf = 1;
      mpeg25 = 1;
    }

    
    lay = 4-((newhead>>17)&3);
    if( ((newhead>>10)&0x3) == 0x3) {
//      fprintf(stderr,"Stream error\n");
		return 0;
//      exit(1);
    }
    if(mpeg25) {
      sampling_frequency = 6 + ((newhead>>10)&0x3);
    }
    else
      sampling_frequency = ((newhead>>10)&0x3) + (lsf*3);

    bitrate_index = ((newhead>>12)&0xf);
    padding   = ((newhead>>9)&0x1);

	if (freqs[sampling_frequency]==0) return 0;
	if (//bitrate_index!=0xF &&
		tabsel_123[lsf][0][bitrate_index]==0) return 0;

    switch(lay)
    {
#ifdef USE_LAYER_1
      case 1:
		framesize  = (int) tabsel_123[lsf][0][bitrate_index] * 12000;
		framesize /= freqs[sampling_frequency];
		framesize  = ((framesize+padding)<<2)-4;
        break;
#endif
#ifdef USE_LAYER_2
      case 2:
		framesize = (int) tabsel_123[lsf][1][bitrate_index] * 144000;
		framesize /= freqs[sampling_frequency];
		framesize += padding - 4;
        break;
#endif
      case 3:

	if (bitrate_index==0)
	  framesize=0;
	else{
          framesize  = (int) tabsel_123[lsf][2][bitrate_index] * 144000;
          framesize /= freqs[sampling_frequency]<<(lsf);
          framesize = framesize + padding - 4;
	}
        break; 
      default:
//        fprintf(stderr,"Sorry, layer %d not supported\n",lay); 
        return (0);
    }
    /*    print_header(fr); */

	if (framesize<=0) return 0;

    return framesize + 4;
}

unsigned int mpglib::get1bit(void)
{
	if (mp.bitcache_size)
	{
		return (mp.bitcache>>(--mp.bitcache_size))&1;
	}

	

	unsigned rval = 0;

	if (mp.reservoir_ptr>0)
	{
		unsigned ptr = mp.reservoir_write_ptr /*+ sizeof(mp.reservoir)*/ - mp.reservoir_ptr;

		unsigned bb = mp.reservoir_ptr;

		mp.reservoir_bytes_total = mp.reservoir_ptr;

		if (bb>sizeof(mp.bitcache)) bb = sizeof(mp.bitcache);

		
		mp.reservoir_ptr -= bb;

		mp.bitcache_size = bb<<3;

		unsigned bc = 0;
		for(;bb;bb--)
			bc = (bc<<8) | mp.reservoir[(ptr++)&(sizeof(mp.reservoir)-1)];

		mp.bitcache = bc;

		rval = (bc>>(--mp.bitcache_size))&1;
	}
	else if (mp.data_pointer<mp.data_size)
	{
		mp.bitcache_size = (mp.data_size - mp.data_pointer)<<3;
		if (mp.bitcache_size>sizeof(mp.bitcache)*8) mp.bitcache_size = sizeof(mp.bitcache)*8;
		unsigned bb = mp.bitcache_size>>3;
		const unsigned char * src = mp.data + mp.data_pointer;
		
		unsigned bc = 0;
		for(;bb;bb--)
			bc = (bc<<8) | *(src++);

		mp.bitcache = bc;
		mp.data_pointer += mp.bitcache_size>>3;
		rval = (bc>>(--mp.bitcache_size))&1;
	} 

	return rval;
}

unsigned mpglib::getbits(unsigned num)
{
	if (mp.bitcache_size>=num)
	{
		mp.bitcache_size -= num;
		return (mp.bitcache >> mp.bitcache_size) & ((1<<num)-1);
	}

	unsigned rv = 0;
	for(;num>0;num--)
	{
		rv = (rv<<1) | get1bit();	  
	}
	return rv;
}




int mpglib::get_channels()
{
	return mp.fr.stereo;
}

int mpglib::get_sample_rate()
{
	return freqs[mp.fr.sampling_frequency];
}
int frame::get_sample_rate()
{
	return freqs[sampling_frequency];
}
