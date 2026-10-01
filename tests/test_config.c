/* SPDX-License-Identifier: Zlib */

#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/shortcuts.h>
#include <girara-gtk/config.h>
#include <girara-gtk/callbacks.h>
#include <girara-gtk/inputbar.h>
#include <girara-gtk/internal.h>
#include <girara/macros.h>

#include "tests.h"

static bool test_shortcut_func(girara_session_t* GIRARA_UNUSED(session), girara_argument_t* GIRARA_UNUSED(argument),
                               girara_event_t* GIRARA_UNUSED(event), unsigned int GIRARA_UNUSED(t)) {
  return true;
}

static unsigned int physical_shortcut_hits;

static bool count_shortcut(girara_session_t* GIRARA_UNUSED(session), girara_argument_t* GIRARA_UNUSED(argument),
                           girara_event_t* GIRARA_UNUSED(event), unsigned int GIRARA_UNUSED(t)) {
  physical_shortcut_hits++;
  return true;
}

static bool count_ten(girara_session_t* GIRARA_UNUSED(session), girara_argument_t* GIRARA_UNUSED(argument),
                      girara_event_t* GIRARA_UNUSED(event), unsigned int GIRARA_UNUSED(t)) {
  physical_shortcut_hits += 10;
  return true;
}

static void test_physical_shortcuts(void) {
  setup_logger();
  physical_shortcut_hits = 0;

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  g_assert_true(girara_shortcut_mapping_add(session, "count", count_shortcut));

  char* filename = NULL;
  int fd = g_file_open_tmp(NULL, &filename, NULL);
  g_assert_cmpint(fd, !=, -1);
  g_assert_true(g_file_set_contents(filename,
                                    "map a count\nmap G count\nmap <C-j> count\nmap gg count\n", -1, NULL));
  g_assert_true(girara_config_parse(session, filename));

  const struct {
    guint keycode;
    guint active_layout_keyval;
    GdkModifierType state;
  } keys[] = {
      {38, GDK_KEY_Cyrillic_ef, 0},
      {42, GDK_KEY_Cyrillic_PE, GDK_SHIFT_MASK},
      {44, GDK_KEY_Cyrillic_o, GDK_CONTROL_MASK},
      {42, GDK_KEY_Cyrillic_pe, 0},
      {42, GDK_KEY_Cyrillic_pe, 0},
  };

  for (size_t i = 0; i < G_N_ELEMENTS(keys); ++i) {
    guint keyval = keys[i].active_layout_keyval;
    guint clean = 0;
    g_assert_true(girara_clean_key_mask(NULL, keys[i].keycode, keys[i].state, &clean, &keyval));
    const gboolean handled = girara_process_view_key_with_code(session, keyval, keys[i].keycode, clean);
    g_assert_cmpint(handled, ==, i != 3);
  }
  g_assert_cmpuint(physical_shortcut_hits, ==, 4);

  g_assert_true(girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_u, count_shortcut, 0, NULL));
  g_assert_true(girara_process_inputbar_key_with_code(session, GDK_KEY_Cyrillic_ghe, 30, GDK_CONTROL_MASK));
  g_assert_cmpuint(physical_shortcut_hits, ==, 5);

  g_assert_true(girara_shortcut_add(session, 0, GDK_KEY_plus, NULL, count_shortcut, session->modes.normal, 0, NULL));
  g_assert_true(girara_shortcut_add(session, GDK_SHIFT_MASK, GDK_KEY_equal, NULL, count_ten,
                                    session->modes.normal, 0, NULL));
  g_assert_true(girara_process_view_key_with_code(session, GDK_KEY_plus, 21, GDK_SHIFT_MASK));
  g_assert_cmpuint(physical_shortcut_hits, ==, 15);
  g_assert_true(girara_shortcut_remove(session, 0, GDK_KEY_plus, NULL, session->modes.normal));
  g_assert_false(girara_process_view_key_with_code(session, GDK_KEY_a, 39, 0));
  g_assert_cmpuint(physical_shortcut_hits, ==, 15);

  close(fd);
  g_remove(filename);
  g_free(filename);
  girara_session_destroy(session);
}

static void test_config_parse_modifier_keys(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);

  /* Register a test shortcut function mapping */
  g_assert_true(girara_shortcut_mapping_add(session, "testfunc", test_shortcut_func));

  char* filename = NULL;
  int fd         = g_file_open_tmp(NULL, &filename, NULL);
  g_assert_cmpint(fd, !=, -1);
  g_assert_nonnull(filename);

  /* Test various modifier key combinations that previously failed */
  if (g_file_set_contents(filename,
                          "map <C-d> testfunc\n"  /* Control modifier */
                          "map <A-f> testfunc\n"  /* Alt modifier */
                          "map <S-j> testfunc\n"  /* Shift modifier */
                          "map <M-k> testfunc\n", /* Meta (Alt) modifier */
                          -1, NULL) == FALSE) {
    g_assert_not_reached();
  }
  g_assert_true(girara_config_parse(session, filename));

  close(fd);
  g_remove(filename);
  g_free(filename);
  girara_session_destroy(session);
}

static void test_fullscreen_binding_alias(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  g_assert_true(girara_shortcut_mapping_add(session, "testfunc", test_shortcut_func));

  char* filename = NULL;
  int fd = g_file_open_tmp(NULL, &filename, NULL);
  g_assert_cmpint(fd, !=, -1);
  g_assert_true(g_file_set_contents(filename, "map [fullscreen] <F11> testfunc\n", -1, NULL));
  g_assert_true(girara_config_parse(session, filename));
  g_assert_true(girara_shortcut_remove(session, 0, GDK_KEY_F11, NULL, session->modes.normal));

  close(fd);
  g_remove(filename);
  g_free(filename);
  girara_session_destroy(session);
}

static void test_config_parse(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);

  int default_val = 1;
  g_assert_true(girara_setting_add(session, "test1", "default-string", STRING, false, NULL, NULL, NULL));
  g_assert_true(girara_setting_add(session, "test2", &default_val, INT, false, NULL, NULL, NULL));

  char* filename = NULL;
  int fd         = g_file_open_tmp(NULL, &filename, NULL);
  g_assert_cmpint(fd, !=, -1);
  g_assert_nonnull(filename);
  if (g_file_set_contents(filename,
                          "set test1 config-string\n"
                          "set test2 2\n",
                          -1, NULL) == FALSE) {
    g_assert_not_reached();
  }
  g_assert_true(girara_config_parse(session, filename));

  char* ptr = NULL;
  g_assert_true(girara_setting_get(session, "test1", &ptr));
  g_assert_cmpstr(ptr, ==, "config-string");
  g_free(ptr);

  int real_val = 0;
  g_assert_true(girara_setting_get(session, "test2", &real_val));
  g_assert_cmpint(real_val, ==, 2);

  close(fd);
  g_remove(filename);
  g_free(filename);
  girara_session_destroy(session);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init();
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/config/parse", test_config_parse);
  g_test_add_func("/config/parse_modifier_keys", test_config_parse_modifier_keys);
  g_test_add_func("/config/fullscreen_binding_alias", test_fullscreen_binding_alias);
  g_test_add_func("/config/physical_shortcuts", test_physical_shortcuts);
  return g_test_run();
}
