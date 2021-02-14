// helper definitions pulled from reaper_plugin.h, lice.h, swell-types.h etc
//
// see those headers for documentation, mainly 'Notes on fx-embed VST implementation' in reaper_plugin.h

#ifndef _REAPER_PLUGIN_H_

typedef struct
{
  double draw_start_time; // pcm-source-inline only: project time at pixel start of draw
  int draw_start_y;       // pcm-source-inline only: if y-scroll is partway into the item, positive pixel value
  double pixels_per_second; // pcm-source-inline only

  int width, height; // width and height of view of the item. if doing a partial update this may be larger than the bitmap passed in
  int mouse_x, mouse_y; // pcm-source-inline: valid only on mouse/key/setcursor/etc messages

  void *extraParms[8];
  // WM_KEYDOWN handlers can use MSG *msg = (MSG *)extraParms[0]
  // WM_SETCURSOR, if pcm-source-inline: handlers should set *extraParms[0] = hcursor
  // WM_PAINT, if fx-embed: extra_flag = (int)(INT_PTR)extraParms[0]
} REAPER_inline_positioninfo;


#define REAPER_INLINE_RETNOTIFY_INVALIDATE 0x1000000 // want refresh of display
#define REAPER_INLINE_RETNOTIFY_SETCAPTURE 0x2000000 // setcapture
#define REAPER_INLINE_RETNOTIFY_SETFOCUS   0x4000000 // set focus to item
#define REAPER_INLINE_RETNOTIFY_NOAUTOSCROLL 0x8000000 // modifier only valid when setcapture set

#endif

#ifndef _LICE_H

typedef unsigned int LICE_pixel;
typedef unsigned char LICE_pixel_chan;

#define LICE_RGBA(r,g,b,a) (((b)&0xff)|(((g)&0xff)<<8)|(((r)&0xff)<<16)|(((a)&0xff)<<24))
#define LICE_GETB(v) ((v)&0xff)
#define LICE_GETG(v) (((v)>>8)&0xff)
#define LICE_GETR(v) (((v)>>16)&0xff)
#define LICE_GETA(v) (((v)>>24)&0xff)

#if defined(__APPLE__) && defined(__ppc__)
#define LICE_PIXEL_A 0
#define LICE_PIXEL_R 1
#define LICE_PIXEL_G 2
#define LICE_PIXEL_B 3
#else
#define LICE_PIXEL_B 0
#define LICE_PIXEL_G 1
#define LICE_PIXEL_R 2
#define LICE_PIXEL_A 3
#endif

class LICE_IBitmap
{
public:
  virtual ~LICE_IBitmap() { }

  virtual LICE_pixel *getBits()=0;
  virtual int getWidth()=0;
  virtual int getHeight()=0;
  virtual int getRowSpan()=0; // includes any off-bitmap data. this is in sizeof(LICE_pixel) units, not bytes.
  virtual bool isFlipped() { return false;  }
  virtual bool resize(int w, int h)=0;

  virtual HDC getDC() { return 0; } // only sysbitmaps have to implement this


  virtual INT_PTR Extended(int id, void* data) { return 0; }
};

#define LICE_EXT_GET_SCALING 0x2001 // data ignored, returns .8 fixed point, returns 0 if unscaled
#define LICE_EXT_GET_ADVISORY_SCALING 0x2003 // data ignored, returns .8 fixed point. returns 0 if unscaled
#define LICE_EXT_GET_ANY_SCALING 0x2004 // data ignored, returns .8 fixed point, 0 if unscaled

#endif

#if !defined(_WIN32) && !defined(WM_CREATE)
#define WM_CREATE                       0x0001
#define WM_DESTROY                      0x0002
#define WM_PAINT                        0x000F
#define WM_ERASEBKGND                   0x0014
#define WM_SETCURSOR                    0x0020
#define WM_GETMINMAXINFO                0x0024
#define WM_NCPAINT                      0x0085
#define WM_KEYDOWN                      0x0100
#define WM_MOUSEFIRST                   0x0200
#define WM_MOUSEMOVE                    0x0200
#define WM_LBUTTONDOWN                  0x0201
#define WM_LBUTTONUP                    0x0202
#define WM_LBUTTONDBLCLK                0x0203
#define WM_RBUTTONDOWN                  0x0204
#define WM_RBUTTONUP                    0x0205
#define WM_RBUTTONDBLCLK                0x0206
#define WM_MBUTTONDOWN                  0x0207
#define WM_MBUTTONUP                    0x0208
#define WM_MBUTTONDBLCLK                0x0209
#define WM_MOUSEWHEEL                   0x020A
#define WM_MOUSEHWHEEL                  0x020E
#define WM_MOUSELAST                    0x020A

#endif
