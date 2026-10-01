/* SPDX-License-Identifier: Zlib */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdlib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <girara/types.h>
#include "types.h"

typedef struct zatura_renderer_class_s ZaturaRendererClass;

struct zatura_renderer_s {
  GObject parent;
};

struct zatura_renderer_class_s {
  GObjectClass parent_class;
};

#define ZATURA_TYPE_RENDERER (zatura_renderer_get_type())
#define ZATURA_RENDERER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_RENDERER, ZaturaRenderer))
#define ZATURA_RENDERER_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_RENDERER, ZaturaRendererClass))
#define ZATURA_IS_RENDERER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_RENDERER))
#define ZATURA_IS_RENDERER_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_RENDERER))
#define ZATURA_RENDERER_GET_CLASS (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_RENDERER, ZaturaRendererClass))

/**
 * Returns the type of the renderer.
 * @return the type
 */
GType zatura_renderer_get_type(void);
/**
 * Create a renderer.
 * @return a renderer object
 */
ZaturaRenderer* zatura_renderer_new(size_t cache_size);

/* Render a page synchronously through the same locked path as the render thread.
 * The caller owns the returned surface and must destroy it. */
cairo_surface_t* zatura_renderer_render_page(ZaturaRenderer* renderer, zatura_page_t* page);

/* Parses the page with its plugin if needed, taking the render lock internally. */
bool zatura_renderer_load_page(ZaturaRenderer* renderer, zatura_page_t* page);

/**
 * Return whether recoloring is enabled.
 * @param renderer a renderer object
 * @returns true if recoloring is enabled, false otherwise
 */
bool zatura_renderer_recolor_enabled(ZaturaRenderer* renderer);
/**
 * Enable/disable recoloring.
 * @param renderer a renderer object
 * @param enable whether to enable or disable recoloring
 */
void zatura_renderer_enable_recolor(ZaturaRenderer* renderer, bool enable);
/**
 * Return whether hue should be preserved while recoloring.
 * @param renderer a renderer object
 * @returns true if hue should be preserved, false otherwise
 */
bool zatura_renderer_recolor_hue_enabled(ZaturaRenderer* renderer);
/**
 * Enable/disable preservation of hue while recoloring.
 * @param renderer a renderer object
 * @param enable whether to enable or disable hue preservation
 */
void zatura_renderer_enable_recolor_hue(ZaturaRenderer* renderer, bool enable);
/**
 * Return whether images should be recolored while recoloring.
 * @param renderer a renderer object
 * @returns true if images should be recolored, false otherwise
 */
bool zatura_renderer_recolor_reverse_video_enabled(ZaturaRenderer* renderer);
/**
 * Enable/disable recoloring of images while recoloring.
 * @param renderer a renderer object
 * @param enable or disable images recoloring
 */
void zatura_renderer_enable_recolor_reverse_video(ZaturaRenderer* renderer, bool enable);
/**
 * Return whether lightness should be adjusted while recoloring.
 * @param renderer a renderer object
 * @returns true if lightness should be adjusted, false otherwise
 */
bool zatura_renderer_recolor_adjust_lightness_enabled(ZaturaRenderer* renderer);
/**
 * Enable/disable adjusting lightness while recoloring.
 * @param renderer a renderer object
 * @param enable or disable adjusting lightness
 */
void zatura_renderer_enable_recolor_adjust_lightness(ZaturaRenderer* renderer, bool enable);
/**
 * Set light and dark colors for recoloring.
 * @param renderer a renderer object
 * @param light light color
 * @param dark dark color
 */
void zatura_renderer_set_recolor_colors(ZaturaRenderer* renderer, const GdkRGBA* light, const GdkRGBA* dark);
/**
 * Set light and dark colors for recoloring.
 * @param renderer a renderer object
 * @param light light color
 * @param dark dark color
 */
void zatura_renderer_set_recolor_colors_str(ZaturaRenderer* renderer, const char* light, const char* dark);
/**
 * Get light and dark colors for recoloring.
 * @param renderer a renderer object
 * @param light light color
 * @param dark dark color
 */
void zatura_renderer_get_recolor_colors(ZaturaRenderer* renderer, GdkRGBA* light, GdkRGBA* dark);
/**
 * Stop rendering. This is a terminal operation: after it returns, no more
 * render jobs will be accepted or executed.
 * @param renderer a render object
 */
void zatura_renderer_stop(ZaturaRenderer* renderer);

/**
 * Lock the render thread. This is useful if you want to render on your own (e.g
 * for printing).
 *
 * @param renderer renderer object
 */
void zatura_renderer_lock(ZaturaRenderer* renderer);

/**
 * Unlock the render thread.
 *
 * @param renderer renderer object.
 */
void zatura_renderer_unlock(ZaturaRenderer* renderer);

/**
 * Add a page to the page cache.
 *
 * @param renderer renderer object.
 * @param page_index The index of the page to be cached.
 */
void zatura_renderer_page_cache_add(ZaturaRenderer* renderer, unsigned int page_index);

typedef struct zatura_render_request_class_s ZaturaRenderRequestClass;

struct zatura_render_request_s {
  GObject parent;
};

struct zatura_render_request_class_s {
  GObjectClass parent_class;
};

#define ZATURA_TYPE_RENDER_REQUEST (zatura_render_request_get_type())
#define ZATURA_RENDER_REQUEST(obj)                                                                                    \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_RENDER_REQUEST, ZaturaRenderRequest))
#define ZATURA_RENDER_REQUEST_CLASS(obj)                                                                              \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_RENDER_REQUEST, ZaturaRenderRequestClass))
#define ZATURA_IS_RENDER_REQUEST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_RENDER_REQUEST))
#define ZATURA_IS_RENDER_REQUEST_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_RENDER_REQUEST))
#define ZATURA_RENDER_REQUEST_GET_CLASS                                                                               \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_RENDER_REQUEST, ZaturaRenderRequestClass))

/**
 * Returns the type of the render request.
 * @return the type
 */
GType zatura_render_request_get_type(void);
/**
 * Create a render request object
 * @param renderer a renderer object
 * @param page the page to be displayed
 * @returns render request object
 */
ZaturaRenderRequest* zatura_render_request_new(ZaturaRenderer* renderer, zatura_page_t* page);

/**
 * Add a page to the render thread list that should be rendered.
 *
 * @param request request object of the page that should be renderer
 * @param last_view_time last view time of the page
 */
void zatura_render_request(ZaturaRenderRequest* request, gint64 last_view_time);

/**
 * Abort an existing render request.
 *
 * @param request request that should be aborted
 */
void zatura_render_request_abort(ZaturaRenderRequest* request);

/**
 * Update the time the page associated to the render request has been viewed the
 * last time.
 *
 * @param request request that should be updated
 */
void zatura_render_request_update_view_time(ZaturaRenderRequest* request);

/**
 * Set "plain" rendering mode, i.e. disabling scaling, recoloring, etc.
 * @param request request that should be updated
 * @param render_plain "plain" rendering setting
 */
void zatura_render_request_set_render_plain(ZaturaRenderRequest* request, bool render_plain);

/**
 * Get "plain" rendering mode, i.e. disabling scaling, recoloring, etc.
 * @param request request that should be updated
 * @returns "plain" rendering setting
 */
bool zatura_render_request_get_render_plain(ZaturaRenderRequest* request);

/**
 * This function is used to unmark all pages as not rendered. This should
 * be used if all pages should be rendered again (e.g.: the zoom level or the
 * colors have changed)
 *
 * @param zatura Zatura object
 */
void render_all(zatura_t* zatura);

#endif // RENDER_H
