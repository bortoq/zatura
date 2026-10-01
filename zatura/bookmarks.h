/* SPDX-License-Identifier: Zlib */

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <stdbool.h>
#include "zatura.h"

struct zatura_bookmark_s {
  gchar* id;
  unsigned int page;
  double x;
  double y;
};

typedef struct zatura_bookmark_s zatura_bookmark_t;

/**
 * Create a bookmark and add it to the list of bookmarks.
 * @param zatura The zatura instance.
 * @param id The bookmark's id.
 * @param page The bookmark's page.
 * @return the bookmark instance or NULL on failure.
 */
zatura_bookmark_t* zatura_bookmark_add(zatura_t* zatura, const gchar* id, unsigned int page);

/**
 * Remove a bookmark from the list of bookmarks.
 * @param zatura The zatura instance.
 * @param id The bookmark's id.
 * @return true on success, false otherwise
 */
bool zatura_bookmark_remove(zatura_t* zatura, const gchar* id);

/**
 * Get bookmark from the list of bookmarks.
 * @param zatura The zatura instance.
 * @param id The bookmark's id.
 * @return The bookmark instance if it exists or NULL otherwise.
 */
zatura_bookmark_t* zatura_bookmark_get(zatura_t* zatura, const gchar* id);

/**
 * Initialize bookmark system
 * @param zatura The zatura instance.
 */
bool zatura_bookmarks_init(zatura_t* zatura);

/**
 * Load bookmarks for a specific file.
 * @param zatura The zatura instance.
 * @param file The file.
 * @return true on success, false otherwise
 */
bool zatura_bookmarks_load(zatura_t* zatura, const gchar* file);

/**
 * Free boomark system
 * @param zatura The zatura instance.
 */
void zatura_bookmarks_free(zatura_t* zatura);

#endif // BOOKMARKS_H
