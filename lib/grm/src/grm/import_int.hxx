#ifndef GRM_IMPORT_INT_HXX_INCLUDED
#define GRM_IMPORT_INT_HXX_INCLUDED

/* ######################### includes ############################################################################### */

#include <string>
#include <vector>
#include <grm/import.h>
#include "import_utils_int.hxx"


/* ######################### internal interface ##################################################################### */

/* ========================= functions ============================================================================== */

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ general ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void cleanupImportModule(void);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ import ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

std::string normalizeLine(const std::string &str);
grm_error_t readDataFile(const std::string &path, std::vector<std::vector<std::vector<double>>> &data,
                         std::vector<int> &x_data, std::vector<int> &y_data, std::vector<int> &error_data,
                         std::vector<std::string> &labels, grm_args_t *args, const char *colms, const char *x_colms,
                         const char *y_colms, const char *e_colms, PlotRange *ranges,
                         grm_special_axis_series_t *special_axis_series, InputFlags &input_flags);
int convertInputstreamIntoArgs(grm_args_t *args, grm_file_args_t *file_args, int argc, char **argv, PlotRange *ranges,
                               grm_special_axis_series_t *special_axis_series, InputFlags &input_flags);
grm_file_args_t *grm_file_args_new();
grm_special_axis_series_t *grm_special_axis_series_new();

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ utility ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void adjustRanges(double *, double *, double, double);

#endif // GRM_IMPORT_INT_HXX_INCLUDED
