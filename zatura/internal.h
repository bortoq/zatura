/* SPDX-License-Identifier: Zlib */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "zatura.h"
#include "plugin.h"

/**
 * Zatura password dialog
 */
typedef struct zatura_password_dialog_info_s {
  char* path;         /**< Path to the file */
  char* uri;          /**< URI to the file */
  zatura_t* zatura; /**< Zatura session */
} zatura_password_dialog_info_t;

struct zatura_document_information_entry_s {
  zatura_document_information_type_t type; /**< Type of the information */
  char* value;                              /**< Value */
};

/**
 * Returns the associated plugin
 *
 * @param document The document
 * @return The plugin or NULL
 */
const zatura_plugin_t* zatura_document_get_plugin(zatura_document_t* document);

/* Locks/unlocks the document while a page is parsed on first use. */
void zatura_document_lock(zatura_document_t* document);
void zatura_document_unlock(zatura_document_t* document);

/* Parses the page with its plugin on first use, taking the document lock internally. */
bool zatura_page_load(zatura_page_t* page, zatura_error_t* error);

/* Returns true once the page has been parsed by its plugin. */
bool zatura_page_is_loaded(zatura_page_t* page);

#endif // INTERNAL_H
