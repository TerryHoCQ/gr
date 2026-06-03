#ifndef VDR_INT_HXX_INCLUDED
#define VDR_INT_HXX_INCLUDED

#include <string>
#include <unordered_map>
#include <memory>

#include "plugin_int.hxx"
#include "plugin_loader_int.hxx"

class Vdr
{
public:
  Vdr() = default;
  ~Vdr();

  DataSource *loadSourceForFile(const std::string &path);

private:
  static std::unordered_map<std::string, std::string> plugin_names;

  std::unordered_map<std::string, std::unique_ptr<PluginLoader>> loaders;

  Plugin *loadPlugin(const std::string &name);
  void unloadPlugin(const std::string &name);
};

#endif /* ifndef VDR_INT_HXX_INCLUDED */
