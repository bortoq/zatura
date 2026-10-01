/* SPDX-License-Identifier: Zlib */

#ifndef INDEX_ELEMENT_OBJECT_H
#define INDEX_ELEMENT_OBJECT_H

#include "types.h"

#include <gtk/gtk.h>

#define ZATURA_TYPE_INDEX_ELEMENT_OBJECT (zatura_index_element_object_get_type())

/* GObject wrapping zatura_index_element_t so it can live in a GListModel */
G_DECLARE_FINAL_TYPE(ZaturaIndexElementObject, zatura_index_element_object, ZATURA, INDEX_ELEMENT_OBJECT, GObject)

struct _ZaturaIndexElementObject {
  GObject parent_instance;
  char* title;                      /* escaped markup for column 1 */
  char* page_label;                 /* primary page string for column 2 */
  char* page_alt;                   /* alt page string for column 3 */
  zatura_index_element_t* element; /* link target */
  GListStore* children;             /* NULL for leaves */
};

#endif