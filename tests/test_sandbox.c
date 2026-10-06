/* SPDX-License-Identifier: Zlib */

#include "zatura.h"
#include "document.h"
#ifdef WITH_SECCOMP
#include "seccomp-filters.h"
#endif
#ifdef WITH_LANDLOCK
#include "landlock.h"
#endif

#include "tests.h"
#ifdef __linux__
#include "sandbox-fds.h"
#endif

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

static void test_create(void) {
  setup_logger();

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));

#ifdef WITH_LANDLOCK
  if (landlock_drop_write() != 0) {
    g_test_skip("Landlock ABI 8 is unavailable; strict startup refuses this kernel");
    zathura_free(zathura);
    return;
  }
#endif
#ifdef WITH_SECCOMP
  g_assert_cmpint(seccomp_enable_strict_filter(zathura), ==, 0);
#endif

#ifdef __linux__
  g_assert_cmpint(sandbox_check_fds(NULL), ==, 0);
#endif

  g_assert_null(zathura_document_open(zathura, NULL, NULL, NULL, NULL));
  g_assert_null(zathura_document_open(zathura, "fl", NULL, NULL, NULL));
  g_assert_null(zathura_document_open(zathura, "fl", "ur", NULL, NULL));
  g_assert_null(zathura_document_open(zathura, "fl", NULL, "pw", NULL));

  zathura_free(zathura);
}

int main(int argc, char* argv[]) {
#ifdef __linux__
  if (sandbox_prepare_inherited_fds() != 0) return 1;
#endif
  setup_logger();

  gtk_init();
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/sandbox/session_create", test_create);
  return g_test_run();
}
