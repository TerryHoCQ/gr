#ifdef __unix__
/* Needed for popen function */
#define _POSIX_C_SOURCE 200112L
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

#define OSC_MAX_RESP_LEN 80
#define ST_TERMINATOR "\033\\"
/* 1x1 minimal png encoded in base64 */
#define TEST_PNG_BASE64                                        \
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/w" \
  "D/AP+gvaeTAAAAC0lEQVQImWNgAAIAAAUAAWJVMogAAAAASUVORK5CYII="

static struct termios saved_term;

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

enum kitty_image_protocol_support_t have_kitty_image_protocol(void)
{
  char *resp;
  static enum kitty_image_protocol_support_t kitty_image_protocol_support = -1;

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
          (resp != NULL && strcmp(resp, "Gi=1719413160;OK") == 0) ? KITTY_IMAGE_PROTOCOL : NO_KITTY_IMAGE_PROTOCOL;
      free(resp);
    }

  return kitty_image_protocol_support;
}

enum tmux_state_t have_tmux(void)
{
  static enum tmux_state_t in_tmux = -1;
  const char *term_env_var = NULL;

  if (in_tmux != -1) return in_tmux;

  in_tmux = NO_TMUX;

  term_env_var = gks_getenv("TERM");
  if (term_env_var != NULL && (strncmp(term_env_var, "screen", 6) == 0 || strncmp(term_env_var, "tmux", 4) == 0))
    {
      /* Check if the tmux session is running locally, otherwise the server cannot be queried */
      if (gks_getenv("TMUX") != NULL)
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
#endif
