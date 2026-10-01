/* SPDX-License-Identifier: Zlib */

#include "keycodes.h"

#include <xkbcommon/xkbcommon.h>

static struct xkb_keymap* us_keymap;
static gsize keymap_initialized;

static void init_us_keymap(void) {
  struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (context == NULL) {
    return;
  }

  const struct xkb_rule_names names = {
      .rules = "evdev", .model = "pc105", .layout = "us",
  };
  us_keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  xkb_context_unref(context);
}

guint girara_keycode_to_keyval(guint keycode, GdkModifierType state, guint fallback) {
  /* NumLock is managed by the active keymap. Preserve its numeric result;
   * the caller normalizes keypad digits to their main-key equivalents. */
  if (fallback >= GDK_KEY_KP_0 && fallback <= GDK_KEY_KP_9) {
    return fallback;
  }

  if (g_once_init_enter(&keymap_initialized)) {
    init_us_keymap();
    g_once_init_leave(&keymap_initialized, 1);
  }

  if (us_keymap == NULL || keycode < xkb_keymap_min_keycode(us_keymap) ||
      keycode > xkb_keymap_max_keycode(us_keymap)) {
    return fallback;
  }

  struct xkb_state* xkb_state = xkb_state_new(us_keymap);
  if (xkb_state == NULL) {
    return fallback;
  }

  xkb_mod_mask_t mods = 0;
  if (state & GDK_SHIFT_MASK) {
    const xkb_mod_index_t shift = xkb_keymap_mod_get_index(us_keymap, XKB_MOD_NAME_SHIFT);
    if (shift != XKB_MOD_INVALID) {
      mods |= (xkb_mod_mask_t)1 << shift;
    }
  }
  xkb_state_update_mask(xkb_state, mods, 0, 0, 0, 0, 0);
  const xkb_keysym_t sym = xkb_state_key_get_one_sym(xkb_state, keycode);
  xkb_state_unref(xkb_state);

  return sym == XKB_KEY_NoSymbol ? fallback : sym;
}

guint girara_keyval_to_keycode(guint keyval, guint* implicit_modifiers) {
  if (implicit_modifiers != NULL) {
    *implicit_modifiers = 0;
  }
  if (keyval == 0) {
    return 0;
  }

  if (g_once_init_enter(&keymap_initialized)) {
    init_us_keymap();
    g_once_init_leave(&keymap_initialized, 1);
  }
  if (us_keymap == NULL) {
    return 0;
  }

  for (unsigned int shifted = 0; shifted <= 1; ++shifted) {
    struct xkb_state* state = xkb_state_new(us_keymap);
    if (state == NULL) {
      return 0;
    }
    if (shifted != 0) {
      const xkb_mod_index_t shift = xkb_keymap_mod_get_index(us_keymap, XKB_MOD_NAME_SHIFT);
      if (shift != XKB_MOD_INVALID) {
        xkb_state_update_mask(state, (xkb_mod_mask_t)1 << shift, 0, 0, 0, 0, 0);
      }
    }

    for (xkb_keycode_t code = xkb_keymap_min_keycode(us_keymap); code <= xkb_keymap_max_keycode(us_keymap); ++code) {
      if (xkb_state_key_get_one_sym(state, code) == keyval) {
        xkb_state_unref(state);
        if (implicit_modifiers != NULL && shifted != 0) {
          *implicit_modifiers = GDK_SHIFT_MASK;
        }
        return code;
      }
    }
    xkb_state_unref(state);
  }

  return 0;
}

guint girara_shortcut_keycode(guint keycode, guint active_keyval) {
  if (active_keyval >= GDK_KEY_KP_0 && active_keyval <= GDK_KEY_KP_9) {
    const guint main_digit = GDK_KEY_0 + active_keyval - GDK_KEY_KP_0;
    const guint main_code = girara_keyval_to_keycode(main_digit, NULL);
    if (main_code != 0) {
      return main_code;
    }
  }
  return keycode;
}
