/* SPDX-License-Identifier: Zlib */

#include <girara/log.h>
#include <girara-gtk/session.h>

#include "zatura.h"

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

  zatura_t* zatura = zatura_create();
  g_assert_nonnull(zatura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zatura_set_config_dir(zatura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zatura_init(zatura));
  zatura_free(zatura);
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
