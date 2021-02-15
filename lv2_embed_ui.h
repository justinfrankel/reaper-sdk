#ifndef _LV2_EMBED_UI_H_
#define _LV2_EMBED_UI_H_

/*********
 * NOT YET IMPLEMENTED, RFC
 */

#define LV2_EMBED_UI_URI "https://cockos.com/lv2/embed_ui"   ///< https://cockos.com/lv2/embed_ui
#define LV2_EMBED_UI_PREFIX LV2_EMBED_UI_URI "#"             ///< https://cockos.com/lv2/embed_ui#

#define LV2_EMBED_UI__EmbedUI     LV2_EMBED_UI_PREFIX "EmbedUI"   ///< https://cockos.com/lv2/embed_ui#EmbedUI


typedef struct _LV2_EMBED_UI_MouseState {
  uint32_t width, height;
  uint32_t mouse_x, mouse_y;
  uint32_t mouse_button_state;
} LV2_EMBED_UI_MouseState;

typedef struct _LV2_EMBED_UI_DrawState {
  LV2_EMBED_UI_MouseState state;

  unsigned int *framebuffer; // state.width x state.height, but framebuffer_span bytes per row
  uint32_t framebuffer_span;

  float dpi_scaling;
  bool update_optional;
} LV2_EMBED_UI_DrawState;

typedef struct _LV2_EMBED_UI_SizeHints {
  uint32_t preferred_aspect; // 16.16 fixed point (65536 = 1:1, 32768 = 1:2, etc)
  uint32_t minimum_aspect;   // 16.16 fixed point

  uint32_t min_width, min_height;
  uint32_t max_width, max_height;
} LV2_EMBED_UI_SizeHints;

typedef enum {
  LV2_EMBED_UI_atom=0,       //passed LV2_Atom
  LV2_EMBED_UI_getSizeHints, //passed LV2_EMBED_UI_SizeHints
  LV2_EMBED_UI_draw,         //passed LV2_EMBED_UI_DrawState
  LV2_EMBED_UI_setCursor,    //passed LV2_EMBED_UI_MouseState
  LV2_EMBED_UI_mouseMove,    //passed LV2_EMBED_UI_MouseState
  LV2_EMBED_UI_mouseDown,    //passed LV2_EMBED_UI_MouseState
  LV2_EMBED_UI_mouseUp,      //passed LV2_EMBED_UI_MouseState
  LV2_EMBED_UI_mouse2Down,   //passed LV2_EMBED_UI_MouseState
  LV2_EMBED_UI_mouse2Up,     //passed LV2_EMBED_UI_MouseState
} LV2_EMBED_UI_Message;

// returned as a LV2UI_Widget
typedef struct _LV2_EMBED_UI_Widget {
  int (*message_processor)(LV2UI_Handle handle, LV2UI_Controller controller, LV2_EMBED_UI_Message message, void *parameter);
} LV2_EMBED_UI_Widget;

#endif
