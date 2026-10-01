/* SPDX-License-Identifier: Zlib */

#include <gtk/gtk.h>

#include "girara-gtk/keycodes.h"

static void test_russian_layout_keeps_shortcut_position(void) {
  /* XKB/evdev keycodes for the physical A, J and G keys.  The fallback
   * keyvals are what a Russian layout reports for those same keys. */
  g_assert_cmpuint(girara_keycode_to_keyval(38, 0, GDK_KEY_Cyrillic_ef), ==, GDK_KEY_a);
  g_assert_cmpuint(girara_keycode_to_keyval(44, GDK_CONTROL_MASK, GDK_KEY_Cyrillic_o), ==, GDK_KEY_j);
  g_assert_cmpuint(girara_keycode_to_keyval(42, GDK_SHIFT_MASK, GDK_KEY_Cyrillic_PE), ==, GDK_KEY_G);
}

static void test_shifted_symbol_and_special_key(void) {
  g_assert_cmpuint(girara_keycode_to_keyval(21, GDK_SHIFT_MASK, GDK_KEY_question), ==, GDK_KEY_plus);
  g_assert_cmpuint(girara_keycode_to_keyval(23, GDK_SHIFT_MASK, GDK_KEY_ISO_Left_Tab), ==, GDK_KEY_ISO_Left_Tab);
  g_assert_cmpuint(girara_keycode_to_keyval(87, 0, GDK_KEY_KP_1), ==, GDK_KEY_KP_1);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/keycodes/russian-layout", test_russian_layout_keeps_shortcut_position);
  g_test_add_func("/keycodes/shift", test_shifted_symbol_and_special_key);
  return g_test_run();
}
