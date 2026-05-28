#ifdef __unix__
/* Needed for popen and strdup functions */
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gkscore.h"
#include "term_util.h"

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEFAULT_HEIGHT_IN_CELLS 24
#define OSC_MAX_RESP_LEN 80
#define ST_TERMINATOR "\033\\"
/* 1x1 minimal png encoded in base64 */
#define TEST_PNG_BASE64                                        \
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/w" \
  "D/AP+gvaeTAAAAC0lEQVQImWNgAAIAAAUAAWJVMogAAAAASUVORK5CYII="

static struct termios saved_term;

static int is_off(const char *s)
{
  const char *off_values[] = {"off", "false", "deactivated", "disabled", "0", NULL};
  const char **off_value_ptr;

  for (off_value_ptr = off_values; *off_value_ptr != NULL; ++off_value_ptr)
    {
      if (strcmp(*off_value_ptr, s) == 0)
        {
          return 1;
        }
    }

  return 0;
}

void makeraw(void)
{
  struct termios term;

  term = saved_term;

  term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  term.c_oflag &= ~OPOST;
  term.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  term.c_cflag &= ~(CSIZE | PARENB);
  term.c_cflag |= CS8;
  term.c_cc[VMIN] = 0;
  term.c_cc[VTIME] = 2; /* 200ms */

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) < 0)
    {
      perror("tcsetattr");
    }
}

char *send_control_sequence(char group, const char *parameters, const char *terminator)
{
  char *req = NULL, *resp = NULL, *escaped_terminator = NULL;
  const char *terminator_ptr = NULL;
  size_t req_len = 0, resp_len = 0, terminator_len = 0, escaped_terminator_len = 0;
  ssize_t cc = 0;
  enum tmux_state_t in_tmux = have_tmux();

  if (terminator == NULL) terminator = ST_TERMINATOR;
  terminator_len = strlen(terminator);

  switch (in_tmux)
    {
    case NESTED_TMUX_SESSION:
        /* strlen("\033Ptmux;\033\033Ptmux;\033\033\033\033G\033\033\\\033\\") == 25 */;
      req_len = 25;
      break;
    case SINGLE_TMUX_SESSION:
      /* strlen("\033Ptmux;\033\033G\033\\") == 12; */
      req_len = 12;
      break;
    default:
      /* strlen("\033G") == 2; */
      req_len = 2;
      break;
    }

  req_len += strlen(parameters);

  if (in_tmux != NO_TMUX)
    {
      /* This is a worst case allocation (all characters need to be `\033` for this case) */
      escaped_terminator = gks_malloc((in_tmux == NESTED_TMUX_SESSION ? 4 : 2) * terminator_len + 1);
      terminator_ptr = terminator;
      while (*terminator_ptr != '\0')
        {
          /* for each level of tmux nesting, `\033` needs to be escaped further */
          if (*terminator_ptr == '\033')
            {
              if (in_tmux == NESTED_TMUX_SESSION)
                {
                  strcpy(escaped_terminator + escaped_terminator_len, "\033\033\033");
                  escaped_terminator_len += 3;
                }
              else
                {
                  escaped_terminator[escaped_terminator_len] = '\033';
                  escaped_terminator_len += 1;
                }
            }
          escaped_terminator[escaped_terminator_len] = *terminator_ptr;
          ++escaped_terminator_len;
          ++terminator_ptr;
        }
      req_len += escaped_terminator_len;
    }
  else
    {
      req_len += terminator_len;
    }


  req = gks_malloc(req_len + 1);

  switch (in_tmux)
    {
    case NESTED_TMUX_SESSION:
      snprintf(req, req_len + 1, "\033Ptmux;\033\033Ptmux;\033\033\033\033%c%s%s\033\033\\\033\\", group, parameters,
               escaped_terminator);
      break;
    case SINGLE_TMUX_SESSION:
      snprintf(req, req_len + 1, "\033Ptmux;\033\033%c%s%s\033\\", group, parameters, escaped_terminator);
      break;
    default:
      snprintf(req, req_len + 1, "\033%c%s%s", group, parameters, terminator);
      break;
    }

  free(escaped_terminator);

  if (isatty(STDIN_FILENO))
    {
      tcgetattr(STDIN_FILENO, &saved_term);
      makeraw();

      write(STDOUT_FILENO, req, req_len);
      fflush(stdout);

      resp = gks_malloc(OSC_MAX_RESP_LEN + 1);
      while ((cc = read(STDIN_FILENO, resp + resp_len, 1)) == 1 && resp_len < OSC_MAX_RESP_LEN)
        {
          /* Skip the OSC escape sequence `\033<group>` in the response buffer */
          if (resp_len == 1 && resp[0] == '\033' && resp[1] == group)
            {
              resp_len = 0;
              continue;
            }
          /* Check for `terminator` as message terminator */
          if (resp_len >= terminator_len && strcmp(resp + resp_len - terminator_len, terminator) == 0)
            {
              resp_len -= terminator_len - 1;
              break;
            }
          /* reponses should be terminated by `terminator`, but check for ST (\033\\) and BELL (\a) as well */
          if (resp_len > 0 && resp[resp_len - 1] == '\033' && resp[resp_len] == '\\')
            {
              --resp_len;
              break;
            }
          if (resp[resp_len] == '\a') break;
          ++resp_len;
        }
      resp[resp_len] = '\0';

      if (resp_len == 0)
        {
          free(resp);
          resp = NULL;
        }

      tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_term);
    }

  free(req);

  return resp;
}

