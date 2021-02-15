#ifndef _LV2_EMBED_UI_H_
#define _LV2_EMBED_UI_H_

/*********
 * NOT YET IMPLEMENTED, RFC
 */

#define LV2_EMBED_UI_URI "https://cockos.com/lv2/embed_ui"   ///< https://cockos.com/lv2/embed_ui
#define LV2_EMBED_UI_PREFIX LV2_EMBED_UI_URI "#"             ///< https://cockos.com/lv2/embed_ui#

#define LV2_EMBED_UI__EmbedUI     LV2_EMBED_UI_PREFIX "EmbedUI"   ///< https://cockos.com/lv2/embed_ui#EmbedUI



// 
typedef struct _LV2_EMBED_UI_MouseState {
  uint32_t width, height;
  uint32_t mouse_x, mouse_y;
  uint32_t mouse_button_state;
} LV2_EMBED_UI_MouseState;

typedef struct _LV2_EMBED_UI_DrawState {
  LV2_EMBED_UI_MouseState state;

  unsigned int *framebuffer;
  uint32_t framebuffer_w, framebuffer_h, framebuffer_span;
  float dpi_scaling;
  bool update_optional;
} LV2_EMBED_UI_DrawState;

// passed LV2_EMBED_UI_DrawState
#define LV2_EMBED_UI__Draw     LV2_EMBED_UI_PREFIX "draw"   ///< https://cockos.com/lv2/embed_ui#draw

// passed LV2_EMBED_UI_MouseState
#define LV2_EMBED_UI__setCursor     LV2_EMBED_UI_PREFIX "setCursor"   ///< https://cockos.com/lv2/embed_ui#setCursor
#define LV2_EMBED_UI__mouseMove     LV2_EMBED_UI_PREFIX "mouseMove"   ///< https://cockos.com/lv2/embed_ui#mouseMove
#define LV2_EMBED_UI__mouseDown     LV2_EMBED_UI_PREFIX "mouseDown"   ///< https://cockos.com/lv2/embed_ui#mouseDown
#define LV2_EMBED_UI__mouseUp       LV2_EMBED_UI_PREFIX "mouseUp"     ///< https://cockos.com/lv2/embed_ui#mouseUp
#define LV2_EMBED_UI__mouse2Down    LV2_EMBED_UI_PREFIX "mouse2Down"  ///< https://cockos.com/lv2/embed_ui#mouse2Down
#define LV2_EMBED_UI__mouse2Up      LV2_EMBED_UI_PREFIX "mouse2Up"    ///< https://cockos.com/lv2/embed_ui#mouse2Up

typedef struct _LV2_EMBED_UI_SizeHints {
  uint32_t preferred_aspect; // 16.16 fixed point (65536 = 1:1, 32768 = 1:2, etc)
  uint32_t minimum_aspect;   // 16.16 fixed point

  uint32_t min_width, min_height;
  uint32_t max_width, max_height;
} LV2_EMBED_UI_SizeHints;

// passed LV2_EMBED_UI_SizeHints
#define LV2_EMBED_UI__getSizeHints  LV2_EMBED_UI_PREFIX "getSizeHints"    ///< https://cockos.com/lv2/embed_ui#getSizeHints

// returned as a LV2UI_Widget
typedef struct _LV2_EMBED_UI_Widget {
  int (*message_processor)(LV2UI_Handle handle, LV2UI_Controller controller, const char *URI, void *parameter);
} LV2_EMBED_UI_Widget;

#endif
