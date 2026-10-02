/* SPDX-License-Identifier: Zlib */
#include "page-effects.h"

#include <math.h>
#include <float.h>
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

static bool pixel_inside_rectangles(const PageRecolorRect* rectangles, size_t count, unsigned int x, unsigned int y) {
  for (size_t idx = 0; idx != count; ++idx) {
    const PageRecolorRect* rect_it = &rectangles[idx];
    if (rect_it->x1 <= x && rect_it->x2 >= x && rect_it->y1 <= y && rect_it->y2 >= y) {
      return true;
    }
  }

  return false;
}

/* Returns the maximum possible saturation for given h and l.
   Assumes that l is in the interval l1, l2 and corrects the value to
   force u=0 on l1 and l2 */
static double colorumax(const double h[3], double l, double l1, double l2) {
  if (fabs(h[0]) <= DBL_EPSILON && fabs(h[1]) <= DBL_EPSILON && fabs(h[2]) <= DBL_EPSILON) {
    return 0;
  }

  const double lv = (l - l1) / (l2 - l1); /* Remap l to the whole interval [0,1] */
  double u        = DBL_MAX;
  double v        = DBL_MAX;
  for (unsigned int k = 0; k < 3; ++k) {
    if (h[k] > DBL_EPSILON) {
      u = fmin(fabs((1 - l) / h[k]), u);
      v = fmin(fabs((1 - lv) / h[k]), v);
    } else if (h[k] < -DBL_EPSILON) {
      u = fmin(fabs(l / h[k]), u);
      v = fmin(fabs(lv / h[k]), v);
    }
  }

  /* rescale v according to the length of the interval [l1, l2] */
  v = fabs(l2 - l1) * v;

  /* forces the returned value to be 0 on l1 and l2, trying not to distort colors too much */
  return fmin(u, v);
}

/* RGB weights for computing lightness. Must sum to one */
static const double weights[] = {0.30, 0.59, 0.11};

