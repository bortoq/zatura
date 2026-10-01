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

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

static void test_create(void) {
  setup_logger();

#ifdef GDK_WINDOWING_X11
  GdkDisplay* display = gdk_display_get_default();

  if (GDK_IS_X11_DISPLAY(display)) {
    g_test_skip("not running under X11");
    return;
  }
#endif

  zatura_t* zatura = zatura_create();
  g_assert_nonnull(zatura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zatura_set_config_dir(zatura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zatura_init(zatura));

#ifdef WITH_LANDLOCK
  landlock_drop_write();
#endif
#ifdef WITH_SECCOMP
  g_assert_cmpint(seccomp_enable_strict_filter(zatura), ==, 0);
#endif

  g_assert_null(zatura_document_open(zatura, NULL, NULL, NULL, NULL));
  g_assert_null(zatura_document_open(zatura, "fl", NULL, NULL, NULL));
  g_assert_null(zatura_document_open(zatura, "fl", "ur", NULL, NULL));
  g_assert_null(zatura_document_open(zatura, "fl", NULL, "pw", NULL));

  zatura_free(zatura);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init();
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/sandbox/session_create", test_create);
  return g_test_run();
}
