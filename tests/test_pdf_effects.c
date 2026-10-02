/* SPDX-License-Identifier: Zlib */
#include <stdint.h>
#include <stdatomic.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include "zatura/internal.h"
#include "zatura/utils.h"
#include "zatura/page-widget.h"
#include <cairo-pdf.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include "zatura/zatura.h"
#include "zatura/render.h"
#include "zatura/page.h"
#include "zatura/document.h"
#include "zatura/shortcuts.h"

static uint32_t middle(cairo_surface_t* surface) {
  cairo_surface_flush(surface);
  const int x = cairo_image_surface_get_width(surface) / 2;
  const int y = cairo_image_surface_get_height(surface) / 2;
  const unsigned char* data = cairo_image_surface_get_data(surface);
  return ((const uint32_t*)(data + y * cairo_image_surface_get_stride(surface)))[x] & 0xffffff;
}

static atomic_uint plugin_renders;
static zathura_plugin_page_render_cairo_t original_render;
static zathura_error_t counted_render(zathura_page_t* page, void* data, cairo_t* cairo, bool printing) {
  ++plugin_renders;
  return original_render(page, data, cairo, printing);
}

static unsigned delivered;
static uint32_t delivered_pixel;
static void completed(ZathuraRenderRequest* request, cairo_surface_t* surface, void* data) {
  (void)request; (void)data;
  ++delivered;
  delivered_pixel = middle(surface);
}

static void wait_for_result(void) {
  const gint64 deadline = g_get_monotonic_time() + 3000000;
  while (!delivered && g_get_monotonic_time() < deadline) {
    while (g_main_context_iteration(NULL, FALSE)) {}
    g_usleep(1000);
  }
  g_assert_cmpuint(delivered, >=, 1);
}

