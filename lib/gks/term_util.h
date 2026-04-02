#ifndef _TERM_UTIL_H_
#define _TERM_UTIL_H_

#ifndef _WIN32
#if __GNUC__ >= 4
#define HIDDEN __attribute__((visibility("hidden")))
#else
#define HIDDEN
#endif

enum image_protocol_support_t
{
  NO_IMAGE_PROTOCOL = 0,
  ITERM_IMAGE_PROTOCOL,
  KITTY_IMAGE_PROTOCOL,
  KITTY_IMAGE_PROTOCOL_WITH_UNICODE_PLACEHOLDERS
};

enum tmux_state_t
{
  NO_TMUX = 0,
  SINGLE_TMUX_SESSION,
  NESTED_TMUX_SESSION
};

enum inline_scroll_t
{
  INLINE_NO_SCROLL = 0,
  INLINE_SCROLL = 1,
  /*
   * TODO: Support these options later:
   * INLINE_SCROLL_REGION = 2,
   * INLINE_CLEAR = 3,
   */
};

enum inline_backend_t
{
  INLINE_BACKEND_AUTO = 0,
  INLINE_BACKEND_ITERM = 1,
  INLINE_BACKEND_KITTY = 2,
  INLINE_BACKEND_KITTY_WITH_UNICODE_PLACEHOLDERS = 3,
};

enum inline_background_t
{
  INLINE_BACKGROUND_AUTO = 0,
  INLINE_BACKGROUND_LIGHT = 1,
  INLINE_BACKGROUND_DARK = 2,
  INLINE_BACKGROUND_NONE = 3,
};

struct inline_options_t
{
  int active;
  enum inline_scroll_t scroll;
  enum inline_backend_t backend;
  enum inline_background_t background;
  int width;
  int height;
};

HIDDEN void makeraw(void);
HIDDEN char *send_control_sequence(char group, const char *parameters, const char *terminator);
HIDDEN int have_iterm(void);
HIDDEN int have_kitty(void);
HIDDEN enum image_protocol_support_t have_image_protocol(void);
HIDDEN int have_iterm_image_protocol(void);
HIDDEN enum image_protocol_support_t have_kitty_image_protocol(void);
HIDDEN enum tmux_state_t have_tmux(void);
HIDDEN int is_dark_term(void);
HIDDEN struct inline_options_t parse_inline_env_var(void);
HIDDEN int term_size(int *rows, int *cols, int *width, int *height);
HIDDEN int term_background_color(int *r, int *g, int *b);

#undef HIDDEN

#endif
#endif /* _TERM_UTIL_H_ */