int have_iterm(void)
{
  char *resp;
  static int is_iterm = -1;

  if (is_iterm != -1) return is_iterm;

  resp = send_control_sequence(']', "1337;ReportCellSize", NULL);
  is_iterm = resp != NULL && strstr(resp, "1337;ReportCellSize=") == resp ? 1 : 0;

  free(resp);

  return is_iterm;
}

int have_kitty(void)
{
  char *resp;
  static int is_kitty = -1;

  if (is_kitty != -1) return is_kitty;

  resp = send_control_sequence('P', "+q544e", NULL);
  /* `787465726d2d6b69747479` is the hex-encoded string `xterm-kitty` */
  is_kitty = resp != NULL && strcmp(resp, "1+r544e=787465726d2d6b69747479") == 0;

  free(resp);

  return is_kitty;
}

enum image_protocol_support_t have_image_protocol(void)
{
  enum image_protocol_support_t kitty_image_protocol_support = have_kitty_image_protocol();
  if (kitty_image_protocol_support != NO_IMAGE_PROTOCOL) return kitty_image_protocol_support;
  return have_iterm_image_protocol() ? ITERM_IMAGE_PROTOCOL : NO_IMAGE_PROTOCOL;
}

int have_iterm_image_protocol(void)
{
  char *resp;
  char *capabilities;
  static int iterm_image_protocol_support = -1;

  if (iterm_image_protocol_support != -1) return iterm_image_protocol_support;

  resp = send_control_sequence(']', "1337;Capabilities", NULL);
  iterm_image_protocol_support = 0;
  if (resp == NULL) return 0;
  capabilities = strchr(resp, '=') + 1;
  if (capabilities == NULL) return 0;
  iterm_image_protocol_support = strchr(capabilities, 'F') != NULL;

  return iterm_image_protocol_support;
}

enum image_protocol_support_t have_kitty_image_protocol(void)
{
  char *resp;
  static enum image_protocol_support_t kitty_image_protocol_support = -1;

  if (kitty_image_protocol_support != -1) return kitty_image_protocol_support;

  /* `1719413160` is an arbitrary randomly chosen id */
  resp = send_control_sequence('_', "Gi=1719413160,f=100,a=q,U=1;" TEST_PNG_BASE64, NULL);
  if (resp != NULL && strcmp(resp, "Gi=1719413160;OK") == 0)
    {
      kitty_image_protocol_support = KITTY_IMAGE_PROTOCOL_WITH_UNICODE_PLACEHOLDERS;
    }
  free(resp);
  if (kitty_image_protocol_support == -1)
    {
      resp = send_control_sequence('_', "Gi=1719413160,f=100,a=q;" TEST_PNG_BASE64, NULL);
      kitty_image_protocol_support =
          (resp != NULL && strcmp(resp, "Gi=1719413160;OK") == 0) ? KITTY_IMAGE_PROTOCOL : NO_IMAGE_PROTOCOL;
      free(resp);
    }

  return kitty_image_protocol_support;
}

enum tmux_state_t have_tmux(void)
{
  static enum tmux_state_t in_tmux = -1;
  const char *tmux_env_var = NULL, *term_env_var = NULL;

  if (in_tmux != -1) return in_tmux;

  in_tmux = NO_TMUX;