static bool recolor_slow(const PageRecolor* options, unsigned int page_width, unsigned int page_height,
                         cairo_surface_t* surface, const PageRecolorRect* rectangles, size_t count, bool (*cancelled)(void*), void* context) {
  const PageColor rgb1 = options->dark;
  const PageColor rgb2 = options->light;

  const double l1       = weights[0] * rgb1.red + weights[1] * rgb1.green + weights[2] * rgb1.blue;
  const double l2       = weights[0] * rgb2.red + weights[1] * rgb2.green + weights[2] * rgb2.blue;
  const double negalph1 = 1. - rgb1.alpha;
  const double negalph2 = 1. - rgb2.alpha;

  const double rgb_diff[] = {rgb2.red - rgb1.red, rgb2.green - rgb1.green, rgb2.blue - rgb1.blue};

  const double h1[3] = {
      rgb1.red * rgb1.alpha - l1,
      rgb1.green * rgb1.alpha - l1,
      rgb1.blue * rgb1.alpha - l1,
  };

  const double h2[3] = {
      rgb2.red * rgb2.alpha - l2,
      rgb2.green * rgb2.alpha - l2,
      rgb2.blue * rgb2.alpha - l2,
  };

  bool adjust_lightness = options->adjust_lightness;

  const int rowstride  = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  for (unsigned int y = 0; y < page_height; y++) {
    if ((y & 63) == 0 && cancelled && cancelled(context)) { return false; }
    unsigned char* data = image + y * rowstride;

    for (unsigned int x = 0; x < page_width; x++, data += 4) {
      /* Check if the pixel belongs to an image when in reverse video mode*/
      if (options->reverse_video == true && count > 0) {
        const bool inside_image = pixel_inside_rectangles(rectangles, count, x, y);
        /* If it's inside and image don't recolor */
        if (inside_image == true) {
          /* It is not guaranteed that the pixel is already opaque. */
          data[3] = 255;
          continue;
        }
      }

      /* Careful. data color components blue, green, red. */
      const double rgb[3] = {data[2] / 255., data[1] / 255., data[0] / 255.};

      /* compute h, s, l data   */
      double l = weights[0] * rgb[0] + weights[1] * rgb[1] + weights[2] * rgb[2];

      if (options->hue == true) {
        /* adjusting lightness keeping hue of current color. white and black
         * go to grays of same ligtness as light and dark colors. */
        const double h[3] = {rgb[0] - l, rgb[1] - l, rgb[2] - l};

        /* u is the maximum possible saturation for given h and l. s is a
         * rescaled saturation between 0 and 1 */
        const double u = colorumax(h, l, 0, 1);
        const double s = fabs(u) > DBL_EPSILON ? 1.0 / u : 0.0;

        /* adjust according to quartic curve, then average with original weighed
         * by half saturation. */
        if (adjust_lightness) {
          /* l = l * s/2 + l^4 * (1 - s/2) */
          double adj = l * l * l * l;
          l          = (l - adj) * (s * 0.5) + adj;
        }

        /* Interpolates lightness between light and dark colors. white goes to
         * light, and black goes to dark. */
        l = l * (l2 - l1) + l1;

        const double su = s * colorumax(h, l, l1, l2);

        /* Mix lightcolor, darkcolor and the original color, according to the
         * minimal and maximal channel of the original color */
        const double tr1 = (1. - fmax(fmax(rgb[0], rgb[1]), rgb[2]));
        const double tr2 = fmin(fmin(rgb[0], rgb[1]), rgb[2]);
        data[3]          = (unsigned char)round(255. * (1. - tr1 * negalph1 - tr2 * negalph2));
        data[2]          = (unsigned char)round(255. * fmin(1, fmax(0, tr1 * h1[0] + tr2 * h2[0] + (l + su * h[0]))));
        data[1]          = (unsigned char)round(255. * fmin(1, fmax(0, tr1 * h1[1] + tr2 * h2[1] + (l + su * h[1]))));
        data[0]          = (unsigned char)round(255. * fmin(1, fmax(0, tr1 * h1[2] + tr2 * h2[2] + (l + su * h[2]))));
      } else {
        if (adjust_lightness) {
          l = l * l;
        }

        /* linear interpolation between dark and light with color ligtness as
         * a parameter */
        const double f1 = 1. - (1. - fmax(fmax(rgb[0], rgb[1]), rgb[2])) * negalph1;
        const double f2 = fmin(fmin(rgb[0], rgb[1]), rgb[2]) * negalph2;
        data[3]         = (unsigned char)round(255. * (f1 - f2));
        data[2]         = (unsigned char)round(255. * (l * rgb_diff[0] - f2 * rgb2.red + f1 * rgb1.red));
        data[1]         = (unsigned char)round(255. * (l * rgb_diff[1] - f2 * rgb2.green + f1 * rgb1.green));
        data[0]         = (unsigned char)round(255. * (l * rgb_diff[2] - f2 * rgb2.blue + f1 * rgb1.blue));
      }
    }
  }
  return true;
}

