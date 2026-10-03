/* SPDX-License-Identifier: Zlib */
/* Deterministic scanned-page workload; invoke each case in a fresh process. */
#include <stdlib.h>
#include <sys/resource.h>
#include <glib/gstdio.h>
#include <girara-gtk/settings.h>
#include <cairo-pdf.h>
#include "zatura/zatura.h"
#include "zatura/document.h"
#include "zatura/document-widget.h"
#include "zatura/page.h"
#include "zatura/page-widget.h"
#include "zatura/render.h"

static void settle(unsigned int ms) {
  const gint64 end = g_get_monotonic_time() + ms * 1000;
  while (g_get_monotonic_time() < end) { g_main_context_iteration(NULL, FALSE); g_usleep(1000); }
}
int main(int argc, char** argv) {
  if (argc != 6) { g_printerr("Usage: benchmark_render zoom device-scale columns cache-MiB recolor(0/1)\n"); return 2; }
  const unsigned int zoom = CLAMP(atoi(argv[1]), 1, 2), factor = CLAMP(atoi(argv[2]), 1, 2);
  const unsigned int columns = CLAMP(atoi(argv[3]), 1, 2), budget = MAX(atoi(argv[4]), 1);
  gtk_init();
  g_autofree char* dir = g_dir_make_tmp("zatura-profile-XXXXXX", NULL);
  g_autofree char* file = g_build_filename(dir, "scan.pdf", NULL);
  cairo_surface_t* image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1920, 2880);
  uint32_t* pixels = (uint32_t*)cairo_image_surface_get_data(image);
  for (unsigned y = 0; y < 2880; ++y) {
    for (unsigned x = 0; x < 1920; ++x) { pixels[y * 1920 + x] = ((x * 255 / 1920) << 16) | ((y * 255 / 2880) << 8) | ((x+y) % 256); }
  }
  cairo_surface_mark_dirty(image);
  cairo_surface_t* pdf = cairo_pdf_surface_create(file, 720, 1080);
  cairo_t* cr = cairo_create(pdf);
  for (unsigned i = 0; i < 6; ++i) {
    cairo_save(cr); cairo_scale(cr, 720.0 / 1920, 1080.0 / 2880);
    cairo_set_source_surface(cr, image, 0, 0); cairo_paint(cr); cairo_restore(cr); cairo_show_page(cr);
  }
  cairo_destroy(cr); cairo_surface_destroy(pdf); cairo_surface_destroy(image);
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, dir); zathura_set_data_dir(app, dir); zathura_set_cache_dir(app, dir);
  g_assert_true(zathura_init(app)); g_assert_true(document_open(app, file, NULL, NULL, 0, NULL)); settle(300);
  zathura_document_widget_stop_page_widget_preload(app->ui.document_widget);
  zathura_document_set_adjust_mode(app->document, ZATHURA_ADJUST_NONE);
  zathura_document_set_zoom(app->document, 1);
  const double normal = zathura_document_get_scale(app->document);
  zathura_document_set_zoom(app->document, zoom * 960.0 / (720 * normal));
  zathura_document_set_device_factors(app->document, factor, factor);
  zathura_renderer_set_cache_limit(app->sync.render_thread, budget);
  zathura_renderer_enable_recolor(app->sync.render_thread, atoi(argv[5]) != 0);
  zathura_renderer_set_page_effects(app->sync.render_thread, &(PageEffects){.brightness=-15,.contrast=5,.gamma=10,.saturation=-10});
  double render_ms = 0, repeat_ms = 0;
  int raster_width = 0;
  for (unsigned i = 0; i < 6; ++i) {
    zathura_page_t* page = zathura_document_get_page(app->document, i);
    ZathuraPageWidget* widget = ZATHURA_PAGE_WIDGET(zathura_document_widget_ensure_page(app->ui.document_widget, i));
    zathura_page_set_visibility(page, i < columns);
    const gint64 started = g_get_monotonic_time();
    cairo_surface_t* surface = zathura_renderer_render_page(app->sync.render_thread, page);
    g_assert_nonnull(surface); render_ms += (g_get_monotonic_time() - started) / 1000.0;
    raster_width = cairo_image_surface_get_width(surface);
    zathura_page_widget_update_surface(widget, surface, false); cairo_surface_destroy(surface);
    /* Run idle eviction without processing layout frames which would overwrite the synthetic visibility setup. */
    while (g_main_context_pending(NULL)) { g_main_context_iteration(NULL, FALSE); }
  }
  for (unsigned n = 0; n < 8; ++n) {
    zathura_renderer_set_page_effects(app->sync.render_thread, &(PageEffects){.brightness=-15-(int)n,.contrast=5,.gamma=10,.saturation=-10});
    const gint64 started = g_get_monotonic_time();
    cairo_surface_t* surface = zathura_renderer_render_page(app->sync.render_thread, zathura_document_get_page(app->document, 0));
    g_assert_nonnull(surface); repeat_ms += (g_get_monotonic_time() - started) / 1000.0;
    cairo_surface_destroy(surface);
  }
  size_t raw, display, total;
  zathura_renderer_get_cache_usage(app->sync.render_thread, &raw, &display, &total);
  struct rusage usage; getrusage(RUSAGE_SELF, &usage);
  g_print("%u,%u,%u,%u,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", zoom,factor,columns,budget,
      atoi(argv[5]) ? "true" : "false",raster_width,render_ms/6,repeat_ms/8,
      raw/1048576.0,display/1048576.0,total/1048576.0,usage.ru_maxrss/1024.0);
  zathura_free(app); g_unlink(file); g_rmdir(dir);
  return 0;
}
