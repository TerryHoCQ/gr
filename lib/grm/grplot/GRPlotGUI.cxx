#include <QApplication>
#include "GRPlotMainWindow.hxx"
#include "GRPlotGUI.hxx"
#include "Util.hxx"

int run(int argc, char **argv, bool pass, bool listen_mode, bool test_mode, bool help_mode, int width, int height,
        int listen_port, const std::string &test_commands_file_path)
{
  if (!pass && getenv("GKS_WSTYPE") != nullptr) return (grm_plot_from_file(argc, argv) != 1);

  QApplication app(argc, argv);
  GRPlotMainWindow window(argc, argv, width, height, listen_mode, listen_port, test_mode,
                          QString::fromStdString(test_commands_file_path), help_mode);

  if (!listen_mode) window.show();
  return app.exec();
}
