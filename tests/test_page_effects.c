/* SPDX-License-Identifier: Zlib */
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "page-effects.h"

static uint32_t adjusted(uint32_t pixel, PageEffects effects, cairo_format_t format) {
  cairo_surface_t* surface = cairo_image_surface_create(format, 1, 1);
  *(uint32_t*)cairo_image_surface_get_data(surface) = pixel;
  cairo_surface_mark_dirty(surface);
  page_effects_apply(surface, &effects);
  const uint32_t result = *(uint32_t*)cairo_image_surface_get_data(surface);
  cairo_surface_destroy(surface);
  return result;
}

static void test_identity(void) {
  const uint32_t pixels[] = {0xff000000, 0xffffffff, 0xffd08021, 0x80402010, 0x00000000};
  for (unsigned i = 0; i < G_N_ELEMENTS(pixels); ++i) {
    g_assert_cmphex(adjusted(pixels[i], (PageEffects){0}, CAIRO_FORMAT_ARGB32), ==, pixels[i]);
    g_assert_cmphex(adjusted(pixels[i], (PageEffects){0}, CAIRO_FORMAT_RGB24), ==, pixels[i]);
  }
}

static void test_controls(void) {
  g_assert_cmphex(adjusted(0xff808080, (PageEffects){.brightness = -100}, CAIRO_FORMAT_ARGB32), ==, 0xff000000);
  g_assert_cmphex(adjusted(0xff808080, (PageEffects){.brightness = 100}, CAIRO_FORMAT_ARGB32), ==, 0xffffffff);
  g_assert_cmphex(adjusted(0xff0020ff, (PageEffects){.contrast = -100}, CAIRO_FORMAT_ARGB32), ==, 0xff808080);
  g_assert_cmphex(adjusted(0xffff0000, (PageEffects){.saturation = -100}, CAIRO_FORMAT_ARGB32), ==, 0xff363636);
  /* gamma=50 maps 0.25 to sqrt(0.25), gamma=-50 maps it to its square. */
  g_assert_cmphex(adjusted(0xff404040, (PageEffects){.gamma = 50}, CAIRO_FORMAT_ARGB32), ==, 0xff808080);
  g_assert_cmphex(adjusted(0xff404040, (PageEffects){.gamma = -50}, CAIRO_FORMAT_ARGB32), ==, 0xff101010);
  /* Combined adjustments: contrast, additive brightness, then gamma. */
  g_assert_cmphex(adjusted(0xff404040, (PageEffects){.contrast = -100, .brightness = -25, .gamma = 50},
                           CAIRO_FORMAT_ARGB32), ==, 0xff808080);
}

static void test_alpha_and_stride(void) {
  g_assert_cmphex(adjusted(0x80402010, (PageEffects){.brightness = 100}, CAIRO_FORMAT_ARGB32), ==, 0x80808080);
  g_assert_cmphex(adjusted(0, (PageEffects){.gamma = 100}, CAIRO_FORMAT_ARGB32), ==, 0);
  uint32_t pixels[] = {0xff404040, 0x12345678, 0x87654321, 0xff808080, 0x12345678, 0x87654321};
  cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*)pixels, CAIRO_FORMAT_RGB24, 1, 2, 12);
  page_effects_apply(surface, &(PageEffects){.contrast = -100});
  g_assert_cmphex(pixels[0], ==, 0xff808080);
  g_assert_cmphex(pixels[3], ==, 0xff808080);
  g_assert_cmphex(pixels[1], ==, 0x12345678);
  g_assert_cmphex(pixels[2], ==, 0x87654321);
  g_assert_cmphex(pixels[4], ==, 0x12345678);
  g_assert_cmphex(pixels[5], ==, 0x87654321);
  cairo_surface_destroy(surface);
}

static void test_bounds(void) {
  g_assert_true(page_effects_valid(&(PageEffects){0}));
  g_assert_true(page_effects_valid(&(PageEffects){-100, 100, -100, 100}));
  g_assert_false(page_effects_valid(&(PageEffects){.gamma = 101}));
  g_assert_false(page_effects_valid(&(PageEffects){.brightness = -101}));
  g_assert_false(page_effects_valid(NULL));
}

static bool cancel_filter(void* data) {
  unsigned* calls = data;
  return ++*calls == 2;
}

static void test_cancellation(void) {
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 256);
  cairo_t* cr = cairo_create(surface);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);
  cairo_destroy(cr);
  unsigned calls = 0;
  g_assert_false(page_effects_apply_cancellable(surface, &(PageEffects){.brightness = -100}, cancel_filter, &calls));
  g_assert_cmpuint(calls, ==, 2);
  /* Early rows changed, but later rows were skipped; callers must discard this partial output. */
  const int stride = cairo_image_surface_get_stride(surface);
  const unsigned char* pixels = cairo_image_surface_get_data(surface);
  g_assert_cmphex(*(const uint32_t*)pixels, ==, 0xff000000);
  g_assert_cmphex(*(const uint32_t*)(pixels + 200 * stride), ==, 0xffffffff);
  cairo_surface_destroy(surface);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/page-effects/identity", test_identity);
  g_test_add_func("/page-effects/controls", test_controls);
  g_test_add_func("/page-effects/alpha-stride", test_alpha_and_stride);
  g_test_add_func("/page-effects/bounds", test_bounds);
  g_test_add_func("/page-effects/cancellation", test_cancellation);
  return g_test_run();
}
