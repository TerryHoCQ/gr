#include <csignal>
#include <string>
#include <QApplication>
#include "GRPlotMainWindow.hxx"
#include "GRPlotGUI.hxx"
#ifdef _WIN32
#include <processenv.h>
#endif

static void handleInterrupt(int)
{
  /*
   * Trigger an application shutdown in Qt's event loop
   * -> close events are generated and processed
   */
  QApplication::quit();
}

int run(int argc, char **argv, bool pass, bool listen_mode, bool test_mode, bool help_mode, int width, int height,
        int listen_port, const std::string &test_commands_file_path)
{
  if (!pass && getenv("GKS_WSTYPE") != nullptr) return (grm_plot_from_file(argc, argv) != 1);

#ifdef _WIN32
  SetEnvironmentVariableW(L"GKS_QT_VERSION", std::to_wstring(QT_VERSION_MAJOR).c_str());
#else
  setenv("GKS_QT_VERSION", std::to_string(QT_VERSION_MAJOR).c_str(), 1);
#endif
  QApplication app(argc, argv);
  GRPlotMainWindow window(argc, argv, width, height, listen_mode, listen_port, test_mode,
                          QString::fromStdString(test_commands_file_path), help_mode);

  if (!listen_mode) window.show();
  std::signal(SIGINT, handleInterrupt);
  std::signal(SIGTERM, handleInterrupt);
  return app.exec();
}
