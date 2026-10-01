/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_KEYCODES_H
#define GIRARA_KEYCODES_H

#include <glib.h>
#include <gdk/gdk.h>

/* Resolve a hardware keycode using fixed US key positions.  This is for
 * shortcuts only; GtkEntry continues to receive the user's layout. */
guint girara_keycode_to_keyval(guint keycode, GdkModifierType state, guint fallback);

#endif
