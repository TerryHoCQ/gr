#include "grm/import.h"
#include "GRPlotConsole.hxx"
#include "Util.hxx"

int run(int argc, char **argv, bool pass, bool listen_mode, bool test_mode, bool help_mode, int width, int height,
        int listen_port, const std::string &test_commands_file_path)
{
  if (help_mode)
    {
      std::string line;
      std::ifstream file(std::string(GRDIR) + "/share/doc/grplot/grplot.man.md");

      while (getline(file, line))
        {
          std::cout << line << std::endl;
        }
      return 1;
    }

  if (width != 600 || height != 450)
    {
      auto args = grm_args_new();
      grm_args_push(args, "size", "dd", static_cast<double>(width), static_cast<double>(height));
      grm_merge(args);
      if (grm_interactive_plot_from_file(args, argc, argv) == 1)
        {
          grm_plot(args);
          grm_args_delete(args);
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
          else if (i == 1 && (token.find(":") == std::string::npos || (token.find(":") == 1 && token.find('/') == 2)))
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
          return grm_render();
#else
          std::stringstream text_stream;
          text_stream << "XML support not compiled in. Please recompile GRPlot with xerces support.";
          fprintf(stderr, "File open not possible. %s\n", text_stream.str().c_str());
          return 0;
#endif
        }
      return grm_plot_from_file(argc, argv) != 1;
    }
  return 0;
}