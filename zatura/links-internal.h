/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_LINKS_INTERNAL_H
#define ZATURA_LINKS_INTERNAL_H

#include "links.h"

#include <gtk/gtk.h>

/**
 * Evaluate link
 *
 * @param zatura Zatura instance
 * @param link The link
 */
void zatura_link_evaluate(zatura_t* zatura, zatura_link_t* link);

/**
 * Display a link using girara_notify
 *
 * @param zatura Zatura instance
 * @param link The link
 */
void zatura_link_display(zatura_t* zatura, zatura_link_t* link);

/**
 * Copy a link into the clipboard using and display it using girara_notify
 *
 * @param zatura Zatura instance
 * @param link The link
 * @param selection target clipboard
 */
void zatura_link_copy(zatura_t* zatura, zatura_link_t* link, GdkClipboard* selection);

#endif