/* SPDX-License-Identifier: Zlib */

#include "bookmarks.h"

#include <girara-gtk/session.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <string.h>

#include "database.h"
#include "document.h"
#include "adjustment.h"

static int bookmark_compare_find(const void* item, const void* data) {
  const zatura_bookmark_t* bookmark = item;
  const char* id                     = data;

  return g_strcmp0(bookmark->id, id);
}

zatura_bookmark_t* zatura_bookmark_add(zatura_t* zatura, const gchar* id, unsigned int page) {
  g_return_val_if_fail(zatura_has_document(zatura) == true && zatura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  zatura_document_t* document = zatura_get_document(zatura);
  double position_x            = zatura_document_get_position_x(document);
  double position_y            = zatura_document_get_position_y(document);
  zatura_bookmark_t* old      = zatura_bookmark_get(zatura, id);

  if (old != NULL) {
    old->page = page;
    old->x    = position_x;
    old->y    = position_y;

    const char* path = zatura_document_get_path(document);
    if (zatura_db_remove_bookmark(zatura->database, path, old->id) == false) {
      girara_warning("Failed to remove old bookmark from database.");
    }

    if (zatura_db_add_bookmark(zatura->database, path, old) == false) {
      girara_warning("Failed to add new bookmark to database.");
    }

    return old;
  }

  zatura_bookmark_t* bookmark = g_try_malloc0(sizeof(zatura_bookmark_t));
  if (bookmark == NULL) {
    return NULL;
  }

  bookmark->id   = g_strdup(id);
  bookmark->page = page;
  bookmark->x    = position_x;
  bookmark->y    = position_y;
  girara_list_append(zatura->bookmarks.bookmarks, bookmark);

  const char* path = zatura_document_get_path(document);
  if (zatura_db_add_bookmark(zatura->database, path, bookmark) == false) {
    girara_warning("Failed to add bookmark to database.");
  }

  return bookmark;
}

bool zatura_bookmark_remove(zatura_t* zatura, const gchar* id) {
  g_return_val_if_fail(zatura_has_document(zatura) == true && zatura->bookmarks.bookmarks, false);
  g_return_val_if_fail(id, false);

  zatura_bookmark_t* bookmark = zatura_bookmark_get(zatura, id);
  if (bookmark == NULL) {
    return false;
  }

  const char* path = zatura_document_get_path(zatura_get_document(zatura));
  if (zatura_db_remove_bookmark(zatura->database, path, bookmark->id) == false) {
    girara_warning("Failed to remove bookmark from database.");
  }

  girara_list_remove(zatura->bookmarks.bookmarks, bookmark);

  return true;
}

zatura_bookmark_t* zatura_bookmark_get(zatura_t* zatura, const gchar* id) {
  g_return_val_if_fail(zatura && zatura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  return girara_list_find(zatura->bookmarks.bookmarks, bookmark_compare_find, id);
}

static int zatura_bookmarks_compare(const void* lhs, const void* rhs) {
  if (lhs && rhs) {
    const zatura_bookmark_t* l = lhs;
    const zatura_bookmark_t* r = rhs;

    return g_strcmp0(l->id, r->id);
  }

  return memcmp(&lhs, &rhs, sizeof(lhs));
}

static void zatura_bookmark_free(void* data) {
  if (data != NULL) {
    zatura_bookmark_t* bookmark = data;
    g_free(bookmark->id);
    g_free(bookmark);
  }
}

bool zatura_bookmarks_init(zatura_t* zatura) {
  if (!zatura) {
    return false;
  }

  zatura->bookmarks.bookmarks = girara_sorted_list_new_with_free(zatura_bookmarks_compare, zatura_bookmark_free);

  return zatura->bookmarks.bookmarks != NULL;
}

bool zatura_bookmarks_load(zatura_t* zatura, const gchar* file) {
  g_return_val_if_fail(zatura && zatura->database, false);
  g_return_val_if_fail(file, false);

  girara_list_clear(zatura->bookmarks.bookmarks);
  return zatura_db_load_bookmarks(zatura->database, file, zatura->bookmarks.bookmarks);
}

void zatura_bookmarks_free(zatura_t* zatura) {
  if (zatura) {
    girara_list_free(zatura->bookmarks.bookmarks);
  }
}
