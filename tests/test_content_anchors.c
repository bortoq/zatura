/* SPDX-License-Identifier: Zlib */
#include <glib/gstdio.h>
#include <girara-gtk/settings.h>
#include <sqlite3.h>
#include "zatura/zatura.h"
#include "zatura/document.h"
#include "zatura/plugin.h"
#include "zatura/bookmarks.h"
#include "zatura/jumplist.h"
#include "zatura/database.h"
#include "zatura/commands.h"
#include "zatura/page.h"
#include "zatura/marks.h"

static void settle(void) {
  const gint64 end = g_get_monotonic_time() + 500000;
  while (g_get_monotonic_time() < end) { g_main_context_iteration(NULL, FALSE); g_usleep(1000); }
}
static zathura_t* create(const char* directory) {
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, directory); zathura_set_data_dir(app, directory); zathura_set_cache_dir(app, directory);
  g_assert_true(zathura_init(app));
  return app;
}
static void quickmark_open(zathura_t* app) {
  g_assert_true(sc_mark_evaluate(app->ui.session, NULL, NULL, 0));
  GListModel* controllers = gtk_widget_observe_controllers(app->ui.session->gtk.window);
  bool found = false;
  for (guint i = 0; i < g_list_model_get_n_items(controllers); ++i) {
    GObject* controller = g_list_model_get_item(controllers, i);
    if (g_object_get_data(controller, "evaluate")) {
      gboolean handled = FALSE;
      g_signal_emit_by_name(controller, "key-pressed", GDK_KEY_a, 38, 0, &handled);
      g_assert_true(handled); found = true;
    }
    g_object_unref(controller);
    if (found) { break; }
  }
  g_object_unref(controllers);
  g_assert_true(found); settle();
}
int main(int argc, char** argv) {
  gtk_init();
  g_autofree char* directory = g_dir_make_tmp("zatura-anchors-XXXXXX", NULL);
  /* Upgrade the intermediate v6 schema, preserving existing bookmark anchors. */
  zathura_t* initial = create(directory);
  if (!zathura_plugin_manager_get_plugin(initial->plugins.manager, "application/x-fictionbook+xml")) {
    zathura_free(initial); g_print("Reflow engine not installed; skipping engine-dependent test.\n"); return 77;
  }
  zathura_free(initial);
  g_autofree char* dbpath = g_build_filename(directory, "bookmarks.sqlite", NULL);
  sqlite3* db = NULL; g_assert_cmpint(sqlite3_open(dbpath, &db), ==, SQLITE_OK);
  g_assert_cmpint(sqlite3_exec(db, "ALTER TABLE quickmarks DROP COLUMN anchor; PRAGMA user_version=6;", NULL, NULL, NULL), ==, SQLITE_OK);
  sqlite3_close(db);
  for (int i = 1; i < argc; ++i) {
    zathura_t* app = create(directory);
    g_assert_true(document_open(app, argv[i], NULL, NULL, 0, NULL)); settle();
    const unsigned int old_page = zathura_document_get_number_of_pages(app->document) / 3;
    page_set(app, old_page); settle();
    zathura_bookmark_t* bookmark = zathura_bookmark_add(app, "content", old_page + 1);
    g_assert_nonnull(bookmark); g_assert_cmpint(bookmark->anchor[0], !=, 0);
    char anchor[128]; g_strlcpy(anchor, bookmark->anchor, sizeof(anchor));
    girara_list_t* mark_args = girara_list_new(); girara_list_append(mark_args, "a");
    g_assert_true(cmd_marks_add(app->ui.session, mark_args)); girara_list_free(mark_args);
    zathura_mark_t* mark = girara_list_nth(app->global.marks, 0);
    g_assert_nonnull(mark); g_assert_cmpstr(mark->anchor, ==, anchor);
    zathura_jumplist_add(app);
    zathura_jump_t* jump = zathura_jumplist_current(app);
    g_assert_nonnull(jump); g_assert_cmpstr(jump->anchor, ==, anchor);
    const int font = 25;
    girara_setting_set(app->ui.session, "reflow-font-size", &font); settle();
    unsigned int mapped = 0;
    g_assert_true(zathura_document_resolve_anchor(app->document, anchor, &mapped));
    g_assert_cmpuint(mapped, >, old_page);
    g_assert_cmpuint(bookmark->page, ==, mapped + 1);
    g_assert_cmpuint(jump->page, ==, mapped);
    g_assert_cmpuint(mark->page, ==, mapped);
    const unsigned two = 2; const int margin = 17;
    girara_setting_set(app->ui.session, "pages-per-row", &two);
    girara_setting_set(app->ui.session, "reflow-margin-inner", &margin); settle();
    g_assert_true(zathura_document_resolve_anchor(app->document, anchor, &mapped));
    g_assert_cmpuint(bookmark->page, ==, mapped + 1);
    g_assert_cmpuint(mark->page, ==, mapped);
    g_assert_false(zathura_document_resolve_anchor(app->document, "mupdf-text-v1:bad:0", &mapped));
    zathura_free(app);
    app = create(directory);
    g_assert_true(document_open(app, argv[i], NULL, NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL)); settle();
    bookmark = zathura_bookmark_get(app, "content");
    g_assert_nonnull(bookmark); g_assert_cmpstr(bookmark->anchor, ==, anchor);
    g_assert_true(zathura_document_resolve_anchor(app->document, bookmark->anchor, &mapped));
    mark = girara_list_nth(app->global.marks, 0);
    g_assert_nonnull(mark); g_assert_cmpstr(mark->anchor, ==, anchor);
    page_set(app, 0); settle(); quickmark_open(app);
    g_assert_cmpuint(zathura_document_get_current_page_number(app->document) / 2, ==, mapped / 2);
    bool found = false;
    for (size_t n = 0; n < girara_list_size(app->jumplist.list); ++n) {
      jump = girara_list_nth(app->jumplist.list, n);
      if (g_strcmp0(jump->anchor, anchor) == 0) { found = true; }
    }
    g_assert_true(found);
    girara_list_t* args = girara_list_new(); girara_list_append(args, "content");
    g_assert_true(cmd_bookmark_open(app->ui.session, args)); girara_list_free(args); settle();
    g_assert_cmpuint(zathura_document_get_current_page_number(app->document) / 2, ==, mapped / 2);
    /* Migrate a legacy numeric bookmark lazily before the next reflow. */
    zathura_bookmark_t* legacy = zathura_bookmark_add(app, "legacy", 1);
    legacy->anchor[0] = '\0';
    mark->anchor[0] = '\0';
    zathura_db_add_bookmark(app->database, argv[i], legacy);
    const int changed_font = 19;
    girara_setting_set(app->ui.session, "reflow-font-size", &changed_font); settle();
    g_assert_cmpint(legacy->anchor[0], !=, 0);
    g_assert_cmpint(mark->anchor[0], !=, 0);
    g_print("Persistent bookmarks/quickmarks/jumps, v6 migration and legacy capture: %s\n", argv[i]);
    zathura_free(app);
  }
  return 0;
}
