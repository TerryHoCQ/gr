#ifndef GR_GRPLOTGUI_HXX
#define GR_GRPLOTGUI_HXX

#include <string>
#include "grm/util.h"

extern "C" GRM_EXPORT int run(int argc, char **argv, bool pass, bool listen_mode, bool test_mode, bool help_mode,
                              int width, int height, int listen_port, const std::string &test_commands_file_path);

#endif // GR_GRPLOTGUI_HXX
