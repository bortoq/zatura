/* SPDX-License-Identifier: Zlib */
#include <unistd.h>
#include <math.h>
#include <glib/gstdio.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/callbacks.h>
#include <cairo-pdf.h>
#include "zatura/zatura.h"
#include "zatura/document.h"
#include "zatura/page.h"
#include "zatura/render.h"
#include "zatura/shortcuts.h"

static void settle(void) {
  const gint64 end = g_get_monotonic_time() + 500000;
  while (g_get_monotonic_time() < end) {
    g_main_context_iteration(NULL, FALSE);
    g_usleep(1000);
  }
}

static void check_spread(zathura_t* app) {
  zathura_document_t* doc = app->document;
  unsigned int height, width;
  zathura_document_get_viewport_size(doc, &height, &width);
  cairo_surface_t* surface = zathura_renderer_render_page(app->sync.render_thread,
      zathura_document_get_page(doc, zathura_document_get_current_page_number(doc)));
  g_assert_nonnull(surface);
  g_assert_cmpint(cairo_image_surface_get_width(surface) * 2, <=, width + 2);
  g_assert_cmpint(cairo_image_surface_get_height(surface), ==, height);
  cairo_surface_destroy(surface);
  g_print("Spread fits viewport %u x %u, %u pages\n", width, height,
          zathura_document_get_number_of_pages(doc));
}

static void check_margin_pixels(zathura_t* app, unsigned page_index, int left, int right,
                                 int top, int bottom) {
  zathura_page_t* page = zathura_document_get_page(app->document, page_index);
  cairo_surface_t* surface = zathura_renderer_render_page(app->sync.render_thread, page);
  g_assert_nonnull(surface);
  cairo_surface_flush(surface);
  const int width = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);
  const int stride = cairo_image_surface_get_stride(surface);
  const unsigned char* pixels = cairo_image_surface_get_data(surface);
  int minx = width, maxx = 0, miny = height, maxy = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const unsigned char* pixel = pixels + y * stride + 4 * x;
      if (pixel[0] < 180 || pixel[1] < 180 || pixel[2] < 180) {
        minx = MIN(minx, x); maxx = MAX(maxx, x);
        miny = MIN(miny, y); maxy = MAX(maxy, y);
      }
    }
  }
  g_print("Page %u ink [%d,%d]..[%d,%d]; margins %d/%d/%d/%d\n",
      page_index, minx, miny, maxx, maxy, left, right, top, bottom);
  g_assert_cmpint(minx, >=, left - 2);
  g_assert_cmpint(minx, <=, left + 3);
  g_assert_cmpint(maxx, <, width - right + 2);
  g_assert_cmpint(miny, >=, top - 2);
  g_assert_cmpint(maxy, <, height - bottom + 2);
  /* Search boxes must follow the same translation as rendered text. */
  zathura_error_t error = ZATHURA_ERROR_OK;
  girara_list_t* hits = zathura_page_search_text(page, "MARKER", &error);
  g_assert_cmpint(error, ==, ZATHURA_ERROR_OK);
  g_assert_nonnull(hits);
  g_assert_cmpuint(girara_list_size(hits), >, 0);
  const double unit = zathura_document_get_scale(app->document);
  int font = 12;
  girara_setting_get(app->ui.session, "reflow-font-size", &font);
  /* Font metric boxes can extend beyond visible glyph ink by up to an em. */
  for (size_t i = 0; i < girara_list_size(hits); ++i) {
    const zathura_rectangle_t* rect = girara_list_nth(hits, i);
    g_assert_cmpfloat(rect->x1 * unit, >=, left - 2);
    g_assert_cmpfloat(rect->y1 * unit, >=, top - font * unit);
    g_assert_cmpfloat(rect->x2 * unit, <=, width - right + 2);
    g_assert_cmpfloat(rect->y2 * unit, <=, height - bottom + font * unit);
    bool overlaps_ink = false;
    for (int y = MAX(0, (int)floor(rect->y1 * unit)); y < MIN(height, (int)ceil(rect->y2 * unit)); ++y) {
      for (int x = MAX(0, (int)floor(rect->x1 * unit)); x < MIN(width, (int)ceil(rect->x2 * unit)); ++x) {
        const unsigned char* pixel = pixels + y * stride + 4 * x;
        overlaps_ink |= pixel[0] < 180 || pixel[1] < 180 || pixel[2] < 180;
      }
    }
    g_assert_true(overlaps_ink);
  }
  girara_list_free(hits);
  cairo_surface_destroy(surface);
}

