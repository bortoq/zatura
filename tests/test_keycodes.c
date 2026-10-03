/* SPDX-License-Identifier: Zlib */

#include <gtk/gtk.h>

#include "girara-gtk/keycodes.h"

static void test_alternate_layout_keeps_shortcut_position(void) {
  /* XKB/evdev keycodes for the physical A, J and G keys.  The fallback
   * keyvals are what a alternate layout reports for those same keys. */
  g_assert_cmpuint(girara_keycode_to_keyval(38, 0, GDK_KEY_Cyrillic_ef), ==, GDK_KEY_a);
  g_assert_cmpuint(girara_keycode_to_keyval(44, GDK_CONTROL_MASK, GDK_KEY_Cyrillic_o), ==, GDK_KEY_j);
  g_assert_cmpuint(girara_keycode_to_keyval(42, GDK_SHIFT_MASK, GDK_KEY_Cyrillic_PE), ==, GDK_KEY_G);
}

static void test_shifted_symbol_and_special_key(void) {
  g_assert_cmpuint(girara_keycode_to_keyval(21, GDK_SHIFT_MASK, GDK_KEY_question), ==, GDK_KEY_plus);
  g_assert_cmpuint(girara_keycode_to_keyval(23, GDK_SHIFT_MASK, GDK_KEY_ISO_Left_Tab), ==, GDK_KEY_ISO_Left_Tab);
  g_assert_cmpuint(girara_keycode_to_keyval(87, 0, GDK_KEY_KP_1), ==, GDK_KEY_KP_1);
}

static void test_configured_keys_resolve_to_positions(void) {
  guint implicit = 0;
  g_assert_cmpuint(girara_keyval_to_keycode(GDK_KEY_a, &implicit), ==, 38);
  g_assert_cmpuint(implicit, ==, 0);
  g_assert_cmpuint(girara_keyval_to_keycode(GDK_KEY_G, &implicit), ==, 42);
  g_assert_cmpuint(implicit, ==, GDK_SHIFT_MASK);
  g_assert_cmpuint(girara_keyval_to_keycode(GDK_KEY_plus, &implicit), ==, 21);
  g_assert_cmpuint(implicit, ==, GDK_SHIFT_MASK);
  g_assert_cmpuint(girara_keyval_to_keycode(GDK_KEY_ISO_Left_Tab, &implicit), ==, 23);
  g_assert_cmpuint(implicit, ==, GDK_SHIFT_MASK);
  g_assert_cmpuint(girara_keyval_to_keycode(GDK_KEY_F11, &implicit), ==, 95);
  g_assert_cmpuint(implicit, ==, 0);
  g_assert_cmpuint(girara_shortcut_keycode(87, GDK_KEY_KP_1), ==, 10);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/keycodes/alternate-layout", test_alternate_layout_keeps_shortcut_position);
  g_test_add_func("/keycodes/shift", test_shifted_symbol_and_special_key);
  g_test_add_func("/keycodes/configured-positions", test_configured_keys_resolve_to_positions);
  return g_test_run();
}
