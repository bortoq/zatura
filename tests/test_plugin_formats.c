/* SPDX-License-Identifier: Zlib */
#include <stdint.h>
#include <girara-gtk/session.h>
#include "zatura/zatura.h"
#include "zatura/internal.h"
#include "zatura/document.h"
#include "zatura/page.h"
#include "zatura/render.h"

int main(int argc, char** argv) {
  gtk_init();
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(app));
  for (int i = 1; i < argc; ++i) {
    g_assert_true(document_open(app, argv[i], NULL, NULL, 0, NULL));
    zathura_document_t* doc = zathura_get_document(app);
    g_assert_cmpuint(zathura_document_get_number_of_pages(doc), >, 0);
    zathura_document_set_adjust_mode(doc, ZATHURA_ADJUST_NONE);
    zathura_document_set_zoom(doc, 1);
    for (unsigned page = 0; page < zathura_document_get_number_of_pages(doc); ++page) {
      cairo_surface_t* surface = zathura_renderer_render_page(app->sync.render_thread, zathura_document_get_page(doc, page));
      g_assert_nonnull(surface);
      g_assert_cmpint(cairo_surface_status(surface), ==, CAIRO_STATUS_SUCCESS);
      cairo_surface_flush(surface);
      const int width = cairo_image_surface_get_width(surface);
      const int height = cairo_image_surface_get_height(surface);
      g_assert_cmpint(width, >, 0);
      g_assert_cmpint(height, >, 0);
      bool ink = false;
      const unsigned char* data = cairo_image_surface_get_data(surface);
      for (int y = 0; y < height && !ink; ++y) {
        const uint32_t* row = (const uint32_t*)(data + y * cairo_image_surface_get_stride(surface));
        for (int x = 0; x < width; ++x) {
          if ((row[x] & 0xffffff) != 0xffffff) { ink = true; break; }
        }
      }
      g_assert_true(ink);
      cairo_surface_destroy(surface);
    }
    g_print("Rendered %s (%u pages)\n", argv[i], zathura_document_get_number_of_pages(doc));
    g_assert_true(document_close(app, false));
  }
  zathura_free(app);
  return 0;
}
