/* SPDX-License-Identifier: Zlib */
#include <glib/gstdio.h>
#include <girara-gtk/settings.h>
#include <cairo-pdf.h>
#include "zatura/zatura.h"
#include "zatura/document.h"
#include "zatura/document-widget.h"
#include "zatura/page.h"
#include "zatura/page-widget.h"
#include "zatura/render.h"

static void settle(void) {
  const gint64 end = g_get_monotonic_time() + 300000;
  while (g_get_monotonic_time() < end) { g_main_context_iteration(NULL, FALSE); g_usleep(1000); }
}
int main(void) {
  gtk_init();
  g_autofree char* dir = g_dir_make_tmp("zatura-cache-XXXXXX", NULL);
  g_autofree char* file = g_build_filename(dir, "pages.pdf", NULL);
  cairo_surface_t* pdf = cairo_pdf_surface_create(file, 100, 100);
  cairo_t* context = cairo_create(pdf);
  for (unsigned int i = 0; i < 3; ++i) { cairo_show_page(context); }
  cairo_destroy(context); cairo_surface_destroy(pdf);
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, dir); zathura_set_data_dir(app, dir); zathura_set_cache_dir(app, dir);
  g_assert_true(zathura_init(app)); g_assert_true(document_open(app, file, NULL, NULL, 0, NULL)); settle();
  zathura_document_widget_stop_page_widget_preload(app->ui.document_widget);
  ZathuraPageWidget* visible = ZATHURA_PAGE_WIDGET(zathura_document_widget_ensure_page(app->ui.document_widget, 0));
  ZathuraPageWidget* hidden = ZATHURA_PAGE_WIDGET(zathura_document_widget_ensure_page(app->ui.document_widget, 2));
  zathura_page_set_visibility(zathura_document_get_page(app->document, 0), true);
  zathura_page_set_visibility(zathura_document_get_page(app->document, 2), false);
  cairo_surface_t* large = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1024, 1024);
  cairo_t* cr = cairo_create(large); cairo_set_source_rgb(cr, 0.5, 0.5, 0.5); cairo_paint(cr); cairo_destroy(cr);
  zathura_renderer_set_cache_limit(app->sync.render_thread, 8);
  zathura_page_widget_update_surface(visible, large, false);
  zathura_page_widget_update_surface(hidden, large, false);
  cairo_surface_destroy(large); settle();
  size_t raw, display, total;
  zathura_renderer_get_cache_usage(app->sync.render_thread, &raw, &display, &total);
  /* Both widgets and their thumbnails alias the same pixels: account once. */
  g_assert_cmpuint(total, <=, 8 * 1024 * 1024);
  g_assert_cmpuint(display, >=, 4 * 1024 * 1024);
  /* Add a distinct offscreen buffer; it must be evicted before the visible one. */
  large = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1024, 1024);
  cr = cairo_create(large); cairo_paint(cr); cairo_destroy(cr);
  zathura_page_widget_update_surface(hidden, large, false); cairo_surface_destroy(large); settle();
  g_assert_true(zathura_page_widget_have_surface(visible));
  g_assert_false(zathura_page_widget_have_surface(hidden));
  zathura_renderer_get_cache_usage(app->sync.render_thread, &raw, &display, &total);
  g_assert_cmpuint(total, <=, 8 * 1024 * 1024);
  zathura_renderer_set_cache_limit(app->sync.render_thread, 1); settle();
  g_assert_true(zathura_page_widget_have_surface(visible)); /* A visible frame is pinned over the budget. */
  gtk_widget_set_visible(app->ui.session->gtk.window, false);
  zathura_page_set_visibility(zathura_document_get_page(app->document, 0), false);
  zathura_page_widget_update_view_time(visible); settle();
  g_assert_false(zathura_page_widget_have_surface(visible));
  zathura_renderer_get_cache_usage(app->sync.render_thread, &raw, &display, &total);
  g_assert_cmpuint(total, <=, 1024 * 1024);
  zathura_free(app);
  g_print("Shared pixel accounting, hidden-page eviction and visible pinning passed.\n");
  return 0;
}
