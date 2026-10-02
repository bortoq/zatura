/* SPDX-License-Identifier: Zlib */

#include <girara/log.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/callbacks.h>

#include "zatura.h"
#include "shortcuts.h"
#include "render.h"

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

static void test_page_effect_bindings(zathura_t* zathura);

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
  test_page_effect_bindings(zathura);
  zathura_free(zathura);
}

static void test_page_effect_bindings(zathura_t* zathura) {
  girara_session_t* session = zathura->ui.session;
  ZathuraRenderer* renderer = zathura_renderer_new(2);
  zathura->sync.render_thread = renderer;
  const char* names[] = {"page-contrast", "page-brightness", "page-gamma", "page-saturation"};
  const girara_mode_t modes[] = {zathura->modes.normal, zathura->modes.presentation};
  for (unsigned m = 0; m < G_N_ELEMENTS(modes); ++m) {
    girara_mode_set(session, modes[m]);
    for (unsigned i = 0; i < G_N_ELEMENTS(names); ++i) {
      /* Use a different layout symbol; matching must use the physical number-row code. */
      g_assert_true(girara_process_view_key_with_code(session, GDK_KEY_ampersand, 10 + 2 * i, 0));
      int value = 0;
      girara_setting_get(session, names[i], &value);
      g_assert_cmpint(value, ==, -1);
      g_assert_true(girara_process_view_key_with_code(session, GDK_KEY_eacute, 11 + 2 * i, 0));
      girara_setting_get(session, names[i], &value);
      g_assert_cmpint(value, ==, 0);
    }
  }
  const int high = 120;
  girara_setting_set(session, "page-gamma", &high);
  int gamma = 0;
  girara_setting_get(session, "page-gamma", &gamma);
  g_assert_cmpint(gamma, ==, 100);
  PageEffects effects = zathura_renderer_get_page_effects(renderer);
  g_assert_cmpint(effects.gamma, ==, 100);
  g_assert_false(zathura_renderer_set_page_effects(renderer, &effects));
  g_assert_true(sc_reset_page_effects(session, NULL, NULL, 0));
  effects = zathura_renderer_get_page_effects(renderer);
  g_assert_cmpint(effects.gamma, ==, 0);
  /* Inputbar digits must remain text input, with no display adjustment binding. */
  girara_mode_set(session, session->modes.inputbar);
  g_assert_false(girara_process_view_key_with_code(session, GDK_KEY_6, 15, 0));
  effects = zathura_renderer_get_page_effects(renderer);
  g_assert_cmpint(effects.gamma, ==, 0);
  zathura->sync.render_thread = NULL;
  g_object_unref(renderer);
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
