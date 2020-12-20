/* $Id: interface.c,v 1.1.1.1 2002/04/04 15:14:40 myers_carpenter Exp $ */

// CHANGED: 
// copy_mp(), added mp->tail null pointer check to fix random crash
// sync_buffer(), added mp->tail null pointer check

#include "StdAfx.h"

int mpglib::decode(unsigned int header, struct frame *fr, const unsigned char *in, int isize, sample *out, int osize, int *done)
{
	profiler(mpglib_decode);

	mp.data = in;
	mp.data_size = isize;
	mp.bitcache_size = 0;
	mp.data_pointer = 0;
	mp.reservoir_ptr = 0;

	if (isize>MAXFRAMESIZE-4) isize = MAXFRAMESIZE-4;

	int iret,bits;

  // we support mpeg2 modes, which are 576 samples
	//if(osize < 1152) {
//		return MP3_ERR;
	//}

	/* First decode header */

	mp.header = header;
  mp.fr = *fr;
	mp.framesize = mp.fr.framesize;
	
	if(mp.fr.lsf)
	mp.ssize = (mp.fr.stereo == 1) ? 9 : 17;
	else
	mp.ssize = (mp.fr.stereo == 1) ? 17 : 32;
	if (mp.fr.error_protection) 
	mp.ssize += 2;
	    
	    
		/* Layer 3 only */
	if (mp.fr.lay==3)
	{
        if (mp.data_bytes() < mp.ssize) 
			return MP3_NEED_MORE;


		if(mp.fr.error_protection)
		  getbits(16);
		bits=do_layer3_sideinfo(&mp.fr);
		/* bits = actual number of bits needed to parse this frame */
		/* can be negative, if all bits needed are in the reservoir */
		if (bits<0) bits=0;

		/* read just as many bytes as necessary before decoding */
		mp.dsize = (bits+7)/8;

	}
	else
	{
		/* Layers 1 and 2 */

		/* check if there is enough input data */
		if(mp.fr.framesize > mp.data_bytes())
			return MP3_NEED_MORE;

		mp.dsize=mp.fr.framesize;
		mp.ssize=0;
	}


	/* now decode main data */

	if(mp.dsize > mp.data_bytes())
		return MP3_NEED_MORE;


	*done = 0;

	if (mp.layer == 0)
		mp.layer = mp.fr.lay;
	else if (mp.layer != mp.fr.lay)
		return MP3_ERR;

	switch (mp.fr.lay)
	{
	case 2:
		if(mp.fr.error_protection)
			getbits(16);

		do_layer2(out,done);
	break;

	case 3:
		if (do_layer3(out,done)==MP3_ERR)
			return MP3_ERR;

	break;
	default:
		return MP3_ERR;
		;
	}


	iret = *done>0 ? MP3_OK : MP3_NEED_MORE;

	/* buffer the ancillary data and reservoir for next frame */
	{
		int bytes = mp.framesize-(mp.ssize+mp.dsize);
		const unsigned char * src = in+isize-bytes;

		mp.reservoir_bytes_total += bytes;
		if (mp.reservoir_bytes_total>sizeof(mp.reservoir)) mp.reservoir_bytes_total = sizeof(mp.reservoir);
		
		while(bytes>0)
		{
			int delta = sizeof(mp.reservoir) - mp.reservoir_write_ptr;
			if (delta>bytes) delta=bytes;
			memcpy(mp.reservoir+mp.reservoir_write_ptr,src,delta);
			src+=delta;
			bytes-=delta;
			mp.reservoir_write_ptr = (mp.reservoir_write_ptr+delta) % sizeof(mp.reservoir);
		}
	}

	mp.framesize =0;

	return iret;
}

void mpglib::init()
{
	memset(&mp,0,sizeof(mp));
	mp.framesize = 0;
	mp.ssize = 0;
	mp.dsize = 0;
	mp.synth_bo = 1;
	mp.reservoir_ptr=0;
	mp.reservoir_write_ptr=0;
	mp.reservoir_bytes_total = 0;

	mp.bitcache=mp.bitcache_size=0;

	memset(&sideinfo,0,sizeof(sideinfo));


	{
		static int inited;
		if (!inited)
		{
			inited=1;
			make_decode_tables(32767);

			init_layer3();
			init_layer2();
		}
	}

	mp.layer = 0;
}

void mpglib::deinit()
{

}

static unsigned get_samples_per_frame_internal(unsigned layer,unsigned srate)
{
	switch(layer)
	{
	case 2:
		return 576*2;
	case 3:
		return srate >= 32000 ? 576*2 : 576;
	default:
		return 0;
	}
}

unsigned frame::get_sample_count()
{
	return get_samples_per_frame_internal(lay,get_sample_rate());
}
  


static const int tabsel_123 [2] [3] [16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

unsigned frame::get_bitrate()
{//bits / second == (bytes*8) / time_seconds / 1000 == (bytes*8) / (sample_count() / srate)
	//return (framesize+4)*8 * get_sample_rate() / get_sample_count();

  int layeridx=2;
  if (lay==1) layeridx=0;
  else if (lay == 2) layeridx=1;

  int br=tabsel_123[lsf][layeridx][bitrate_index];

  return br*1000;
}
