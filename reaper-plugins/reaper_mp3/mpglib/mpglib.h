
#ifndef _MPGLIB_H_
#define _MPGLIB_H_

#if defined(DEBUG) || defined(_DEBUG)
#include <assert.h>
#endif

#ifndef USE_LAYER_2
#define USE_LAYER_2
#endif


#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2    1.41421356237309504880
#endif

#ifndef FALSE
#define         FALSE                   0
#endif
#ifndef TRUE
#define         TRUE                    1
#endif


#define mpglib_real_size 64




#if mpglib_real_size == 32
#define real float
#elif mpglib_real_size == 64
#define real double
#else
#error invalid mpglib_real_size
#endif

#define sample real

#if !defined(_DEBUG) && mpglib_real_size == 32
#define MPGLIB_HAVE_ASM
#endif

enum
{
	SBLIMIT=32,
	SSLIMIT=18,
	MPG_MD_STEREO=0,
	MPG_MD_JOINT_STEREO=1,
	MPG_MD_DUAL_CHANNEL=2,
	MPG_MD_MONO=3,
	MAXFRAMESIZE=1792,
	SCALE_BLOCK=12,
	//AUSHIFT=3,
};

struct frame {
    int stereo;//number of channels
	int jsbound;
    int lsf;
    int mpeg25;
    int lay;
    int error_protection;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int extension;
    int mode;
    int mode_ext;
    int copyright;
    int original;
    int emphasis;
    int framesize; /* computed framesize, WITHOUT first header dword */

	/* AF: ADDED FOR LAYER1/LAYER2 */
#if defined(USE_LAYER_2) || defined(USE_LAYER_1)
    int II_sblimit;
    struct al_table2 *alloc;
	int down_sample_sblimit;
	int	down_sample;

#endif

	int get_sample_rate();
	inline int get_channels() {return stereo;}
	unsigned get_sample_count();
	unsigned get_bitrate();

};

struct gr_info_s {
      int scfsi;
      unsigned part2_3_length;
      unsigned big_values;
      unsigned scalefac_compress;
	  unsigned window_switching_flag;
      unsigned block_type;
      unsigned mixed_block_flag;
      unsigned table_select[3];
      unsigned maxband[3];
      unsigned maxbandl;
      unsigned maxb;
      unsigned region1start;
      unsigned region2start;
	  int region0_count,region1_count;
      unsigned preflag;
      unsigned scalefac_scale;
      unsigned count1table_select;
      signed int subblock_gain[3];
      signed int global_gain;


};

struct III_sideinfo
{
  unsigned main_data_begin;
  unsigned private_bits;
  struct {
    struct gr_info_s ch[2];
  } gr[2];

  unsigned char use_ext_sfb;
  unsigned char use_subst_big_values;
  unsigned int ext_main_data_begin;

};




typedef struct mpstr_tag {
	enum {RESERVOIR_SIZE = 0x8000, RESERVOIR_SIZE_MASK = RESERVOIR_SIZE-1};
	unsigned bitcache,bitcache_size;

	const unsigned char * data;
	int data_pointer;//in bytes
	int data_size;
	int data_bytes() {
#if defined(DEBUG) || defined(_DEBUG)
		assert(reservoir_ptr<=0);//only when not reading from reservoir
#endif
		return (data_size-data_pointer+(bitcache_size>>3));
	}

	int framesize;
	int ssize;
	int dsize;
	struct frame fr;
	real hybrid_block[2][2][SBLIMIT*SSLIMIT];
	int hybrid_blc[2];
	unsigned int header;
	real synth_buffs[2][2][0x110];
    int  synth_bo;

	unsigned char reservoir[RESERVOIR_SIZE];//MAXFRAMESIZE*10];
	//according to some mp3tech.org docs, reservoir may need data from up to 9 last frames
	//used as circular buffer
	int reservoir_ptr,reservoir_write_ptr,reservoir_bytes_total;
	//reservoir_ptr in bytes, reservoir_write_ptr in bytes
	int layer;

} MPSTR, *PMPSTR;

#ifdef _WIN32
#ifndef BOOL
typedef int BOOL;
#endif
#endif

#define MP3_ERR -1
#define MP3_OK  0
#define MP3_NEED_MORE 1


class mpglib
{
public:
	MPSTR mp;

	III_sideinfo sideinfo;

	void init();
	void deinit();
	unsigned int getbits(unsigned number_of_bits);
	int set_pointer( PMPSTR mp, int backstep);
	unsigned int get1bit(void);
	void III_get_side_info_1(struct III_sideinfo *si,int stereo,int ms_stereo,int sfreq);
	void III_get_side_info_2(struct III_sideinfo *si,int stereo,int ms_stereo,int sfreq, int mpeg25);
	int do_layer3_sideinfo(struct frame *fr);
	int III_dequantize_sample(real xr[SBLIMIT][SSLIMIT],const int *scf,struct gr_info_s *gr_infos,int sfreq,int part2bits);
	int do_layer3(sample *pcm_sample,int *pcm_point);//return 1 on error
	int II_step_one(unsigned int *bit_alloc,int *scale,struct frame *fr);
	int II_step_two(unsigned int *bit_alloc,real fraction[2][4][SBLIMIT],int *scale,struct frame *fr,int x1);
	int do_layer2(sample *pcm_sample,int *pcm_point);
	int III_get_scale_factors_1(int scf[39],struct gr_info_s *gr_infos);
	int III_get_scale_factors_2(int scf[39],struct gr_info_s *gr_infos,int i_stereo,int ms_stereo);
	
	int decode(unsigned int header, struct frame *fr, const unsigned char *in, int isize, sample *out, int osize, int *done);
	//data passed to decode must contain complete mpeg frame, calling code must take care of sync

	void reset() {deinit();init();}

	mpglib() {init();}
	~mpglib() {deinit();}

	int get_channels();
	int get_sample_rate();
};

int  decode_header(struct frame *fr,unsigned int newhead);//newhead = needs correct byte order

#endif /* _MPGLIB_H_ */