static bool recolor_fast(const PageRecolor* options, unsigned int page_width, unsigned int page_height,
                         cairo_surface_t* surface, const PageRecolorRect* rectangles, size_t count, bool (*cancelled)(void*), void* context) {
  const PageColor rgb1 = options->dark;
  const PageColor rgb2 = options->light;

  const double l1 = weights[0] * rgb1.red + weights[1] * rgb1.green + weights[2] * rgb1.blue;
  const double l2 = weights[0] * rgb2.red + weights[1] * rgb2.green + weights[2] * rgb2.blue;

  const double rgb_diff[] = {rgb2.red - rgb1.red, rgb2.green - rgb1.green, rgb2.blue - rgb1.blue};

  bool adjust_lightness = options->adjust_lightness;

  const int rowstride  = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  for (unsigned int y = 0; y < page_height; y++) {
    if ((y & 63) == 0 && cancelled && cancelled(context)) { return false; }
    unsigned char* data = image + y * rowstride;

    for (unsigned int x = 0; x < page_width; x++, data += 4) {
      /* Check if the pixel belongs to an image when in reverse video mode*/
      if (options->reverse_video == true && count > 0) {
        const bool inside_image = pixel_inside_rectangles(rectangles, count, x, y);
        /* If it's inside and image don't recolor */
        if (inside_image == true) {
          /* It is not guaranteed that the pixel is already opaque. */
          data[3] = 255;
          continue;
        }
      }

      /* Careful. data color components blue, green, red. */
      const double rgb[3] = {data[2] / 255., data[1] / 255., data[0] / 255.};

      /* compute h, s, l data   */
      double l = weights[0] * rgb[0] + weights[1] * rgb[1] + weights[2] * rgb[2];

      if (options->hue == true) {
        /* adjusting lightness keeping hue of current color. white and black
         * go to grays of same ligtness as light and dark colors. */
        const double h[3] = {rgb[0] - l, rgb[1] - l, rgb[2] - l};

        /* u is the maximum possible saturation for given h and l. s is a
         * rescaled saturation between 0 and 1 */
        const double u = colorumax(h, l, 0, 1);
        const double s = fabs(u) > DBL_EPSILON ? 1.0 / u : 0.0;

        /* adjust according to quartic curve, then average with original weighed
         * by half saturation. */
        if (adjust_lightness) {
          /* l = l * s/2 + l^4 * (1 - s/2) */
          double adj = l * l * l * l;
          l          = (l - adj) * (s * 0.5) + adj;
        }

        /* Interpolates lightness between light and dark colors. white goes to
         * light, and black goes to dark. */
        l = l * (l2 - l1) + l1;

        const double su = s * colorumax(h, l, l1, l2);

        /* Mix lightcolor, darkcolor and the original color, according to the
         * minimal and maximal channel of the original color */
        data[3] = 255;
        data[2] = (unsigned char)round(255. * (l + su * h[0]));
        data[1] = (unsigned char)round(255. * (l + su * h[1]));
        data[0] = (unsigned char)round(255. * (l + su * h[2]));
      } else {
        if (adjust_lightness) {
          l = l * l;
        }

        /* linear interpolation between dark and light with color ligtness as
         * a parameter */
        data[3] = 255;
        data[2] = (unsigned char)round(255. * (l * rgb_diff[0] + rgb1.red));
        data[1] = (unsigned char)round(255. * (l * rgb_diff[1] + rgb1.green));
        data[0] = (unsigned char)round(255. * (l * rgb_diff[2] + rgb1.blue));
      }
    }
  }
  return true;
}

bool page_recolor_apply_cancellable(cairo_surface_t* surface, const PageRecolor* options,
    const PageRecolorRect* rectangles, size_t count, bool (*cancelled)(void*), void* context) {
  g_return_val_if_fail(surface && options && (!count || rectangles), false);
  g_return_val_if_fail(cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE, false);
  const PageColor a = options->dark, b = options->light;
  const bool fast = (!options->hue || (fabs(a.red-a.blue) < DBL_EPSILON && fabs(a.red-a.green) < DBL_EPSILON &&
      fabs(b.red-b.blue) < DBL_EPSILON && fabs(b.red-b.green) < DBL_EPSILON)) &&
      a.alpha >= 1-DBL_EPSILON && b.alpha >= 1-DBL_EPSILON;
  cairo_surface_flush(surface);
  const unsigned width = cairo_image_surface_get_width(surface), height = cairo_image_surface_get_height(surface);
  const bool ok = fast ? recolor_fast(options, width, height, surface, rectangles, count, cancelled, context)
                       : recolor_slow(options, width, height, surface, rectangles, count, cancelled, context);
  cairo_surface_mark_dirty(surface);
  return ok;
}
