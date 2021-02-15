#ifndef _REAPER_PLUGIN_FX_EMBED_H_
#define _REAPER_PLUGIN_FX_EMBED_H_


/*
 * to support via VST2: canDo("hasCockosEmbeddedUI") should return 0xbeef0000
 * dispatcher will be called with opcode=effVendorSpecific, index=effEditDraw, value=parm2, ptr=(void*)(INT_PTR)parm3, opt=message (REAPER_FXEMBED_WM_*)
 *
 * to support via VST3: IController should support IReaperUIEmbedInterface, see reaper_vst3_interfaces.h
 *
 * to support via LV2: todo
 */

// these alias to win32's WM_*


#define REAPER_FXEMBED_WM_IS_SUPPORTED                 0x0000
/* return 1 if embedding is supported and available
 * return -1 if embedding is supported and unavailable
 * return 0 if embedding is not supported
*/

#define REAPER_FXEMBED_WM_CREATE                       0x0001 // called when embedding begins (return value ignored)
#define REAPER_FXEMBED_WM_DESTROY                      0x0002 // called when embedding ends (return value ignored)



typedef struct REAPER_FXEMBED_DrawInfo // alias of REAPER_inline_positioninfo
{
  double _res1;
  int _res2;
  double _res3;

  int width, height;
  int mouse_x, mouse_y;

  INT_PTR extra_flags;
} REAPER_FXEMBED_DrawInfo;

#define REAPER_FXEMBED_WM_PAINT                        0x000F
/*
 * draw embedded UI.
 * parm2: REAPER_FXEMBED_IBitmap * to draw into. note
 * parm3: REAPER_FXEMBED_DrawInfo *
 *
 * if extra_flags has 1 set, update is optional. if no change since last draw, return 0.
 * if extra_flags has 0x10000 set, left mouse button is down and captured
 * if extra_flags has 0x20000 set, right mouse button is down and captured
 *
 * HiDPI:
 * if REAPER_FXEMBED_IBitmap::Extended(REAPER_FXEMBED_EXT_GET_ADVISORY_SCALING,NULL) returns nonzero, then it is a 24.8 scalefactor for UI drawing
 *
 * return 1 if drawing occurred, 0 otherwise.
 *
 */

#define REAPER_FXEMBED_WM_SETCURSOR                    0x0020 // parm3: REAPER_FXEMBED_DrawInfo*. set mouse cursor and return 1, or return 0.

#define REAPER_FXEMBED_WM_GETMINMAXINFO                0x0024
/*
 * get size hints. parm3 = (REAPER_FXEMBED_SizeHints*). return 1 if supported
 * note that these are just hints, the actual size may vary
 */
typedef struct REAPER_FXEMBED_SizeHints { // alias to MINMAXINFO
  int preferred_aspect; // 16.16 fixed point (65536 = 1:1, 32768 = 1:2, etc)
  int minimum_aspect;   // 16.16 fixed point

  int _res1, _res2, _res3, _res4;

  int min_width, min_height;
  int max_width, max_height;
} REAPER_FXEMBED_SizeHints;

/*
 * mouse messages
 * parm3 = (REAPER_FXEMBED_DrawInfo*)
 * capture is automatically set on mouse down, released on mouse up
 */

#define REAPER_FXEMBED_WM_MOUSEMOVE                    0x0200
#define REAPER_FXEMBED_WM_LBUTTONDOWN                  0x0201
#define REAPER_FXEMBED_WM_LBUTTONUP                    0x0202
#define REAPER_FXEMBED_WM_LBUTTONDBLCLK                0x0203
#define REAPER_FXEMBED_WM_RBUTTONDOWN                  0x0204
#define REAPER_FXEMBED_WM_RBUTTONUP                    0x0205
#define REAPER_FXEMBED_WM_RBUTTONDBLCLK                0x0206
#define REAPER_FXEMBED_WM_MOUSEWHEEL                   0x020A



/*
 * bitmap interface
 * this is an alias of LICE_IBitmap etc from WDL/lice/lice.h
 *
 */
#define REAPER_FXEMBED_RGBA(r,g,b,a) (((b)&0xff)|(((g)&0xff)<<8)|(((r)&0xff)<<16)|(((a)&0xff)<<24))
#define REAPER_FXEMBED_GETB(v) ((v)&0xff)
#define REAPER_FXEMBED_GETG(v) (((v)>>8)&0xff)
#define REAPER_FXEMBED_GETR(v) (((v)>>16)&0xff)
#define REAPER_FXEMBED_GETA(v) (((v)>>24)&0xff)

#if defined(__APPLE__) && defined(__ppc__)
#define REAPER_FXEMBED_PIXEL_A 0
#define REAPER_FXEMBED_PIXEL_R 1
#define REAPER_FXEMBED_PIXEL_G 2
#define REAPER_FXEMBED_PIXEL_B 3
#else
#define REAPER_FXEMBED_PIXEL_B 0
#define REAPER_FXEMBED_PIXEL_G 1
#define REAPER_FXEMBED_PIXEL_R 2
#define REAPER_FXEMBED_PIXEL_A 3
#endif

#ifdef __cplusplus
class REAPER_FXEMBED_IBitmap // alias of LICE_IBitmap
{
public:
  virtual ~REAPER_FXEMBED_IBitmap() { }

  virtual unsigned int *getBits()=0;
  virtual int getWidth()=0;
  virtual int getHeight()=0;
  virtual int getRowSpan()=0; // includes any off-bitmap data. this is in sizeof(unsigned int) units, not bytes.
  virtual bool isFlipped() { return false;  }
  virtual bool resize(int w, int h)=0;

  virtual void *getDC() { return 0; } // do not use

  virtual INT_PTR Extended(int id, void* data) { return 0; }
};
#endif

#define REAPER_FXEMBED_EXT_GET_ADVISORY_SCALING 0x2003 // data ignored, returns .8 fixed point. returns 0 if unscaled

#ifdef LV2_H_INCLUDED
/*
 * LV2 API
 */

typedef void *LV2_FXEMBED_Controller;
typedef struct _LV2_FXEMBED_HostNotifications {
  LV2_FXEMBED_Controller controller;

  // mirror of LV2UI_Touch
  void (*touch)(LV2_FXEMBED_Controller, uint32_t port_index, bool grabbed);

  // mirror of LV2UI_Write_Function. host may (but is not required to) implement types other than port_protocol=0 (float value)
  void (*write_func)(LV2_FXEMBED_Controller controller,
                     uint32_t         port_index,
                     uint32_t         buffer_size,
                     uint32_t         port_protocol,
                     const void*      buffer);

  // for non-C++ users
  // scaling will be set to 0 for 100%, otherwise 24.8 fixed point (512=200%/retina)
  unsigned int *(*get_bitmap_info)(intptr_t ibitmap_ptr, uint32_t *width, uint32_t *height, uint32_t *rowspan_integers, uint32_t *scaling);

  // future expansion
  intptr_t (*extended)(LV2_FXEMBED_Controller controller, const char *uri, void *data);

} LV2_FXEMBED_HostNotifications;

#define LV2_FXEMBED__interface "https://cockos.com/lv2/fxembed#interface"
typedef struct _LV2_FXEMBED_Interface {

  // called from non-realtime context with REAPER_FXEMBED_WM_*
  intptr_t (*embed_message)(LV2_Handle lv2, LV2_FXEMBED_HostNotifications *callbacks, int msg, intptr_t parm2, intptr_t parm3);
} LV2_FXEMBED_Interface;

#endif // LV2_H_INCLUDED

#endif
