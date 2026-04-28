#include "grm/import.h"
#include "grm/plot.h"
#include "GRPlotConsole.hxx"
#include "Util.hxx"
#ifdef _WIN32
#include <filesystem>
#else
#include <sys/wait.h>
#include "term_util.h"
#endif

int run(int argc, char **argv, bool pass, bool listen_mode, bool test_mode, bool help_mode, int width, int height,
        int listen_port, const std::string &test_commands_file_path)
{
  if (help_mode)
    {
      auto manual_filepath = util::getEnvVar(W("GRDIR"), W(GRDIR)) + W("/share/doc/grplot/grplot.man.md");
#ifndef _WIN32
      auto print_manual = [&manual_filepath](int fd) {
        std::ifstream file{manual_filepath};
        std::string line;

        while (getline(file, line))
          {
            write(fd, line.c_str(), line.length());
            write(fd, "\n", 1);
          }
      };

      bool showed_help_with_pager = [&print_manual]() {
        if (!isatty(STDOUT_FILENO)) return false; // stdout is not connected to a terminal (probably to a pipe)
        int pipe_fds[2];
        if (pipe(pipe_fds) == -1) return false;

        pid_t pid = fork();
        if (pid == -1) return false;

        if (pid == 0) // child process
          {
            // close unused write end
            close(pipe_fds[1]);
            // redirect stdin, so reads from stdin will read from the pipe's read end
            dup2(pipe_fds[0], STDIN_FILENO);
            // close unused read end, since stdin can be used now
            close(pipe_fds[0]);
            auto pager_cmd = util::getEnvVar("PAGER", "less");
            execlp(pager_cmd.c_str(), pager_cmd.c_str(), nullptr);
            // `execlp` completely replaces the current process, so if `exit` is reached, `execlp` failed
            exit(1);
          }
        else // parent process
          {
            // close unused read end
            close(pipe_fds[0]);
            print_manual(pipe_fds[1]);
            // signal EOF
            close(pipe_fds[1]);
            // wait for pager / child process to exit
            wait(NULL);
          }
        return true;
      }();
      if (!showed_help_with_pager) print_manual(STDOUT_FILENO);
#else
      std::ifstream file{std::filesystem::path(manual_filepath)};
      std::string line;
      while (getline(file, line))
        {
          std::cout << line << std::endl;
        }
#endif

      return 1;
    }

#ifndef _WIN32
  /*
   * In console mode, we need to check which kind of output we can actually use. If the terminal is capable of
   * displaying inline graphics, use this. Otherwise, write the output to a file.
   * But only do this if `GKS_INLINE` and `GKS_WSTYPE` are not set, because we don't want to overwrite the user's
   * settings.
   */
  if ((getenv("GKS_INLINE") == nullptr || getenv("GKS_INLINE")[0] == '\0') &&
      (getenv("GKS_WSTYPE") == nullptr || getenv("GKS_WSTYPE")[0] == '\0'))
    {
      bool use_inline = false;
      if (isatty(STDOUT_FILENO))
        {
          enum image_protocol_support_t protocol = have_image_protocol();
          if (protocol != NO_IMAGE_PROTOCOL)
            {
              std::string gks_inline_config{"background=light,backend="};
              switch (protocol)
                {
                case NO_IMAGE_PROTOCOL: // is handled before
                  break;
                case ITERM_IMAGE_PROTOCOL:
                  gks_inline_config += "iterm";
                  break;
                case KITTY_IMAGE_PROTOCOL:
                  gks_inline_config += "kitty";
                  break;
                case KITTY_IMAGE_PROTOCOL_WITH_UNICODE_PLACEHOLDERS:
                  gks_inline_config += "kitty-unicode";
                  break;
                default:
                  std::cerr << "Unexpected image protocol: " << protocol << std::endl;
                  gks_inline_config += "auto"; // this works but is inefficient, since GKS will run the detection again
                  break;
                }
              setenv("GKS_INLINE", gks_inline_config.c_str(), 1);
              use_inline = true;
            }
          else
            {
              std::cerr << "Inline graphics not available, falling back to file output." << std::endl;
            }
        }
      if (!use_inline)
        {
          /* If no image protocol support could be detected, write to a file instead. */
          setenv("GKS_WSTYPE", "pdf", 1);
          setenv("GKS_FILEPATH", "grplot", 1);
          std::cout << "Writing to file \"grplot.pdf\"" << std::endl;
        }
    }
#else
  /* On Windows, inline graphics are not supported, so write to a file instead. */
  if (getenv("GKS_WSTYPE") == nullptr || getenv("GKS_WSTYPE")[0] == '\0')
    {
      SetEnvironmentVariableA("GKS_WSTYPE", "pdf");
      SetEnvironmentVariableA("GKS_FILEPATH", "grplot");
      std::cout << "Writing to file \"grplot.pdf\"" << std::endl;
    }
#endif

  if (width != 600 || height != 450)
    {
      auto args = grm_args_new();
      grm_args_push(args, "size", "dd", static_cast<double>(width), static_cast<double>(height));
      grm_merge(args);
      if (grm_interactive_plot_from_file(args, argc, argv) == 1)
        {
          grm_plot(args);
          grm_args_delete(args);
          grm_finalize();
          return 1;
        }
    }
  else
    {
      std::string file_name, optional_file;
      for (int i = 1; i < argc; i++)
        {
          std::string token = argv[i];

          /* parameter needed for import.cxx are handled differently than grm-parameters */
          if (util::startsWith(token, "file:"))
            {
              file_name = token.substr(5, token.length() - 1);
              break;
            }
          else if (i == 1 && (token.find(":") == std::string::npos ||
                              // Check if is an absolute Windows path, like `C:\Users\...`
                              (token.length() > 2 && isalpha(token[0]) && token[1] == ':' &&
                               (token[2] == '/' || token[2] == '\\'))))
            {
              optional_file = token; /* it's only used, if no "file:" keyword was found */
            }
        }
      if (file_name.empty()) file_name = optional_file;

      if (util::endsWith(file_name, ".xml.png") || util::endsWith(file_name, ".xml"))
        {
#ifndef NO_XERCES_C
          auto file = fopen(file_name.c_str(), "rb");
          grm_load_graphics_tree(file);
          fclose(file);
          auto ret = grm_render();
          grm_finalize();
          return ret;
#else
          std::stringstream text_stream;
          text_stream << "XML support not compiled in. Please recompile GRPlot with xerces support.";
          fprintf(stderr, "File open not possible. %s\n", text_stream.str().c_str());
          return 0;
#endif
        }
      auto ret = grm_plot_from_file(argc, argv) != 1;
      grm_finalize();
      return ret;
    }
  return 0;
}