  tmux_env_var = gks_getenv("TMUX");
  term_env_var = gks_getenv("TERM");
  if (tmux_env_var != NULL ||
      (term_env_var != NULL && (strncmp(term_env_var, "screen", 6) == 0 || strncmp(term_env_var, "tmux", 4) == 0)))
    {
      /* Check if the tmux session is running locally, otherwise the server cannot be queried */
      if (tmux_env_var != NULL)
        {
          FILE *fp;
          char client_termname[80];

          /* Inside tmux we can query the tmux server for the outer terminal name */
          fp = popen("tmux display -p '#{client_termname}'", "r");
          if (fp == NULL)
            {
              /* Reading failed, assume a single tmux session */
              in_tmux = SINGLE_TMUX_SESSION;
            }
          /* Read the output a line at a time - output it. */
          else if (fgets(client_termname, sizeof(client_termname), fp) == NULL)
            {
              /* Reading failed, assume a single tmux session */
              in_tmux = SINGLE_TMUX_SESSION;
            }
          pclose(fp);
          /* if still unset */
          if (in_tmux == NO_TMUX)
            {
              in_tmux = (strncmp(client_termname, "screen", 6) == 0 || strncmp(client_termname, "tmux", 4) == 0)
                            ? NESTED_TMUX_SESSION
                            : SINGLE_TMUX_SESSION;
            }
        }
      else
        {
          in_tmux = SINGLE_TMUX_SESSION;
        }
    }

  return in_tmux;
}

int is_dark_term(void)
{
  int r, g, b;

  if (!term_background_color(&r, &g, &b))
    {
      /* If term background color couldn't be read, assume it's light */
      return 0;
    }

  /* Formula taken from <https://www.w3.org/TR/AERT/#color-contrast> */
  return 0.299 * r + 0.587 * g + 0.114 * b < 128;
}

struct inline_options_t parse_inline_env_var(void)
{
  static struct inline_options_t options = {-1, -1, INLINE_BACKEND_AUTO, -1, 0, DEFAULT_HEIGHT_IN_CELLS};
  char *options_string, *option;

  if (options.active != -1) return options;

  options_string = getenv("GKS_INLINE");
  if (options_string == NULL || *options_string == '\0')
    {
      options.active = 0;
      return options;
    }

  options_string = strdup(options_string);
  if (options_string == NULL)
    {
      options.active = 0;
      return options;
    }
  options.active = -1;
  option = strtok(options_string, ",");
  while (option != NULL)
    {
      char *key_value_delim_ptr = strpbrk(option, ":=");
      const char *key = option;
      const char *value;
      if (key_value_delim_ptr != NULL)
        {
          *key_value_delim_ptr = '\0';
          value = key_value_delim_ptr + 1;
        }
      else
        {
          value = "";
          /* Support options like `noscroll` instead of `scroll=off` */
          if (strlen(key) > 2 && strncmp(key, "no", 2) == 0)
            {
              key += 2;
              value = "off";
            }
        }
      /* A single `off`, `false`, `deactivated`, `disabled` or `0` deactivates inline display */
      if (*value == '\0' && is_off(key))
        {
          options.active = 0;
        }
      else if (strcmp(key, "scroll") == 0)
        {
          if (is_off(value))
            {
              options.scroll = INLINE_NO_SCROLL;
            }
          /*
           * TODO: Add support for `region` and `clear`
           * else if (strcmp(value, "region") == 0)
           *   {
           *     options.scroll = INLINE_SCROLL_REGION;
           *   }
           * else if (strcmp(value, "clear") == 0)
           *   {
           *     options.scroll = INLINE_CLEAR;
           *   }
           */
          else
            {
              options.scroll = INLINE_SCROLL;
            }
        }
      else if (strcmp(key, "backend") == 0)
        {
          if (strcmp(value, "auto") == 0)
            {
              options.backend = INLINE_BACKEND_AUTO;
            }
          else if (strcmp(value, "iterm") == 0)
            {
              options.backend = INLINE_BACKEND_ITERM;
            }
          else if (strcmp(value, "kitty") == 0)
            {
              options.backend = INLINE_BACKEND_KITTY;
            }
          else if (strcmp(value, "kitty-unicode") == 0)
            {
              options.backend = INLINE_BACKEND_KITTY_WITH_UNICODE_PLACEHOLDERS;
            }
        }
      else if (strcmp(key, "background") == 0)
        {
          if (strcmp(value, "auto") == 0)
            {
              options.background = INLINE_BACKGROUND_AUTO;
            }
          else if (strcmp(value, "light") == 0)
            {
              options.background = INLINE_BACKGROUND_LIGHT;
            }
          else if (strcmp(value, "dark") == 0)
            {
              options.background = INLINE_BACKGROUND_DARK;
            }
          else if (strcmp(value, "none") == 0)
            {
              options.background = INLINE_BACKGROUND_NONE;
            }
        }
      else if (strcmp(key, "size") == 0)
        {
          if (sscanf(value, "%ux%u", &options.width, &options.height) != 2)
            {
              options.width = 0;
              /* If the following `sscanf` fails, `options.height` is still set to `DEFAULT_HEIGHT_IN_CELLS` */
              sscanf(value, "%u", &options.height);
            }
        }
      option = strtok(NULL, ",");
    }
  if (options.active == -1)
    {
      options.active = 1;
    }
  /* Add compatibility with the older `GKS_SCROLL_ITERM` env variable */
  if (options.scroll == -1)
    {
      options.scroll = gks_getenv_bool("GKS_SCROLL_ITERM") ? INLINE_SCROLL : INLINE_NO_SCROLL;
    }
  /* Add compatibility with the `GKS_BACKGROUND` env variable */
  if (options.background == -1)
    {
      const char *background = gks_getenv("GKS_BACKGROUND");
      if (background != NULL)
        {
          if (strcmp(background, "light") == 0)
            options.background = INLINE_BACKGROUND_LIGHT;
          else if (strcmp(background, "dark") == 0)
            options.background = INLINE_BACKGROUND_DARK;
          else if (strcmp(background, "none") == 0)
            options.background = INLINE_BACKGROUND_NONE;
        }
      if (options.background == -1) options.background = INLINE_BACKGROUND_AUTO;
    }

