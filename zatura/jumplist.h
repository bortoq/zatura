/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_JUMPLIST_H
#define ZATURA_JUMPLIST_H

#include <girara/datastructures.h>
#include "types.h"

typedef struct zatura_jumplist_s {
  girara_list_t* list;
  girara_list_iterator_t* cur;
  unsigned int size;
  unsigned int max_size;
} zatura_jumplist_t;

/**
 * Checks whether current jump has a previous jump
 *
 * @param zatura The zatura session
 * @return true if current jump has a previous jump
 */
bool zatura_jumplist_has_previous(zatura_t* zatura);

/**
 * Checks whether current jump has a next jump
 *
 * @param zatura The zatura session
 * @return true if current jump has a next jump
 */
bool zatura_jumplist_has_next(zatura_t* zatura);

/**
 * Return current jump in the jumplist
 *
 * @param zatura The zatura session
 * @return current jump
 */
zatura_jump_t* zatura_jumplist_current(zatura_t* zatura);

/**
 * Move forward in the jumplist
 *
 * @param zatura The zatura session
 */
void zatura_jumplist_forward(zatura_t* zatura);

/**
 * Move backward in the jumplist
 *
 * @param zatura The zatura session
 */
void zatura_jumplist_backward(zatura_t* zatura);

/**
 * Add current page as a new item to the jumplist after current position
 *
 * @param zatura The zatura session
 */
void zatura_jumplist_add(zatura_t* zatura);

/**
 * Trim entries from the beginning of the jumplist to maintain it's maximum size constraint.
 *
 * @param zatura The zatura session
 */
void zatura_jumplist_trim(zatura_t* zatura);

/**
 * Set maximum jump list size (and trim if necessary)
 *
 * @param zatura The zatura session
 * @param max_size New maximum size
 */
void zatura_jumplist_set_max_size(zatura_t* zatura, size_t max_size);

/**
 * Load the jumplist of the specified file
 *
 * @param zatura The zatura session
 * @param file The file whose jumplist is to be loaded
 *
 * return A linked list of zatura_jump_t structures constituting the jumplist of the specified file, or NULL.
 */
bool zatura_jumplist_load(zatura_t* zatura, const char* file);

/**
 * Init jumplist with a maximum size
 *
 * @param zatura The zatura session
 * @param max_size maximum jumplist size (or 0 for unbounded lists)
 */
void zatura_jumplist_init(zatura_t* zatura, size_t max_size);

/**
 * Check if the jumplist is initialized
 *
 * @param zatura The zatura session
 */
bool zatura_jumplist_is_initialized(zatura_t* zatura);

/**
 * Clear jumplist
 *
 * After this operation, the jumplist is empty but initialized.
 *
 * @param zatura The zatura session
 */
void zatura_jumplist_clear(zatura_t* zatura);

/**
 * Free jumplist
 *
 * @param zatura The zatura session
 */
void zatura_jumplist_free(zatura_t* zatura);

#endif
