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
#define LV2_EMBED_UI__Draw     LV2_EMBED_UI_PREFIX "Draw"   ///< https://cockos.com/lv2/embed_ui#Draw

// passed LV2_EMBED_UI_MouseState
#define LV2_EMBED_UI__SetCursor     LV2_EMBED_UI_PREFIX "SetCursor"   ///< https://cockos.com/lv2/embed_ui#SetCursor
#define LV2_EMBED_UI__MouseMove     LV2_EMBED_UI_PREFIX "MouseMove"   ///< https://cockos.com/lv2/embed_ui#MouseMove
#define LV2_EMBED_UI__MouseDown     LV2_EMBED_UI_PREFIX "MouseDown"   ///< https://cockos.com/lv2/embed_ui#MouseDown
#define LV2_EMBED_UI__MouseUp       LV2_EMBED_UI_PREFIX "MouseUp"     ///< https://cockos.com/lv2/embed_ui#MouseUp
#define LV2_EMBED_UI__Mouse2Down    LV2_EMBED_UI_PREFIX "Mouse2Down"  ///< https://cockos.com/lv2/embed_ui#Mouse2Down
#define LV2_EMBED_UI__Mouse2Up      LV2_EMBED_UI_PREFIX "Mouse2Up"    ///< https://cockos.com/lv2/embed_ui#Mouse2Up

typedef struct _LV2_EMBED_UI_SizeHints {
  uint32_t preferred_aspect; // 16.16 fixed point (65536 = 1:1, 32768 = 1:2, etc)
  uint32_t minimum_aspect;   // 16.16 fixed point

  uint32_t min_width, min_height;
  uint32_t max_width, max_height;
} LV2_EMBED_UI_SizeHints;

// passed LV2_EMBED_UI_SizeHints
#define LV2_EMBED_UI__GetSizeHints  LV2_EMBED_UI_PREFIX "GetSizeHints"    ///< https://cockos.com/lv2/embed_ui#GetSizeHints

// returned as a LV2UI_Widget
typedef struct _LV2_EMBED_UI_Widget {
  int (*message_processor)(LV2UI_Handle handle, LV2UI_Controller controller, const char *URI, void *parameter);
} LV2_EMBED_UI_Widget;

#endif
