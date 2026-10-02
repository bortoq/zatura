/* SPDX-License-Identifier: Zlib */
#include "page-effects.h"

#include <math.h>
#include <stdint.h>
#include <glib.h>

bool page_effects_valid(const PageEffects* effects) {
  return effects && effects->brightness >= -100 && effects->brightness <= 100 &&
         effects->contrast >= -100 && effects->contrast <= 100 && effects->gamma >= -100 && effects->gamma <= 100 &&
         effects->saturation >= -100 && effects->saturation <= 100;
}

bool page_effects_apply_cancellable(cairo_surface_t* surface, const PageEffects* effects,
                                    bool (*cancelled)(void*), void* data_context) {
  g_return_val_if_fail(surface && page_effects_valid(effects), false);
  if (effects->brightness == 0 && effects->contrast == 0 && effects->gamma == 0 && effects->saturation == 0) {
    return true;
  }
  g_return_val_if_fail(cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE, false);
  const cairo_format_t format = cairo_image_surface_get_format(surface);
  g_return_val_if_fail(format == CAIRO_FORMAT_ARGB32 || format == CAIRO_FORMAT_RGB24, false);

  const double brightness = effects->brightness / 100.0;
  const double contrast = 1.0 + effects->contrast / 100.0;
  const double saturation = 1.0 + effects->saturation / 100.0;
  /* Positive gamma lifts midtones. LUT avoids pow() for every channel of every pixel. */
  double gamma_table[4097];
  if (effects->gamma != 0) {
    const double exponent = exp2(-effects->gamma / 50.0);
    for (unsigned i = 0; i < G_N_ELEMENTS(gamma_table); ++i) {
      gamma_table[i] = pow(i / 4096.0, exponent);
    }
  }

  /* Opaque pixels without saturation mixing need only three byte lookups.
   * Grayscale pixels use the same path even when saturation is adjusted. */
  unsigned char channel_table[256];
  for (unsigned i = 0; i < G_N_ELEMENTS(channel_table); ++i) {
    double value = CLAMP(0.5 + contrast * (i / 255.0 - 0.5) + brightness, 0.0, 1.0);
    if (effects->gamma != 0) {
      value = gamma_table[(unsigned)(value * 4096 + 0.5)];
    }
    channel_table[i] = (unsigned char)(value * 255 + 0.5);
  }

  cairo_surface_flush(surface);
  unsigned char* data = cairo_image_surface_get_data(surface);
  const int stride = cairo_image_surface_get_stride(surface);
  const int width = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);
  for (int y = 0; y < height; ++y) {
    if ((y & 63) == 0 && cancelled && cancelled(data_context)) {
      return false;
    }
    uint32_t* row = (uint32_t*)(data + (size_t)y * stride);
    for (int x = 0; x < width; ++x) {
      const uint32_t pixel = row[x];
      const unsigned alpha = format == CAIRO_FORMAT_ARGB32 ? pixel >> 24 : 255;
      if (alpha == 0) {
        continue;
      }
      const unsigned r = (pixel >> 16) & 255;
      const unsigned g = (pixel >> 8) & 255;
      const unsigned b = pixel & 255;
      if (alpha == 255 && (effects->saturation == 0 || (r == g && g == b))) {
        row[x] = (pixel & 0xff000000) | ((uint32_t)channel_table[r] << 16) |
                 ((uint32_t)channel_table[g] << 8) | channel_table[b];
        continue;
      }
      double channels[] = {((pixel >> 16) & 255) / (double)alpha,
                           ((pixel >> 8) & 255) / (double)alpha, (pixel & 255) / (double)alpha};
      const double luminance = 0.2126 * channels[0] + 0.7152 * channels[1] + 0.0722 * channels[2];
      uint32_t output = pixel & 0xff000000;
      for (unsigned c = 0; c < 3; ++c) {
        const double saturated = luminance + saturation * (channels[c] - luminance);
        double value = CLAMP(0.5 + contrast * (saturated - 0.5) + brightness, 0.0, 1.0);
        if (effects->gamma != 0) {
          value = gamma_table[(unsigned)(value * 4096 + 0.5)];
        }
        output |= (uint32_t)(value * alpha + 0.5) << (16 - c * 8);
      }
      row[x] = output;
    }
  }
  cairo_surface_mark_dirty(surface);
  return true;
}

void page_effects_apply(cairo_surface_t* surface, const PageEffects* effects) {
  page_effects_apply_cancellable(surface, effects, NULL, NULL);
}