  free(options_string);

  return options;
}

int term_size(int *rows, int *cols, int *width, int *height)
{
  char *resp;
  int read_term_size = 0;
  int rows_, cols_, width_, height_;

  resp = send_control_sequence('[', "18", "t");
  /* terminals should respond with `\033[8;<rows>;<cols>t` */
  if (resp != NULL && sscanf(resp, "8;%d;%d", &rows_, &cols_) == 2) read_term_size = 1;
  free(resp);

  if (!read_term_size) return 0;

  read_term_size = 0;
  resp = send_control_sequence('[', "14", "t");
  /* terminals should respond with `\033[4;<width>;<height>t` */
  if (resp != NULL && sscanf(resp, "4;%d;%d", &width_, &height_) == 2) read_term_size = 1;
  free(resp);

  if (read_term_size)
    {
      if (rows != NULL) *rows = rows_;
      if (cols != NULL) *cols = cols_;
      if (width != NULL) *width = width_;
      if (height != NULL) *height = height_;
    }

  return read_term_size;
}

/*!
 * \brief Query the terminal for its size, calcuate the cell size and cache the result for later queries.
 *
 * \param[out] cell_width
 * \param[out] cell_height
 * \return if the query was successful
 */
int term_cell_size(double *cell_width, double *cell_height)
{
  static double cell_width_ = -1, cell_height_ = -1;

  if (cell_width_ < 0 || cell_height_ < 0)
    {
      int rows, cols, width, height;
      term_size(&rows, &cols, &height, &width);
      if (rows > 0 && cols > 0 && width > 0 && height > 0)
        {
          cell_width_ = (double)width / cols;
          cell_height_ = (double)height / rows;
        }
    }

  if (cell_width_ >= 0 || cell_height_ >= 0)
    {
      if (cell_width != NULL) *cell_width = cell_width_;
      if (cell_height != NULL) *cell_height = cell_height_;
      return 1;
    }

  return 0;
}

int term_background_color(int *r, int *g, int *b)
{
  char *resp;
  int read_background_color = 0;
  int r_, g_, b_;

  if (have_kitty())
    {
      /* kitty will respond with `21;background=rgb:rr/gg/bb` */
      resp = send_control_sequence(']', "21;background=?", NULL);
      if (resp != NULL && sscanf(resp, "21;background=rgb:%2x/%2x/%2x", &r_, &g_, &b_) == 3) read_background_color = 1;
    }
  else
    {
      /*
       * xterm compatible terminals should respond with `11;rgb:rrrr/gggg/bbbb`
       * Only the first two values for each color channel need to be read.
       */
      resp = send_control_sequence(']', "11;?", NULL);
      if (resp != NULL && sscanf(resp, "11;rgb:%2x%*x/%2x%*x/%2x%*x", &r_, &g_, &b_) == 3) read_background_color = 1;
    }
  free(resp);

  if (read_background_color)
    {
      if (r != NULL) *r = r_;
      if (g != NULL) *g = g_;
      if (b != NULL) *b = b_;
    }

  return read_background_color;
}
#endif
