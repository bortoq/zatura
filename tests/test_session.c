/* SPDX-License-Identifier: Zlib */

#include <girara/log.h>
#include <girara-gtk/session.h>

#include "zatura.h"
#include "shortcuts.h"

#include "tests.h"

static void test_girara_create(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  girara_session_destroy(session);
}

static void test_girara_init(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  g_assert_true(girara_session_init(session, "test"));
  girara_session_destroy(session);
}

static void test_create(void) {
  setup_logger();
  girara_set_log_level(GIRARA_ERROR);

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));
  g_assert_cmpuint(girara_list_size(zathura->ui.session->modes.identifiers), ==, 5);
  g_assert_cmpint(girara_mode_get(zathura->ui.session), ==, zathura->modes.normal);
  sc_toggle_fullscreen(zathura->ui.session, NULL, NULL, 0);
  g_assert_cmpint(girara_mode_get(zathura->ui.session), ==, zathura->modes.normal);
  girara_mode_set(zathura->ui.session, zathura->modes.index);
  sc_toggle_fullscreen(zathura->ui.session, NULL, NULL, 0);
  g_assert_cmpint(girara_mode_get(zathura->ui.session), ==, zathura->modes.index);
  girara_mode_set(zathura->ui.session, zathura->modes.insert);
  sc_toggle_fullscreen(zathura->ui.session, NULL, NULL, 0);
  g_assert_cmpint(girara_mode_get(zathura->ui.session), ==, zathura->modes.insert);
  girara_mode_set(zathura->ui.session, zathura->modes.presentation);
  sc_toggle_fullscreen(zathura->ui.session, NULL, NULL, 0);
  g_assert_cmpint(girara_mode_get(zathura->ui.session), ==, zathura->modes.presentation);
  girara_mode_set(zathura->ui.session, zathura->ui.session->modes.inputbar);
  sc_toggle_fullscreen(zathura->ui.session, NULL, NULL, 0);
  g_assert_cmpint(girara_mode_get(zathura->ui.session), ==, zathura->ui.session->modes.inputbar);
  zathura_free(zathura);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init();
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/girara/session/create", test_girara_create);
  g_test_add_func("/girara/session/init", test_girara_init);
  g_test_add_func("/session/create", test_create);
  return g_test_run();
}
