/* SPDX-License-Identifier: Zlib */

#include "jumplist.h"

#include <girara/utils.h>
#include <girara/log.h>
#include <math.h>

#include "zatura.h"
#include "document.h"
#include "database.h"

static void zatura_jumplist_reset_current(zatura_t* zatura) {
  g_return_if_fail(zatura != NULL && zatura->jumplist.cur != NULL);

  while (girara_list_iterator_has_next(zatura->jumplist.cur) == true) {
    girara_list_iterator_next(zatura->jumplist.cur);
  }
}

static void zatura_jumplist_append_jump(zatura_t* zatura) {
  g_return_if_fail(zatura != NULL && zatura->jumplist.list != NULL);

  zatura_jump_t* jump = g_try_malloc0(sizeof(zatura_jump_t));
  if (jump == NULL) {
    return;
  }

  girara_list_append(zatura->jumplist.list, jump);

  if (zatura->jumplist.size == 0) {
    zatura->jumplist.cur = girara_list_iterator(zatura->jumplist.list);
  }

  ++zatura->jumplist.size;
  zatura_jumplist_trim(zatura);
}

static void zatura_jumplist_save(zatura_t* zatura) {
  g_return_if_fail(zatura_has_document(zatura) == true);

  zatura_jump_t* cur = zatura_jumplist_current(zatura);
  if (cur != NULL) {
    zatura_document_t* document = zatura_get_document(zatura);
    cur->x                       = zatura_document_get_position_x(document);
    cur->y                       = zatura_document_get_position_y(document);
    cur->page                    = zatura_document_get_current_page_number(document);
  }
}

bool zatura_jumplist_has_previous(zatura_t* zatura) {
  return girara_list_iterator_has_previous(zatura->jumplist.cur);
}

bool zatura_jumplist_has_next(zatura_t* zatura) {
  return girara_list_iterator_has_next(zatura->jumplist.cur);
}

zatura_jump_t* zatura_jumplist_current(zatura_t* zatura) {
  if (zatura->jumplist.cur != NULL) {
    return girara_list_iterator_data(zatura->jumplist.cur);
  } else {
    return NULL;
  }
}

void zatura_jumplist_forward(zatura_t* zatura) {
  if (girara_list_iterator_has_next(zatura->jumplist.cur)) {
    girara_list_iterator_next(zatura->jumplist.cur);
  }
}

void zatura_jumplist_backward(zatura_t* zatura) {
  if (girara_list_iterator_has_previous(zatura->jumplist.cur)) {
    girara_list_iterator_previous(zatura->jumplist.cur);
  }
}

void zatura_jumplist_trim(zatura_t* zatura) {
  g_return_if_fail(zatura != NULL && zatura->jumplist.list != NULL && zatura->jumplist.size != 0);

  girara_list_iterator_t* cur = girara_list_iterator(zatura->jumplist.list);

  while (zatura->jumplist.size > zatura->jumplist.max_size) {
    if (girara_list_iterator_data(cur) == girara_list_iterator_data(zatura->jumplist.cur)) {
      girara_list_iterator_free(zatura->jumplist.cur);
      zatura->jumplist.cur = NULL;
    }

    girara_list_iterator_remove(cur);
    --zatura->jumplist.size;
  }

  if (zatura->jumplist.size == 0 || zatura->jumplist.cur != NULL) {
    girara_list_iterator_free(cur);
  } else {
    zatura->jumplist.cur = cur;
  }
}

void zatura_jumplist_add(zatura_t* zatura) {
  g_return_if_fail(zatura_has_document(zatura) == true && zatura->jumplist.list != NULL);

  zatura_document_t* document = zatura_get_document(zatura);
  double x                     = zatura_document_get_position_x(document);
  double y                     = zatura_document_get_position_y(document);

  if (zatura->jumplist.size != 0) {
    zatura_jumplist_reset_current(zatura);

    zatura_jump_t* cur = zatura_jumplist_current(zatura);
    if (cur != NULL) {
      if (fabs(cur->x - x) <= DBL_EPSILON && fabs(cur->y - y) <= DBL_EPSILON) {
        return;
      }
    }
  }

  zatura_jumplist_append_jump(zatura);
  zatura_jumplist_reset_current(zatura);
  zatura_jumplist_save(zatura);
}

void zatura_jumplist_set_max_size(zatura_t* zatura, size_t max_size) {
  zatura->jumplist.max_size = max_size;
  if (zatura->jumplist.list != NULL && zatura->jumplist.size != 0) {
    zatura_jumplist_trim(zatura);
  }
}

bool zatura_jumplist_load(zatura_t* zatura, const char* file) {
  g_return_val_if_fail(zatura != NULL && file != NULL, false);

  if (zatura->database == NULL) {
    return false;
  }

  girara_list_t* list = zatura_db_load_jumplist(zatura->database, file);
  if (list == NULL) {
    girara_error("Failed to load the jumplist from the database");
    return false;
  }

  girara_list_free(zatura->jumplist.list);
  zatura->jumplist.list = list;
  zatura->jumplist.size = girara_list_size(zatura->jumplist.list);

  if (zatura->jumplist.size != 0) {
    zatura->jumplist.cur = girara_list_iterator(zatura->jumplist.list);
    zatura_jumplist_reset_current(zatura);
    zatura_jumplist_trim(zatura);
    girara_debug("Loaded the jumplist from the database");
  } else {
    girara_debug("No jumplist for this file in the database yet");
  }

  return true;
}

void zatura_jumplist_init(zatura_t* zatura, size_t max_size) {
  zatura->jumplist.max_size = max_size;
  zatura->jumplist.list     = girara_list_new_with_free(g_free);
  zatura->jumplist.size     = 0;
  zatura->jumplist.cur      = NULL;
}

bool zatura_jumplist_is_initialized(zatura_t* zatura) {
  return zatura->jumplist.list != NULL;
}

void zatura_jumplist_clear(zatura_t* zatura) {
  if (zatura == NULL) {
    return;
  }

  /* remove jump list */
  girara_list_iterator_free(zatura->jumplist.cur);
  zatura->jumplist.cur = NULL;
  girara_list_clear(zatura->jumplist.list);
  zatura->jumplist.size = 0;
}

void zatura_jumplist_free(zatura_t* zatura) {
  if (zatura == NULL) {
    return;
  }

  /* remove jump list */
  zatura_jumplist_clear(zatura);
  girara_list_free(zatura->jumplist.list);
  zatura->jumplist.list = NULL;
}
