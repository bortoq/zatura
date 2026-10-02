/* SPDX-License-Identifier: Zlib */
#ifndef ZATURA_PAGE_EFFECTS_H
#define ZATURA_PAGE_EFFECTS_H

#include <stdbool.h>
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

#endif
