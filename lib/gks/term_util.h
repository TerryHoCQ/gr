#ifndef _TERM_UTIL_H_
#define _TERM_UTIL_H_

#ifndef _WIN32
#if __GNUC__ >= 4
#define HIDDEN __attribute__((visibility("hidden")))
#else
#define HIDDEN
#endif

enum kitty_image_protocol_support_t
{
  NO_KITTY_IMAGE_PROTOCOL = 0,
  KITTY_IMAGE_PROTOCOL,
  KITTY_IMAGE_PROTOCOL_WITH_UNICODE_PLACEHOLDERS
};

enum tmux_state_t
{
  NO_TMUX = 0,
  SINGLE_TMUX_SESSION,
  NESTED_TMUX_SESSION
};

HIDDEN void makeraw(void);
HIDDEN char *send_control_sequence(char group, const char *parameters, const char *terminator);
HIDDEN int have_iterm(void);
HIDDEN int have_kitty(void);
HIDDEN enum kitty_image_protocol_support_t have_kitty_image_protocol(void);
HIDDEN enum tmux_state_t have_tmux(void);
HIDDEN int term_size(int *rows, int *cols, int *width, int *height);

#undef HIDDEN

#endif
#endif /* _TERM_UTIL_H_ */