static void check_margins(zathura_t* app) {
  const int top = 13, bottom = 21, outer = 17, inner = 35;
  girara_setting_set(app->ui.session, "reflow-margin-top", &top);
  girara_setting_set(app->ui.session, "reflow-margin-bottom", &bottom);
  girara_setting_set(app->ui.session, "reflow-margin-outer", &outer);
  girara_setting_set(app->ui.session, "reflow-margin-inner", &inner);
  settle();
  check_spread(app);
  check_margin_pixels(app, 2, inner, outer, top, bottom); /* first page in right column */
  check_margin_pixels(app, 3, outer, inner, top, bottom);
  bool rtl = true;
  girara_setting_set(app->ui.session, "page-right-to-left", &rtl);
  settle();
  check_margin_pixels(app, 2, outer, inner, top, bottom);
  check_margin_pixels(app, 3, inner, outer, top, bottom);
  rtl = false;
  girara_setting_set(app->ui.session, "page-right-to-left", &rtl);
  girara_setting_set(app->ui.session, "first-page-column", "1:1");
  settle();
  check_margin_pixels(app, 2, outer, inner, top, bottom);
  check_margin_pixels(app, 3, inner, outer, top, bottom);
  const unsigned one = 1;
  girara_setting_set(app->ui.session, "pages-per-row", &one);
  settle();
  check_margin_pixels(app, 2, outer, outer, top, bottom);
  /* Changing margin settings alone must trigger repagination. */
  const unsigned large_count = zathura_document_get_number_of_pages(app->document);
  const int zero = 0;
  girara_setting_set(app->ui.session, "reflow-margin-top", &zero);
  girara_setting_set(app->ui.session, "reflow-margin-bottom", &zero);
  girara_setting_set(app->ui.session, "reflow-margin-outer", &zero);
  girara_setting_set(app->ui.session, "reflow-margin-inner", &zero);
  settle();
  g_assert_cmpuint(zathura_document_get_number_of_pages(app->document), <, large_count);
  check_margin_pixels(app, 2, 0, 0, 0, 0);
  const int negative = -5;
  girara_setting_set(app->ui.session, "reflow-margin-top", &negative);
  int actual = -1;
  girara_setting_get(app->ui.session, "reflow-margin-top", &actual);
  g_assert_cmpint(actual, ==, 0);
  const int standard = 4;
  girara_setting_set(app->ui.session, "reflow-margin-top", &standard);
  girara_setting_set(app->ui.session, "reflow-margin-bottom", &standard);
  girara_setting_set(app->ui.session, "reflow-margin-outer", &standard);
  girara_setting_set(app->ui.session, "reflow-margin-inner", &standard);
  girara_setting_set(app->ui.session, "first-page-column", "1:2");
  const unsigned two = 2;
  girara_setting_set(app->ui.session, "pages-per-row", &two);
  settle();
}

