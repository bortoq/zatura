/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_KEYCODES_H
#define GIRARA_KEYCODES_H

#include <glib.h>
#include <gdk/gdk.h>

/* Resolve a hardware keycode using fixed US key positions.  This is for
 * shortcuts only; GtkEntry continues to receive the user's layout. */
guint girara_keycode_to_keyval(guint keycode, GdkModifierType state, guint fallback);

/* Resolve a configured US key symbol to its physical key and implicit Shift.
 * Returns 0 for symbols that are not present on the reference keyboard. */
guint girara_keyval_to_keycode(guint keyval, guint* implicit_modifiers);

/* Keep numeric keypad digits compatible with main-row digit shortcuts. */
guint girara_shortcut_keycode(guint keycode, guint active_keyval);

#endif
