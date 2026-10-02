/* SPDX-License-Identifier: Zlib */
#ifndef ZATURA_REFLOW_H
#define ZATURA_REFLOW_H
#include "types.h"
/* Optional plugin symbol, preserving the existing plugin ABI. Layout is
 * called only after render workers and page objects have been released. */
typedef struct {
  bool (*supported)(zathura_document_t* document);
  zathura_error_t (*layout)(zathura_document_t* document, float width, float height,
                            float font_size, unsigned int* page);
} zatura_reflow_plugin_t;
/* v2 adds mirrored page margins; v1 remains available for older plugins. */
typedef struct {
  float top, bottom, outer, inner; /* points, after viewport conversion */
  unsigned int columns, first_column;
  bool right_to_left;
} zatura_reflow_margins_t;
typedef struct {
  zathura_error_t (*layout)(zathura_document_t* document, float width, float height,
                            float font_size, const zatura_reflow_margins_t* margins,
                            unsigned int* page);
} zatura_reflow_plugin_v2_t;
#endif
