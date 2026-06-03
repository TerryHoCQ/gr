#ifndef GRM_IMPORT_H_INCLUDED
#define GRM_IMPORT_H_INCLUDED

/* ######################### includes ############################################################################### */

#include "args.h"

#ifdef __cplusplus
#include <memory>
#include <string>
#include "grm/dom_render/context.hxx"

extern "C" {
#endif


/* ######################### public interface ####################################################################### */

/* ========================= functions ============================================================================== */

/* ------------------------- plot ----------------------------------------------------------------------------------- */

GRM_EXPORT int grm_interactive_plot_from_file(grm_args_t *args, int argc, char **argv);
GRM_EXPORT int grm_plot_from_file(int argc, char **argv);

#ifdef __cplusplus
}
GRM_EXPORT int grm_context_data_from_file(const std::shared_ptr<GRM::Context> &context, const std::string &path,
                                          bool interpret_matrix_as_one_column = false);
#endif
#endif /* GRM_IMPORT_H_INCLUDED */