int main(int argc, char** argv) {
  gtk_init();
  zathura_t* app = zathura_create();
  zathura_set_config_dir(app, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(app));
  for (int i = 1; i < argc; ++i) {
    const unsigned one = 1;
    const int font = 12;
    girara_setting_set(app->ui.session, "pages-per-row", &one);
    girara_setting_set(app->ui.session, "reflow-font-size", &font);
    if (!document_open(app, argv[i], NULL, NULL, 0, NULL)) {
      zathura_free(app);
      return 77;
    }
    g_assert_true(zathura_document_is_reflowable(app->document));
    settle();
    const unsigned two = 2;
    girara_setting_set(app->ui.session, "pages-per-row", &two);
    settle();
    check_spread(app);
    page_set(app, 0);
    settle();
    for (unsigned step = 0; step < 24; ++step) {
      girara_argument_t scroll = {.n = step < 12 ? FULL_DOWN : FULL_UP};
      sc_scroll(app->ui.session, &scroll, NULL, 0);
      const gint64 deadline = g_get_monotonic_time() + 80000;
      while (g_get_monotonic_time() < deadline) {
        g_main_context_iteration(NULL, FALSE);
        g_usleep(1000);
      }
      unsigned vh, vw;
      zathura_document_get_viewport_size(app->document, &vh, &vw);
      const unsigned current = zathura_document_get_current_page_number(app->document);
      const unsigned row = (current + 1) / 2; /* default first-page-column 1:2 */
      GtkAdjustment* vertical = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app->ui.view));
      g_assert_cmpfloat_with_epsilon(gtk_adjustment_get_value(vertical), row * (vh + 1), 1.0);
    }
    g_assert_cmpuint(zathura_document_get_current_page_number(app->document), ==, 0);
    check_margins(app);
    const unsigned pages = zathura_document_get_number_of_pages(app->document);
    page_set(app, pages / 2);
    settle();
    /* Physical +/= row works even when the supplied symbol is non-Latin. */
    g_assert_true(girara_process_view_key_with_code(app->ui.session, GDK_KEY_Cyrillic_be, 21, GDK_CONTROL_MASK));
    int actual = 0;
    girara_setting_get(app->ui.session, "reflow-font-size", &actual);
    g_assert_cmpint(actual, ==, 13);
    settle();
    check_spread(app);
    g_assert_cmpuint(zathura_document_get_number_of_pages(app->document), >=, pages);
    g_assert_cmpuint(zathura_document_get_current_page_number(app->document), >, 0);
    g_assert_true(girara_process_view_key_with_code(app->ui.session, GDK_KEY_plus, 21,
                                                  GDK_CONTROL_MASK | GDK_SHIFT_MASK));
    girara_setting_get(app->ui.session, "reflow-font-size", &actual);
    g_assert_cmpint(actual, ==, 14);
    g_assert_true(girara_process_view_key_with_code(app->ui.session, GDK_KEY_Cyrillic_yu, 20, GDK_CONTROL_MASK));
    girara_setting_get(app->ui.session, "reflow-font-size", &actual);
    g_assert_cmpint(actual, ==, 13);
    settle();
    check_spread(app);
    /* Resizing a two-page view must change the actual page layout. */
    gtk_window_set_default_size(GTK_WINDOW(app->ui.session->gtk.window), 900, 650);
    settle();
    check_spread(app);
    const int high = 1000;
    girara_setting_set(app->ui.session, "reflow-font-size", &high);
    girara_setting_get(app->ui.session, "reflow-font-size", &actual);
    g_assert_cmpint(actual, ==, 72);
    const int low = -10;
    girara_setting_set(app->ui.session, "reflow-font-size", &low);
    girara_setting_get(app->ui.session, "reflow-font-size", &actual);
    g_assert_cmpint(actual, ==, 6);
    g_print("Reflow and font shortcuts passed: %s\n", argv[i]);
    document_close(app, false);
  }
  char* fixture = NULL;
  int fd = g_file_open_tmp("zatura-fixed-XXXXXX.pdf", &fixture, NULL);
  close(fd);
  cairo_surface_t* pdf = cairo_pdf_surface_create(fixture, 300, 500);
  cairo_surface_destroy(pdf);
  if (document_open(app, fixture, NULL, NULL, 0, NULL)) {
    g_assert_false(zathura_document_is_reflowable(app->document));
    girara_argument_t step = {.n = 1};
    g_assert_false(sc_adjust_book_font(app->ui.session, &step, NULL, 1));
  }
  zathura_free(app);
  g_unlink(fixture);
  g_free(fixture);
  return 0;
}
