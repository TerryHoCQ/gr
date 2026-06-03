#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/param.h>
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

#include <stdarg.h>
#include <grm/util_int.h>
#include <grm/utilcpp_int.hxx>
#include "plugin_loader_int.hxx"

#ifdef __EMSCRIPTEN__
#include "csv_plugin.hxx"
#endif


PluginLoader::PluginLoader(const std::string &plugin_name) : handle{nullptr}, plugin_name{plugin_name}, p{nullptr} {}
PluginLoader::PluginLoader(PluginLoader &&l)
    : handle{std::exchange(l.handle, nullptr)}, plugin_name{l.plugin_name}, p{std::exchange(l.p, nullptr)}
{
}
PluginLoader::~PluginLoader()
{
  if (handle)
    {
      if (p)
        {
          deallocatePlugin(p);
        }

#ifdef _WIN32
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
    }
}

Plugin *PluginLoader::getPlugin()
{
  if (p)
    {
      return p;
    }
  Plugin *p = allocatePlugin();

  if (!p)
    {
      return nullptr;
    }

  Plugin::ApiVersion version = p->apiVersion();
  if (version.major == CURRENT_MAJOR_VERSION && version.minor == CURRENT_MINOR_VERSION)
    {
      return p;
    }
  fprintf(stderr, "Wrong plugin API version %d.%d, expected %d.%d\n", version.major, version.minor,
          CURRENT_MAJOR_VERSION, CURRENT_MINOR_VERSION);
  return nullptr;
}

class LastDLError
{
public:
  LastDLError()
  {
#ifdef _WIN32
    auto error_code = GetLastError();
    LPWSTR error_message = nullptr;

    if (!FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&error_message, 0,
                        nullptr))
      {
        error_message_ = L"<Could not get error message>";
        return;
      }
    error_message_ = error_message;
    LocalFree(error_message);
#else
    error_message_ = dlerror();
#endif
  }

private:
#ifdef _WIN32
  std::wstring error_message_;
  friend std::wostream &operator<<(std::wostream &os, const LastDLError &ld)
#else
  std::string error_message_;
  friend std::ostream &operator<<(std::ostream &os, const LastDLError &ld)
#endif
  {
    os << ld.error_message_;
    return os;
  }
};

extern "C" {
typedef void *(*PluginAllocator)(void);
}
Plugin *PluginLoader::allocatePlugin()
{
  void *alloc = NULL;
#ifdef __EMSCRIPTEN__
  if (plugin_name == "csv_plugin")
    {
      alloc = reinterpret_cast<void *>(CsvAllocPlugin);
    }
#else
#ifdef _WIN32
  auto &cerr = std::wcerr;
#else
  auto &cerr = std::cerr;
#endif
  const std::string symbol = "allocPlugin";

  auto grdir = getEnvVar(W("GRDIR"), W(GRDIR));
  decltype(grdir) path_name;

  path_name = grdir;
  path_name += W(PATH_SEP);
  path_name += W(LIB_DIR);
  path_name += W(PATH_SEP);
  path_name += W(LIB_PREFIX);
  path_name += w(plugin_name);
  path_name += W("." LIB_EXT);

#ifdef _WIN32
  handle = LoadLibraryW(path_name.c_str());
#else
  handle = dlopen(path_name.c_str(), RTLD_LAZY);
#endif

  // -------------- load plugin allocator --------------
  if (handle == NULL)
    {
      cerr << "Failed to load plugin \"" << plugin_name.c_str() << "\", reason: \"" << LastDLError() << "\""
           << std::endl;
      return nullptr;
    }
#ifdef _WIN32
  alloc = (void *)GetProcAddress(handle, symbol.c_str());
#else
  alloc = dlsym(handle, symbol.c_str());
#endif
#endif
  if (alloc == NULL)
    {
#ifndef __EMSCRIPTEN__
      /*
       * On Windows, `cerr` is a `wostream` which is not compatible with `std::string`. HOWEVER, it can take `char *`
       * instead, converting each `char` to a `wchar_t` without any locale conversion (no UTF-8 support). In this case,
       * this is ok, since `symbol` and `plugin_name` are ASCII-only strings.
       */
      cerr << "Unresolved symbol \"" << symbol.c_str() << "\" in plugin \"" << plugin_name.c_str() << "\", reason: \""
           << LastDLError() << "\"" << std::endl;
#endif
      return nullptr;
    }

  PluginAllocator allocator{reinterpret_cast<PluginAllocator>(alloc)};

  p = static_cast<Plugin *>((*allocator)());
  return p;
}

extern "C" {
typedef void (*PluginDeallocator)(void *);
}
void PluginLoader::deallocatePlugin(Plugin *p)
{
  void *dealloc = NULL;
#ifdef __EMSCRIPTEN__
  if (plugin_name == "csv_plugin")
    {
      dealloc = reinterpret_cast<void *>(CsvDeallocPlugin);
    }
#else
#ifdef _WIN32
  auto &cerr = std::wcerr;
#else
  auto &cerr = std::cerr;
#endif
  const std::string symbol = "deallocPlugin";

  if (!handle)
    {
      cerr << "Trying to deallocate a plugin, but shared library is not open." << std::endl;
      return;
    }
#ifdef _WIN32
  dealloc = (void *)GetProcAddress(handle, symbol.c_str());
#else
  dealloc = dlsym(handle, symbol.c_str());
#endif
#endif
  if (dealloc == NULL)
    {
#ifndef __EMSCRIPTEN__
      cerr << "Unresolved symbol \"" << symbol.c_str() << "\" in plugin \"" << plugin_name.c_str() << "\", reason: \""
           << LastDLError() << "\"" << std::endl;
#endif
      return;
    }

  PluginDeallocator deallocator{reinterpret_cast<PluginDeallocator>(dealloc)};
  (*deallocator)(p);
}
