#if !(defined(__MINGW32__) && !defined(__MINGW64__))
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stringapiset.h>
#endif
#ifdef _WIN32
#define LIB_DIR "bin"
#define LIB_EXT "dll"
#define LIB_PREFIX ""
#define PATH_SEP "\\"
#elif __APPLE__
#define LIB_DIR "lib"
#define LIB_EXT "dylib"
#define LIB_PREFIX "lib"
#define PATH_SEP "/"
#else
#define LIB_DIR "lib"
#define LIB_EXT "so"
#define LIB_PREFIX "lib"
#define PATH_SEP "/"
#endif

#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <strsafe.h>
#else
#include <dlfcn.h>
#include <sys/param.h>
#endif
#include <string.h>
#include "Util.hxx"

const unsigned int WIDTH = 600;
const unsigned int HEIGHT = 450;

int main(int argc, char **argv)
{
  bool pass = false, listen_mode = false, test_mode = false, help_mode = false;
  int width = WIDTH, height = HEIGHT, listen_port = -1;
  std::string test_commands_file_path = "";
  void *run;
#ifdef _WIN32
  HINSTANCE handle;
#else
  void *handle;
#endif

  // Ensure that the `GRDIR` environment variable is set, so GR can find its components like fonts.
  try
    {
      util::setGrdir();
    }
  // Catch an exception, print an error message but ignore it. If GR is located in its install location,
  // no environment variables need to be set at all.
  catch (std::exception &e)
    {
#ifdef _WIN32
      int needed_wide_chars = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
      std::vector<wchar_t> what_wide(needed_wide_chars);
      MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, what_wide.data(), needed_wide_chars);
      std::wcerr << what_wide.data() << std::endl;
#else
      std::cerr << e.what() << std::endl;
#endif
      std::cerr << "Failed to set the \"GRDIR\" environment variable, falling back to GRDIR=\"" << GRDIR << "\"."
                << std::endl;
    }

  if (argc > 1)
    {
      for (int i = 1; i < argc; i++) // parse the -- attributes
        {
          if (strcmp(argv[i], "--size") == 0)
            {
              if (argc > i + 2)
                {
                  std::vector<std::string> strings;
                  std::string size;
                  auto s = argv[i + 1];
                  std::istringstream size_stream(s);
                  while (getline(size_stream, size, ','))
                    {
                      strings.push_back(size);
                    }
                  if (!strings.empty() && strings.size() == 2)
                    {
                      width = stoi(strings[0]);
                      height = stoi(strings[1]);
                      i += 1;
                    }
                  else
                    {
                      fprintf(stderr, "Given size is invalid, use default size (%d,%d).\n", WIDTH, HEIGHT);
                    }
                }
              else
                {
                  fprintf(stderr, "No size given after \"--size\" parameter, use default size (%d, %d).\n", WIDTH,
                          HEIGHT);
                }
            }
          else if (strcmp(argv[i], "--listen") == 0)
            {
              listen_mode = true;
              if (argc > i + 1)
                {
                  listen_port = atoi(argv[i + 1]);
                  if (listen_port <= 0 || listen_port > 65535)
                    {
                      fprintf(stderr,
                              "Port to listen on given after \"--listen\" is invalid, using default port 8002\n");
                      listen_port = 8002;
                    }
                  else
                    {
                      i += 1;
                    }
                }
              else
                {
                  fprintf(stderr, "No port to listen on given after \"--listen\", using default port 8002\n");
                  listen_port = 8002;
                }
            }
          else if (strcmp(argv[i], "--test") == 0)
            {
              if (argc > i + 2 && !util::startsWith(argv[i + 1], "--"))
                {
                  test_commands_file_path = argv[i + 1];
                  test_mode = true;
                  i += 1;
                }
              else
                {
                  fprintf(stderr, "No test commands file given after \"--test\" parameter.\n");
                }
            }
          else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) /* help page should be shown */
            {
              auto path = util::getEnvVar(W("GRDIR"), W(GRDIR)) + W("/share/doc/grplot/grplot.man.md");
              if (!util::fileExists(path))
                {
                  fprintf(stderr, "Helpfile not found\n");
                  return 1;
                }
              pass = true;
              help_mode = true;
              i += 1;
            }
          else
            {
              argv += (i - 1);
              argc -= (i - 1);
              break;
            }
          if (argc <= 2 && !listen_mode && !help_mode)
            {
              fprintf(stderr, "Not enough command line arguments: specify an input file or the \"--listen\" option.\n");
              return 1;
            }
        }
    }
  else
    {
      fprintf(stderr, "Usage: grplot <FILE> [<KEY:VALUE>] ...\n  -h, --help\n");
      return 0;
    }

#ifdef _WIN32
  std::vector<std::wstring> library_to_try;
#else
  std::vector<std::string> library_to_try;
#endif
  auto grdir = util::getEnvVar(W("GRDIR"), W(GRDIR));
  if ((getenv("GKS_WSTYPE") == nullptr || strcmp(getenv("GKS_WSTYPE"), "381") == 0 || test_mode)
#ifdef __unix__
      && ((getenv("DISPLAY") != nullptr && getenv("DISPLAY")[0] != '\0') ||
          (getenv("WAYLAND_DISPLAY") != nullptr && getenv("WAYLAND_DISPLAY")[0] != '\0'))
#endif
  )
    {
      library_to_try.push_back(W("GUI"));
    }
  library_to_try.push_back(W("Console"));

  decltype(grdir) path_name;
  for (const auto &lib : library_to_try)
    {
      path_name = grdir;
      path_name += W(PATH_SEP);
      path_name += W(LIB_DIR);
      path_name += W(PATH_SEP);
      path_name += W(LIB_PREFIX);
      path_name += W("grplot");
      path_name += lib;
      path_name += W("." LIB_EXT);
#ifdef _WIN32
      handle = LoadLibraryW(path_name.c_str());
      run = (void *)GetProcAddress(handle, "run");
#else
      handle = dlopen(path_name.c_str(), RTLD_LAZY);
      run = dlsym(handle, "run");
#endif
      if (handle != nullptr)
        {
          break;
        }
      else if (lib == W("GUI"))
        {
          fprintf(stderr, "Could not load the GRPlot GUI library, falling back to console mode.\n");
        }
    }

  if (handle == nullptr)
    {
      fprintf(
          stderr,
          "Internal error: Could not load any GRPlot backend, although the console mode should always be available.\n");
#ifdef _WIN32
      fwprintf(stderr, L"Expected the console library in \"%ls\".\n", path_name.c_str());
#else
      fprintf(stderr, "Expected the console library in \"%s\".\n", path_name.c_str());
#endif
      return 1;
    }
  if (run == nullptr)
    {
      fprintf(stderr, "Internal error: Could not load the \"run\" function from the GRPlot library.\n");
#ifdef _WIN32
      fwprintf(stderr, L"Expected the \"run\" function in \"%ls\".\n", path_name.c_str());
#else
      fprintf(stderr, "Expected the \"run\" function in \"%s\".\n", path_name.c_str());
#endif
      return 1;
    }

  reinterpret_cast<int (*)(int, char **, bool, bool, bool, bool, int, int, int, const std::string &)>(run)(
      argc, argv, pass, listen_mode, test_mode, help_mode, width, height, listen_port, test_commands_file_path);
#ifdef _WIN32
  FreeLibrary(handle);
#else
  dlclose(handle);
#endif
}
#else
#include <iostream>

int main(int argc, char **argv)
{
  std::cerr << "grplot is not supported on MinGW 32-bit." << std::endl;
  return 1;
}
#endif
