#ifndef GRM_INTERACTION_INT_H_INCLUDED
#define GRM_INTERACTION_INT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* ######################### includes ############################################################################### */

#include <grm/interaction.h>

#include "datatype/template/list_int.h"


/* ######################### internal interface ##################################################################### */

/* ========================= datatypes ============================================================================== */

/* ------------------------- tooltip list --------------------------------------------------------------------------- */

DECLARE_LIST_TYPE(tooltip, grm_tooltip_info_t *)

#undef DECLARE_LIST_TYPE


/* ========================= macros ================================================================================= */

/* ------------------------- user interaction ----------------------------------------------------------------------- */

#define INPUT_ANGLE_DELTA_FACTOR 0.001
#define INPUT_DEFAULT_KEEP_ASPECT_RATIO 1


/* ========================= functions ============================================================================== */

/* ------------------------- user interaction ----------------------------------------------------------------------- */

err_t get_tooltips(const int mouse_x, const int mouse_y, err_t (*tooltip_callback)(int, int, grm_tooltip_info_t *));


/* ========================= methods ================================================================================ */

/* ------------------------- tooltip list --------------------------------------------------------------------------- */

DECLARE_LIST_METHODS(tooltip)

#undef DECLARE_LIST_METHODS


#ifdef __cplusplus
}
#endif
#endif /* ifndef GRM_INTERACTION_INT_H_INCLUDED */
