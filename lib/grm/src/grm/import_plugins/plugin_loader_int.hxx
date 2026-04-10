#ifndef PLUGINLOADER_INT_HXX_INCLUDED
#define PLUGINLOADER_INT_HXX_INCLUDED

#include <string>
#include <utility>

#include "plugin_int.hxx"

#ifdef _WIN32
#include <libloaderapi.h>
#endif

class PluginLoader
{
public:
  static const unsigned int CURRENT_MAJOR_VERSION = 1;
  static const unsigned int CURRENT_MINOR_VERSION = 0;

  PluginLoader(const std::string &plugin_name);
  PluginLoader(PluginLoader &&l);
  ~PluginLoader();

  Plugin *getPlugin();

private:
  Plugin *allocatePlugin();
  void deallocatePlugin(Plugin *p);

  const std::string &plugin_name;
  Plugin *p;
#ifdef _WIN32
  HMODULE handle;
#else
  void *handle;
#endif
};

#endif /* ifndef PLUGINLOADER_INT_HXX_INCLUDED */
