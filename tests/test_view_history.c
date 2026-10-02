/* SPDX-License-Identifier: Zlib */
#include <glib/gstdio.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/callbacks.h>
#include <cairo-pdf.h>
#include <sqlite3.h>
#include "zatura/zatura.h"
#include "zatura/document.h"
#include "zatura/database.h"
#include "zatura/page.h"
#include "zatura/shortcuts.h"
#include "zatura/view-settings.h"

static void settle(void) {
  const gint64 end = g_get_monotonic_time() + 500000;
  while (g_get_monotonic_time() < end) { g_main_context_iteration(NULL, FALSE); g_usleep(1000); }
}
static zathura_t* create(const char* directory) {
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, directory); zathura_set_data_dir(app, directory);
  zathura_set_cache_dir(app, directory);
  g_assert_true(zathura_init(app));
  return app;
}
static void set_int(zathura_t* app, const char* name, int value) {
  g_assert_true(girara_setting_set(app->ui.session, name, &value));
}
static void check_int(zathura_t* app, const char* name, int expected) {
  int value = 0; g_assert_true(girara_setting_get(app->ui.session, name, &value));
  g_assert_cmpint(value, ==, expected);
}
static void check_bool(zathura_t* app, const char* name, bool expected) {
  bool value = false; g_assert_true(girara_setting_get(app->ui.session, name, &value));
  g_assert_cmpint(value, ==, expected);
}
static void exercise(const char* directory, const char* document, const char* other) {
  zathura_t* app = create(directory);
  g_assert_true(document_open(app, document, NULL, NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL));
  settle();
  girara_mode_set(app->ui.session, app->modes.normal);
  /* Physical T, even when the active layout supplies a Cyrillic symbol. */
  g_assert_true(girara_process_view_key_with_code(app->ui.session, GDK_KEY_Cyrillic_ie, 28, 0));
  check_bool(app, "statusbar-show-time", true);
  g_assert_cmpuint(app->statusbar_clock_source, >, 0);
  const char* label = gtk_label_get_text(app->ui.statusbar.page_number);
  g_assert_cmpint(label[2], ==, ':'); g_assert_cmpint(label[6], ==, '[');
  g_assert_true(girara_process_view_key_with_code(app->ui.session, GDK_KEY_t, 28, 0));
  check_bool(app, "statusbar-show-time", false);
  g_assert_cmpuint(app->statusbar_clock_source, ==, 0);
  g_assert_cmpint(gtk_label_get_text(app->ui.statusbar.page_number)[0], ==, '[');
  sc_toggle_time(app->ui.session, NULL, NULL, 0);
  set_int(app, "page-brightness", -23); set_int(app, "page-contrast", 4);
  set_int(app, "page-gamma", 12); set_int(app, "page-saturation", -15);
  set_int(app, "reflow-font-size", 17);
  set_int(app, "reflow-margin-top", 2); set_int(app, "reflow-margin-bottom", 6);
  set_int(app, "reflow-margin-outer", 3); set_int(app, "reflow-margin-inner", 5);
  unsigned two = 2; bool yes = true;
  girara_setting_set(app->ui.session, "pages-per-row", &two);
  girara_setting_set(app->ui.session, "first-page-column", "1:1");
  girara_setting_set(app->ui.session, "page-right-to-left", &yes);
  girara_setting_set(app->ui.session, "recolor", &yes);
  settle();
  const unsigned pages = zathura_document_get_number_of_pages(app->document);
  page_set(app, pages / 2); settle();
  const unsigned target = zathura_document_get_current_page_number(app->document);
  zathura_document_set_adjust_mode(app->document, ZATHURA_ADJUST_WIDTH);
  zathura_free(app); /* real close and database release, then a fresh instance */
  app = create(directory);
  g_assert_true(document_open(app, document, NULL, NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL));
  if (!zathura_document_is_reflowable(app->document)) {
    g_assert_cmpint(zathura_document_get_adjust_mode(app->document), ==, ZATHURA_ADJUST_WIDTH);
  }
  settle();
  check_int(app, "page-brightness", -23); check_int(app, "page-contrast", 4);
  check_int(app, "page-gamma", 12); check_int(app, "page-saturation", -15);
  check_int(app, "reflow-font-size", 17);
  check_int(app, "reflow-margin-top", 2); check_int(app, "reflow-margin-bottom", 6);
  check_int(app, "reflow-margin-outer", 3); check_int(app, "reflow-margin-inner", 5);
  check_int(app, "pages-per-row", 2);
  check_bool(app, "recolor", true); check_bool(app, "page-right-to-left", true);
  check_bool(app, "statusbar-show-time", true);
  zathura_fileinfo_t stored = {0};
  g_assert_true(zathura_db_get_fileinfo(app->database, document, zathura_document_get_hash(app->document), &stored));
  g_assert_cmpuint(stored.current_page, ==, target);
  g_free(stored.first_page_column_list); g_free(stored.view_settings);
  /* The page counter may select either page at the centre of the same RTL spread. */
  g_assert_cmpuint(zathura_document_get_current_page_number(app->document) / 2, ==, target / 2);
  document_close(app, false);
  /* A different, unseen document starts with the configured defaults. */
  g_assert_true(document_open(app, other, NULL, NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL));
  check_int(app, "page-brightness", 0); check_int(app, "reflow-font-size", 12);
  check_int(app, "reflow-margin-inner", 4); check_bool(app, "recolor", false);
  check_bool(app, "statusbar-show-time", false);
  zathura_free(app);
}
int main(int argc, char** argv) {
  gtk_init();
  g_autofree char* directory = g_dir_make_tmp("zatura-view-history-XXXXXX", NULL);
  g_autofree char* pdf = g_build_filename(directory, "test.pdf", NULL);
  g_autofree char* other = g_build_filename(directory, "other.pdf", NULL);
  cairo_surface_t* surface = cairo_pdf_surface_create(pdf, 300, 500);
  cairo_t* cr = cairo_create(surface);
  for (int i = 0; i < 20; ++i) { cairo_show_text(cr, "History test"); cairo_show_page(cr); }
  cairo_destroy(cr); cairo_surface_destroy(surface);
  surface = cairo_pdf_surface_create(other, 400, 550); cairo_surface_destroy(surface);
  /* Simulate a v4 history database and verify its additive migration. */
  zathura_t* app = create(directory); zathura_free(app);
  g_autofree char* dbpath = g_build_filename(directory, "bookmarks.sqlite", NULL);
  sqlite3* db = NULL; g_assert_cmpint(sqlite3_open(dbpath, &db), ==, SQLITE_OK);
  g_assert_cmpint(sqlite3_exec(db, "DROP TABLE document_view; PRAGMA user_version=4;", NULL, NULL, NULL), ==, SQLITE_OK);
  sqlite3_close(db);
  exercise(directory, pdf, other);
  for (int i = 1; i < argc; ++i) {
    /* Unseen file must have a different hash on each pass. */
    surface = cairo_pdf_surface_create(other, 400 + i, 550); cairo_surface_destroy(surface);
    exercise(directory, argv[i], other);
  }
  app = create(directory);
  zatura_view_settings_restore(app, "{\"page-brightness\":\"bad\",\"statusbar-show-time\":42}");
  check_int(app, "page-brightness", 0); check_bool(app, "statusbar-show-time", false);
  zathura_free(app);
  g_print("View persistence, v4 migration, isolated defaults and clock passed\\n");
  return 0;
}
