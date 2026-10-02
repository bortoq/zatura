/* SPDX-License-Identifier: Zlib */
#ifndef ZATURA_PAGE_EFFECTS_H
#define ZATURA_PAGE_EFFECTS_H

#include <stdbool.h>
#include <stddef.h>
#include <cairo.h>

/* Display adjustments, each in [-100, 100], with zero as identity. */
typedef struct {
  int brightness;
  int contrast;
  int gamma;
  int saturation;
} PageEffects;

bool page_effects_valid(const PageEffects* effects);
bool page_effects_apply_cancellable(cairo_surface_t* surface, const PageEffects* effects,
                                    bool (*cancelled)(void*), void* data);
void page_effects_apply(cairo_surface_t* surface, const PageEffects* effects);

/* Recolor pixel transforms; GTK/Girara and plugin image discovery stay in adapters. */
typedef struct { double red, green, blue, alpha; } PageColor;
typedef struct { double x1, y1, x2, y2; } PageRecolorRect;
typedef struct {
  PageColor dark, light;
  bool hue, reverse_video, adjust_lightness;
} PageRecolor;
bool page_recolor_apply_cancellable(cairo_surface_t* surface, const PageRecolor* options,
    const PageRecolorRect* rectangles, size_t count, bool (*cancelled)(void*), void* context);
#endif