int main(void) {
  gtk_init();
  char* fixture = NULL;
  int fd = g_file_open_tmp("zatura-effects-XXXXXX.pdf", &fixture, NULL);
  g_assert_cmpint(fd, >=, 0);
  close(fd);
  cairo_surface_t* pdf = cairo_pdf_surface_create(fixture, 32, 32);
  cairo_t* cr = cairo_create(pdf);
  cairo_set_source_rgb(cr, 0.25, 0.5, 0.75);
  cairo_paint(cr);
  cairo_destroy(cr);
  cairo_surface_destroy(pdf);
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(app));
  if (!document_open(app, fixture, NULL, NULL, 0, NULL)) {
    g_print("PDF plugin unavailable; skipping integration check.\n");
    zathura_free(app);
    g_unlink(fixture);
    g_free(fixture);
    return 77;
  }
  zathura_page_t* page = zathura_document_get_page(zathura_get_document(app), 0);
  ZathuraRenderer* renderer = app->sync.render_thread;
  zathura_plugin_functions_t* functions = (zathura_plugin_functions_t*)
      zathura_plugin_get_functions(zathura_document_get_plugin(zathura_get_document(app)));
  original_render = functions->page_render_cairo;
  functions->page_render_cairo = counted_render;
  /* Make a substantial page surface so the filter and copy paths are exercised. */
  zathura_document_set_adjust_mode(zathura_get_document(app), ZATHURA_ADJUST_NONE);
  zathura_document_set_zoom(zathura_get_document(app), 30);
  const gint64 cold_start = g_get_monotonic_time();
  cairo_surface_t* surface = zathura_renderer_render_page(renderer, page);
  const gint64 cold_us = g_get_monotonic_time() - cold_start;
  const uint32_t original = middle(surface);
  const int initial_width = cairo_image_surface_get_width(surface);
  g_assert_cmpuint(plugin_renders, ==, 1);
  GtkWidget* widget = zathura_page_get_widget(app, page);
  zathura_page_widget_update_surface(ZATHURA_PAGE_WIDGET(widget), surface, false);
  int widget_width = 0, widget_height = 0;
  gtk_widget_get_size_request(widget, &widget_width, &widget_height);
  const gint64 adjustment_start = g_get_monotonic_time();
  cairo_surface_destroy(surface);
  int value = -100;
  girara_setting_set(app->ui.session, "page-saturation", &value);
  surface = zathura_renderer_render_page(renderer, page);
  uint32_t pixel = middle(surface);
  g_assert_cmpuint((pixel >> 16) & 255, ==, pixel & 255);
  g_assert_cmpuint((pixel >> 8) & 255, ==, pixel & 255);
  cairo_surface_destroy(surface);
  g_assert_cmpuint(plugin_renders, ==, 1);
  g_assert_true(zathura_page_widget_have_surface(ZATHURA_PAGE_WIDGET(widget)));
  int retained_width = 0, retained_height = 0;
  gtk_widget_get_size_request(widget, &retained_width, &retained_height);
  g_assert_cmpint(retained_width, ==, widget_width);
  g_assert_cmpint(retained_height, ==, widget_height);
  sc_reset_page_effects(app->ui.session, NULL, NULL, 0);
  value = 50;
  girara_setting_set(app->ui.session, "page-gamma", &value);
  surface = zathura_renderer_render_page(renderer, page);
  pixel = middle(surface);
  for (unsigned shift = 0; shift <= 16; shift += 8) {
    g_assert_cmpuint((pixel >> shift) & 255, >, (original >> shift) & 255);
  }
  cairo_surface_destroy(surface);
  value = -100;
  girara_setting_set(app->ui.session, "page-brightness", &value);
  surface = zathura_renderer_render_page(renderer, page);
  g_assert_cmphex(middle(surface), ==, 0);
  cairo_surface_destroy(surface);
  const gint64 adjustments_us = g_get_monotonic_time() - adjustment_start;
  g_assert_cmpuint(plugin_renders, ==, 1);
  /* A changed zoom must miss the raw cache; returning to it must hit. */
  zathura_document_set_zoom(zathura_get_document(app), 31);
  surface = zathura_renderer_render_page(renderer, page);
  g_assert_cmpint(cairo_image_surface_get_width(surface), >, initial_width);
  cairo_surface_destroy(surface);
  g_assert_cmpuint(plugin_renders, ==, 2);
  surface = zathura_renderer_render_page(renderer, page);
  cairo_surface_destroy(surface);
  g_assert_cmpuint(plugin_renders, ==, 2);
  bool recolor = true;
  girara_setting_set(app->ui.session, "recolor", &recolor);
  value = 100;
  girara_setting_set(app->ui.session, "page-brightness", &value);
  surface = zathura_renderer_render_page(renderer, page);
  g_assert_cmphex(middle(surface), ==, 0xffffff);
  cairo_surface_destroy(surface);

  ZathuraRenderRequest* request = zathura_render_request_new(renderer, page);
  zathura_render_request_set_render_plain(request, true);
  g_signal_connect(request, "completed", G_CALLBACK(completed), NULL);
  zathura_render_request(request, g_get_real_time());
  wait_for_result();
  g_assert_cmphex(delivered_pixel, ==, original);
  g_assert_cmpuint(plugin_renders, >=, 3);
  g_object_unref(request);

  const unsigned renders_before_repeats = plugin_renders;
  delivered = 0;
  request = zathura_render_request_new(renderer, page);
  g_signal_connect(request, "completed", G_CALLBACK(completed), NULL);
  /* Main context stays owned while old jobs render: completion must be queued. */
  g_assert_true(g_main_context_acquire(NULL));
  for (unsigned i = 0; i < 20; ++i) {
    value = i % 2 == 0 ? -100 : 100;
    girara_setting_set(app->ui.session, "page-brightness", &value);
    zathura_render_request(request, g_get_real_time());
    g_usleep(1000);
  }
  g_main_context_release(NULL);
  const gint64 latest_deadline = g_get_monotonic_time() + 3000000;
  while ((!delivered || delivered_pixel != 0xffffff) && g_get_monotonic_time() < latest_deadline) {
    g_main_context_iteration(NULL, FALSE);
    g_usleep(1000);
  }
  g_assert_cmpuint(delivered, >=, 1);
  g_assert_cmphex(delivered_pixel, ==, 0xffffff);
  /* Keep changing settings faster than filtering a large frame. At least two
   * completed frames must be presented before key repeat stops. */
  zathura_document_set_zoom(zathura_get_document(app), 60);
  surface = zathura_renderer_render_page(renderer, page);
  cairo_surface_destroy(surface);
  delivered = 0;
  const gint64 repeat_deadline = g_get_monotonic_time() + 2000000;
  unsigned repeats = 0;
  while (g_get_monotonic_time() < repeat_deadline) {
    value = (repeats++ % 2) ? -20 : 20;
    girara_setting_set(app->ui.session, "page-brightness", &value);
    zathura_render_request(request, g_get_real_time());
    for (unsigned tick = 0; tick < 8; ++tick) {
      g_main_context_iteration(NULL, FALSE);
    }
    g_usleep(1000);
  }
  g_assert_cmpuint(delivered, >=, 2);
  g_print("During continuous adjustment: %u intermediate frames for %u setting changes\n", delivered, repeats);
  const gint64 settle_deadline = g_get_monotonic_time() + 3000000;
  const unsigned last_generation = zathura_renderer_get_effects_generation(renderer);
  while (zathura_render_request_get_completed_effects_generation(request) != last_generation &&
         g_get_monotonic_time() < settle_deadline) {
    g_main_context_iteration(NULL, FALSE);
    g_usleep(1000);
  }
  g_assert_cmpuint(zathura_render_request_get_completed_effects_generation(request), ==, last_generation);
  g_object_unref(request);
  g_assert_cmpuint(plugin_renders, ==, renders_before_repeats + 1);
  sc_reset_page_effects(app->ui.session, NULL, NULL, 0);
  functions->page_render_cairo = original_render;
  zathura_free(app);
  g_unlink(fixture);
  g_free(fixture);
  g_print("Cold page: %.2f ms; three cached adjustments: %.2f ms; width: %d pixels\n",
          cold_us / 1000.0, adjustments_us / 1000.0, initial_width);
  g_print("Live PDF: sync adjustments, recolor, plain rendering, and stale completion checks passed.\n");
  return 0;
}
