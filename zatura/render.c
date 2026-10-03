/* SPDX-License-Identifier: Zlib */

#include "render.h"

#include <math.h>
#include <string.h>
#include <stdatomic.h>
#include <girara/datastructures.h>
#include <girara/utils.h>

#include "adjustment.h"
#include "zatura.h"
#include "document.h"
#include "document-widget.h"
#include "page.h"
#include "page-widget.h"
#include "utils.h"
#include "internal.h"

/* Keep original pixels independently of recolor and display adjustments. */
#define RAW_CACHE_LIMIT ((size_t)128 * 1024 * 1024)
typedef struct {
  zathura_page_t* page;
  unsigned int width, height;
  double scale;
  zathura_device_factors_t factors;
  cairo_surface_t* surface;
  size_t bytes;
} RawPage;

static void raw_page_free(RawPage* entry) {
  cairo_surface_destroy(entry->surface);
  g_free(entry);
}

typedef struct {
  zathura_page_t* page;
  cairo_surface_t* surface;
  cairo_surface_t* thumbnail;
  size_t surface_bytes, thumbnail_bytes;
  bool visible;
} DisplayPixels;

/* private data for ZaturaRenderer */
typedef struct private_s {
  GThreadPool* pool;       /**< Pool of threads */
  girara_list_t* requests; /**< Render requests */
  GMutex mutex;            /**< Render lock */

  /**
   * Page cache
   */
  struct {
    int* cache;
    size_t size;
    size_t num_cached_pages;
  } page_cache;

  /**
   * Recolor information
   */
  struct {
    GdkRGBA light;
    GdkRGBA dark;
    bool enabled;
    bool hue;
    bool reverse_video;
    bool adjust_lightness;
  } recolor;

  GMutex raw_mutex;
  GQueue raw_pages;
  size_t raw_bytes;
  size_t cache_limit;
  GHashTable* display_pixels; /* Borrowed surface identities, protected by raw_mutex. */
  guint cache_trim_source;

  GMutex effects_mutex;
  PageEffects effects;
  atomic_uint effects_generation;

  atomic_bool about_to_close; /**< Render thread is to be freed */
} ZathuraRendererPrivate;

/* private data for ZaturaRenderRequest */
typedef struct request_private_s {
  ZathuraRenderer* renderer;
  zathura_page_t* page;
  gint64 last_view_time;
  girara_list_t* active_jobs;
  GMutex jobs_mutex;
  atomic_uint generation;
  unsigned int completed_effects_generation;
  bool render_plain;
} ZathuraRenderRequestPrivate;

/* define the two types */
G_DEFINE_TYPE_WITH_CODE(ZathuraRenderer, zathura_renderer, G_TYPE_OBJECT, G_ADD_PRIVATE(ZathuraRenderer))
G_DEFINE_TYPE_WITH_CODE(ZathuraRenderRequest, zathura_render_request, G_TYPE_OBJECT,
                        G_ADD_PRIVATE(ZathuraRenderRequest))

/* private methods for ZaturaRenderer  */
static void renderer_finalize(GObject* object);
/* private methods for ZaturaRenderRequest */
static void render_request_dispose(GObject* object);
static void render_request_finalize(GObject* object);

static void render_job(void* data, void* user_data);
static gint render_thread_sort(gconstpointer a, gconstpointer b, gpointer data);
static ssize_t page_cache_lru_invalidate(ZathuraRenderer* renderer);
static void page_cache_invalidate_all(ZathuraRenderer* renderer);
static bool page_cache_is_full(ZathuraRenderer* renderer, bool* result);

/* job description for render thread */
typedef struct render_job_s {
  ZathuraRenderRequest* request;
  atomic_uint generation;
  unsigned int effects_generation;
  PageEffects effects;
} render_job_t;

static bool render_job_is_stale(const render_job_t* job);

/* init, new and free for ZaturaRenderer */

static void zathura_renderer_class_init(ZathuraRendererClass* class) {
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = renderer_finalize;
}

static void zathura_renderer_init(ZathuraRenderer* renderer) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->pool                   = g_thread_pool_new(render_job, renderer, 1, TRUE, NULL);
  priv->about_to_close         = false;
  g_thread_pool_set_sort_function(priv->pool, render_thread_sort, NULL);
  g_mutex_init(&priv->mutex);
  g_mutex_init(&priv->effects_mutex);
  g_mutex_init(&priv->raw_mutex);
  g_queue_init(&priv->raw_pages);
  priv->cache_limit = (size_t)256 * 1024 * 1024;
  priv->display_pixels = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

  /* recolor */
  priv->recolor.enabled          = false;
  priv->recolor.hue              = true;
  priv->recolor.reverse_video    = false;
  priv->recolor.adjust_lightness = false;

  /* page cache */
  priv->page_cache.size             = 0;
  priv->page_cache.cache            = NULL;
  priv->page_cache.num_cached_pages = 0;

  zathura_renderer_set_recolor_colors_str(renderer, "#000000", "#FFFFFF");

  priv->requests = girara_list_new();
}

static bool page_cache_init(ZathuraRenderer* renderer, size_t cache_size) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);

  priv->page_cache.size  = cache_size;
  priv->page_cache.cache = g_try_malloc0(cache_size * sizeof(int));
  if (priv->page_cache.cache == NULL) {
    return false;
  }

  page_cache_invalidate_all(renderer);
  return true;
}

ZathuraRenderer* zathura_renderer_new(size_t cache_size) {
  g_return_val_if_fail(cache_size > 0, NULL);

  GObject* obj         = g_object_new(ZATHURA_TYPE_RENDERER, NULL);
  ZathuraRenderer* ret = ZATHURA_RENDERER(obj);

  if (page_cache_init(ret, cache_size) == false) {
    g_object_unref(obj);
    return NULL;
  }

  return ret;
}

static void renderer_finalize(GObject* object) {
  ZathuraRenderer* renderer    = ZATHURA_RENDERER(object);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);

  zathura_renderer_stop(renderer);
  g_mutex_clear(&(priv->mutex));
  g_mutex_clear(&priv->effects_mutex);
  g_queue_clear_full(&priv->raw_pages, (GDestroyNotify)raw_page_free);
  g_hash_table_unref(priv->display_pixels);
  g_mutex_clear(&priv->raw_mutex);

  g_free(priv->page_cache.cache);
  girara_list_free(priv->requests);
}

unsigned int zathura_renderer_get_effects_generation(ZathuraRenderer* renderer) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), 0);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->effects_generation;
}

PageEffects zathura_renderer_get_page_effects(ZathuraRenderer* renderer) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), ((PageEffects){0}));
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_lock(&priv->effects_mutex);
  const PageEffects effects = priv->effects;
  g_mutex_unlock(&priv->effects_mutex);
  return effects;
}

bool zathura_renderer_set_page_effects(ZathuraRenderer* renderer, const PageEffects* effects) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer) && page_effects_valid(effects), false);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_lock(&priv->effects_mutex);
  const bool changed = priv->effects.brightness != effects->brightness || priv->effects.contrast != effects->contrast ||
                       priv->effects.gamma != effects->gamma || priv->effects.saturation != effects->saturation;
  if (changed) {
    priv->effects = *effects;
    ++priv->effects_generation;
  }
  g_mutex_unlock(&priv->effects_mutex);
  return changed;
}

/* (un)register requests at the renderer */

static void renderer_unregister_request(ZathuraRenderer* renderer, ZathuraRenderRequest* request) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  girara_list_remove(priv->requests, request);
}

static void renderer_register_request(ZathuraRenderer* renderer, ZathuraRenderRequest* request) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (girara_list_contains(priv->requests, request) == false) {
    girara_list_append(priv->requests, request);
  }
}

/* init, new and free for ZaturaRenderRequest */

enum {
  REQUEST_COMPLETED,
  REQUEST_CACHE_ADDED,
  REQUEST_CACHE_INVALIDATED,
  REQUEST_LAST_SIGNAL,
};

static guint request_signals[REQUEST_LAST_SIGNAL] = {0};

static void zathura_render_request_class_init(ZathuraRenderRequestClass* class) {
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = render_request_dispose;
  object_class->finalize     = render_request_finalize;

  request_signals[REQUEST_COMPLETED] =
      g_signal_new("completed", ZATHURA_TYPE_RENDER_REQUEST, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                   g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_POINTER);

  request_signals[REQUEST_CACHE_ADDED] = g_signal_new("cache-added", ZATHURA_TYPE_RENDER_REQUEST, G_SIGNAL_RUN_LAST, 0,
                                                      NULL, NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  request_signals[REQUEST_CACHE_INVALIDATED] =
      g_signal_new("cache-invalidated", ZATHURA_TYPE_RENDER_REQUEST, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                   g_cclosure_marshal_generic, G_TYPE_NONE, 0);
}

static void zathura_render_request_init(ZathuraRenderRequest* request) {
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  priv->renderer                    = NULL;
  priv->page                        = NULL;
}

ZathuraRenderRequest* zathura_render_request_new(ZathuraRenderer* renderer, zathura_page_t* page) {
  g_return_val_if_fail(renderer != NULL && page != NULL, NULL);

  GObject* obj = g_object_new(ZATHURA_TYPE_RENDER_REQUEST, NULL);
  if (obj == NULL) {
    return NULL;
  }

  ZathuraRenderRequest* request     = ZATHURA_RENDER_REQUEST(obj);
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  /* we want to make sure that renderer lives long enough */
  priv->renderer    = g_object_ref(renderer);
  priv->page        = page;
  priv->active_jobs = girara_list_new();
  g_mutex_init(&priv->jobs_mutex);
  priv->generation   = 0;
  priv->render_plain = false;

  /* register the request with the renderer */
  renderer_register_request(renderer, request);

  return request;
}

static void render_request_dispose(GObject* object) {
  ZathuraRenderRequest* request     = ZATHURA_RENDER_REQUEST(object);
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);

  if (priv->renderer != NULL) {
    zathura_render_request_set_surfaces(request, NULL, NULL);
    /* unregister the request */
    renderer_unregister_request(priv->renderer, request);
    /* release our private reference to the renderer */
    g_clear_object(&priv->renderer);
  }

  G_OBJECT_CLASS(zathura_render_request_parent_class)->dispose(object);
}

static void render_request_finalize(GObject* object) {
  ZathuraRenderRequest* request     = ZATHURA_RENDER_REQUEST(object);
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);

  if (girara_list_size(priv->active_jobs) != 0) {
    girara_error("This should not happen!");
  }
  girara_list_free(priv->active_jobs);
  g_mutex_clear(&priv->jobs_mutex);

  G_OBJECT_CLASS(zathura_render_request_parent_class)->finalize(object);
}

/* renderer methods */

bool zathura_renderer_recolor_enabled(ZathuraRenderer* renderer) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.enabled;
}

void zathura_renderer_enable_recolor(ZathuraRenderer* renderer, bool enable) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->recolor.enabled        = enable;
}

bool zathura_renderer_recolor_hue_enabled(ZathuraRenderer* renderer) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.hue;
}

void zathura_renderer_enable_recolor_hue(ZathuraRenderer* renderer, bool enable) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->recolor.hue            = enable;
}

bool zathura_renderer_recolor_reverse_video_enabled(ZathuraRenderer* renderer) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.reverse_video;
}

void zathura_renderer_enable_recolor_reverse_video(ZathuraRenderer* renderer, bool enable) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->recolor.reverse_video  = enable;
}

bool zathura_renderer_recolor_adjust_lightness_enabled(ZathuraRenderer* renderer) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.adjust_lightness;
}

void zathura_renderer_enable_recolor_adjust_lightness(ZathuraRenderer* renderer, bool enable) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv   = zathura_renderer_get_instance_private(renderer);
  priv->recolor.adjust_lightness = enable;
}

void zathura_renderer_set_recolor_colors(ZathuraRenderer* renderer, const GdkRGBA* light, const GdkRGBA* dark) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (light != NULL) {
    priv->recolor.light = *light;
  }
  if (dark != NULL) {
    priv->recolor.dark = *dark;
  }
}

void zathura_renderer_set_recolor_colors_str(ZathuraRenderer* renderer, const char* light, const char* dark) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  if (dark != NULL) {
    GdkRGBA color;
    if (parse_color(&color, dark) == true) {
      zathura_renderer_set_recolor_colors(renderer, NULL, &color);
    }
  }
  if (light != NULL) {
    GdkRGBA color;
    if (parse_color(&color, light) == true) {
      zathura_renderer_set_recolor_colors(renderer, &color, NULL);
    }
  }
}

void zathura_renderer_get_recolor_colors(ZathuraRenderer* renderer, GdkRGBA* light, GdkRGBA* dark) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (light != NULL) {
    *light = priv->recolor.light;
  }
  if (dark != NULL) {
    *dark = priv->recolor.dark;
  }
}

void zathura_renderer_lock(ZathuraRenderer* renderer) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_lock(&priv->mutex);
}

void zathura_renderer_unlock(ZathuraRenderer* renderer) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_unlock(&priv->mutex);
}

void zathura_renderer_stop(ZathuraRenderer* renderer) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (priv->about_to_close == false) {
    girara_debug("Setting about-to-close flag for renderer");
  }
  priv->about_to_close = true;
  g_clear_handle_id(&priv->cache_trim_source, g_source_remove);

  if (priv->pool != NULL) {
    girara_debug("Waiting for thread pool to finish.");
    g_thread_pool_free(priv->pool, FALSE, TRUE);
    priv->pool = NULL;
  }
}

/* ZaturaRenderRequest methods */

void zathura_render_request(ZathuraRenderRequest* request, gint64 last_view_time) {
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  g_mutex_lock(&request_priv->jobs_mutex);

  bool unfinished_jobs = false;
  /* check if there are any active jobs left */
  for (size_t idx = 0; idx != girara_list_size(request_priv->active_jobs); ++idx) {
    render_job_t* job = girara_list_nth(request_priv->active_jobs, idx);
    if (!render_job_is_stale(job)) {
      unfinished_jobs = true;
      break;
    }
  }

  /* only add a new job if there are no active ones left */
  if (!unfinished_jobs) {
    if (!request_priv->renderer) {
      g_mutex_unlock(&request_priv->jobs_mutex);
      return;
    }

    ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(request_priv->renderer);
    if (priv->about_to_close || !priv->pool) {
      g_mutex_unlock(&request_priv->jobs_mutex);
      return;
    }

    request_priv->last_view_time = last_view_time;

    render_job_t* job = g_try_malloc0(sizeof(render_job_t));
    if (!job) {
      g_mutex_unlock(&request_priv->jobs_mutex);
      return;
    }

    job->request    = g_object_ref(request);
    job->generation = request_priv->generation;
    g_mutex_lock(&priv->effects_mutex);
    job->effects = priv->effects;
    job->effects_generation = priv->effects_generation;
    g_mutex_unlock(&priv->effects_mutex);
    girara_list_append(request_priv->active_jobs, job);

    g_thread_pool_push(priv->pool, job, NULL);
  }

  g_mutex_unlock(&request_priv->jobs_mutex);
}

void zathura_render_request_abort(ZathuraRenderRequest* request) {
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  ++request_priv->generation;
}

void zathura_render_request_update_view_time(ZathuraRenderRequest* request) {
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  request_priv->last_view_time              = g_get_real_time();
}

/* render job */

static bool render_job_is_stale(const render_job_t* job) {
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(job->request);

  /* Effect changes must not starve presentation during keyboard repeat.
   * Geometry changes and explicit aborts still invalidate work immediately. */
  return job->generation != request_priv->generation;
}

unsigned int zathura_render_request_get_completed_effects_generation(ZathuraRenderRequest* request) {
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  return priv->completed_effects_generation;
}

static void remove_job_and_free(render_job_t* job) {
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(job->request);

  g_mutex_lock(&request_priv->jobs_mutex);
  girara_list_remove(request_priv->active_jobs, job);
  g_mutex_unlock(&request_priv->jobs_mutex);

  g_object_unref(job->request);
  g_free(job);
}

typedef struct emit_completed_signal_s {
  render_job_t* job;
  cairo_surface_t* surface;
} emit_completed_signal_t;

static gboolean emit_completed_signal(void* data) {
  emit_completed_signal_t* ecs              = data;
  render_job_t* job                         = ecs->job;
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(job->request);
  ZathuraRendererPrivate* priv              = zathura_renderer_get_instance_private(request_priv->renderer);

  const bool valid = !priv->about_to_close && !render_job_is_stale(job);
  const bool refresh = valid && !request_priv->render_plain &&
      job->effects_generation != priv->effects_generation;
  ZathuraRenderRequest* request = g_object_ref(job->request);
  if (valid) {
    request_priv->completed_effects_generation = job->effects_generation;
    /* Present completed intermediate frames while newer settings accumulate. */
    girara_debug("Emitting signal for page %u", zathura_page_get_index(request_priv->page) + 1);
    g_signal_emit(job->request, request_signals[REQUEST_COMPLETED], 0, ecs->surface);
  } else {
    girara_debug("Discarding completed render after invalidation");
  }
  /* mark the request as done */
  remove_job_and_free(job);
  if (refresh) {
    zathura_render_request(request, g_get_real_time());
  }
  g_object_unref(request);

  /* clean up the data */
  cairo_surface_destroy(ecs->surface);
  g_free(ecs);

  return FALSE;
}

static bool recolor(ZathuraRendererPrivate* priv, zathura_page_t* page,
                     cairo_surface_t* surface, zathura_device_factors_t factors,
                     bool (*cancelled)(void*), void* context) {
  const GdkRGBA a = priv->recolor.dark, b = priv->recolor.light;
  const PageRecolor options = {.dark = {a.red, a.green, a.blue, a.alpha},
      .light = {b.red, b.green, b.blue, b.alpha}, .hue = priv->recolor.hue,
      .reverse_video = priv->recolor.reverse_video, .adjust_lightness = priv->recolor.adjust_lightness};
  g_autoptr(GArray) rectangles = g_array_new(FALSE, FALSE, sizeof(PageRecolorRect));
  if (options.reverse_video) {
    g_autoptr(girara_list_t) images = zathura_page_images_get(page, NULL);
    for (size_t i = 0; images && i < girara_list_size(images); ++i) {
      const zathura_image_t* image = girara_list_nth(images, i);
      const zathura_rectangle_t r = recalc_rectangle(page, image->position);
      const PageRecolorRect rect = {r.x1 * factors.x, r.y1 * factors.y, r.x2 * factors.x, r.y2 * factors.y};
      g_array_append_val(rectangles, rect);
    }
  }
  return page_recolor_apply_cancellable(surface, &options, (const PageRecolorRect*)rectangles->data,
                                         rectangles->len, cancelled, context);
}

static bool effect_job_cancelled(void* data) {
  render_job_t* job = data;
  ZathuraRenderRequestPrivate* request = zathura_render_request_get_instance_private(job->request);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(request->renderer);
  return priv->about_to_close || render_job_is_stale(job);
}

static bool postprocess_surface(ZathuraRendererPrivate* priv, zathura_page_t* page, unsigned int UNUSED(width),
                                unsigned int UNUSED(height), cairo_surface_t* surface, zathura_device_factors_t factors,
                                const PageEffects* effects, render_job_t* job) {
  if (priv->recolor.enabled) {
    if (!recolor(priv, page, surface, factors, job ? effect_job_cancelled : NULL, job)) { return false; }
  }
  return page_effects_apply_cancellable(surface, effects, job ? effect_job_cancelled : NULL, job);
}

static bool invoke_completed_signal(render_job_t* job, cairo_surface_t* surface) {
  emit_completed_signal_t* ecs = g_try_malloc0(sizeof(emit_completed_signal_t));
  if (ecs == NULL) {
    return false;
  }

  ecs->job     = job;
  ecs->surface = cairo_surface_reference(surface);

  /* emit signal from the main context, i.e. the main thread */
  g_main_context_invoke(NULL, emit_completed_signal, ecs);
  return true;
}

static bool render_to_cairo_surface(cairo_surface_t* surface, zathura_page_t* page, ZathuraRenderer* renderer,
                                    double real_scale) {
  cairo_t* cairo = cairo_create(surface);
  if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
    return false;
  }

  cairo_save(cairo);
  cairo_set_source_rgb(cairo, 1, 1, 1);
  cairo_paint(cairo);
  cairo_restore(cairo);

  /* apply scale (used by e.g. Poppler as pixels per point) */
  if (fabs(real_scale - 1.0f) > FLT_EPSILON) {
    cairo_scale(cairo, real_scale, real_scale);
  }

  zathura_renderer_lock(renderer);
  const int err = zathura_page_render(page, cairo, false);
  zathura_renderer_unlock(renderer);
  cairo_destroy(cairo);

  return err == ZATHURA_ERROR_OK;
}

static size_t cache_usage_locked(ZathuraRendererPrivate* priv, size_t* displayed) {
  g_autoptr(GHashTable) seen = g_hash_table_new(g_direct_hash, g_direct_equal);
  size_t total = 0, display_bytes = 0;
  GHashTableIter iterator; gpointer value;
  g_hash_table_iter_init(&iterator, priv->display_pixels);
  while (g_hash_table_iter_next(&iterator, NULL, &value)) {
    const DisplayPixels* pair = value;
    if (pair->surface && g_hash_table_add(seen, pair->surface)) { display_bytes += pair->surface_bytes; }
    if (pair->thumbnail && g_hash_table_add(seen, pair->thumbnail)) { display_bytes += pair->thumbnail_bytes; }
  }
  total = display_bytes;
  for (GList* link = priv->raw_pages.head; link; link = link->next) {
    const RawPage* raw = link->data;
    if (g_hash_table_add(seen, raw->surface)) { total += raw->bytes; }
  }
  if (displayed) { *displayed = display_bytes; }
  return total;
}

static bool raw_visible_locked(ZathuraRendererPrivate* priv, const RawPage* raw) {
  GHashTableIter iterator; gpointer value;
  g_hash_table_iter_init(&iterator, priv->display_pixels);
  while (g_hash_table_iter_next(&iterator, NULL, &value)) {
    const DisplayPixels* pair = value;
    if (pair->page == raw->page && pair->visible) { return true; }
  }
  return false;
}

static void trim_raw_locked(ZathuraRendererPrivate* priv) {
  while (priv->raw_bytes > MIN(RAW_CACHE_LIMIT, priv->cache_limit) ||
         cache_usage_locked(priv, NULL) > priv->cache_limit) {
    GList* candidate = priv->raw_pages.tail;
    while (candidate && raw_visible_locked(priv, candidate->data)) { candidate = candidate->prev; }
    if (!candidate) { break; } /* Visible originals stay reusable while holding adjustment keys. */
    RawPage* old = candidate->data;
    priv->raw_bytes -= old->bytes;
    g_queue_delete_link(&priv->raw_pages, candidate);
    raw_page_free(old);
  }
}

static gboolean trim_display_cache(gpointer data) {
  ZathuraRenderer* renderer = data;
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->cache_trim_source = 0;
  while (!priv->about_to_close) {
    g_mutex_lock(&priv->raw_mutex);
    trim_raw_locked(priv);
    const size_t bytes = cache_usage_locked(priv, NULL);
    ZathuraRenderRequest* oldest = NULL;
    gint64 oldest_time = G_MAXINT64;
    if (bytes > priv->cache_limit) {
      GHashTableIter iterator; gpointer key, value;
      g_hash_table_iter_init(&iterator, priv->display_pixels);
      while (g_hash_table_iter_next(&iterator, &key, &value)) {
        const DisplayPixels* pair = value;
        ZathuraRenderRequestPrivate* request = zathura_render_request_get_instance_private(key);
        if (!pair->visible && (pair->surface || pair->thumbnail) && request->last_view_time < oldest_time) {
          oldest = key; oldest_time = request->last_view_time;
        }
      }
    }
    if (oldest) { g_object_ref(oldest); }
    g_mutex_unlock(&priv->raw_mutex);
    if (!oldest) { break; }
    ZathuraRenderRequestPrivate* request = zathura_render_request_get_instance_private(oldest);
    const int page = zathura_page_get_index(request->page);
    for (size_t i = 0; i < priv->page_cache.num_cached_pages; ++i) {
      if (priv->page_cache.cache[i] == page) {
        memmove(priv->page_cache.cache + i, priv->page_cache.cache + i + 1,
                (priv->page_cache.num_cached_pages - i - 1) * sizeof(int));
        priv->page_cache.cache[--priv->page_cache.num_cached_pages] = -1;
        break;
      }
    }
    g_signal_emit(oldest, request_signals[REQUEST_CACHE_INVALIDATED], 0);
    g_object_unref(oldest);
  }
  return G_SOURCE_REMOVE;
}

static void schedule_cache_trim(ZathuraRenderer* renderer) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (!priv->cache_trim_source && !priv->about_to_close) {
    priv->cache_trim_source = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, trim_display_cache,
                                            g_object_ref(renderer), g_object_unref);
  }
}

void zathura_renderer_set_cache_limit(ZathuraRenderer* renderer, unsigned int mib) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_lock(&priv->raw_mutex);
  priv->cache_limit = (size_t)CLAMP(mib, 1, 16384) * 1024 * 1024;
  g_mutex_unlock(&priv->raw_mutex);
  schedule_cache_trim(renderer);
}

void zathura_renderer_get_cache_usage(ZathuraRenderer* renderer, size_t* raw, size_t* display, size_t* total) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_lock(&priv->raw_mutex);
  *raw = priv->raw_bytes;
  *total = cache_usage_locked(priv, display);
  g_mutex_unlock(&priv->raw_mutex);
}

void zathura_render_request_set_surfaces(ZathuraRenderRequest* request, cairo_surface_t* surface,
                                        cairo_surface_t* thumbnail) {
  ZathuraRenderRequestPrivate* rp = zathura_render_request_get_instance_private(request);
  if (!rp->renderer) { return; }
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(rp->renderer);
  DisplayPixels* pair = g_new0(DisplayPixels, 1);
  pair->page = rp->page; pair->surface = surface; pair->thumbnail = thumbnail;
  pair->visible = zathura_page_get_visibility(rp->page);
  if (surface) { pair->surface_bytes = (size_t)cairo_image_surface_get_stride(surface) * cairo_image_surface_get_height(surface); }
  if (thumbnail) { pair->thumbnail_bytes = (size_t)cairo_image_surface_get_stride(thumbnail) * cairo_image_surface_get_height(thumbnail); }
  g_mutex_lock(&priv->raw_mutex);
  if (surface || thumbnail) { g_hash_table_replace(priv->display_pixels, request, pair); }
  else { g_hash_table_remove(priv->display_pixels, request); g_free(pair); }
  g_mutex_unlock(&priv->raw_mutex);
  schedule_cache_trim(rp->renderer);
}

void zathura_render_request_set_visible(ZathuraRenderRequest* request, bool visible) {
  ZathuraRenderRequestPrivate* rp = zathura_render_request_get_instance_private(request);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(rp->renderer);
  g_mutex_lock(&priv->raw_mutex);
  DisplayPixels* pair = g_hash_table_lookup(priv->display_pixels, request);
  if (pair) { pair->visible = visible; }
  g_mutex_unlock(&priv->raw_mutex);
  schedule_cache_trim(rp->renderer);
}

static cairo_surface_t* original_surface(ZathuraRenderer* renderer, zathura_page_t* page, unsigned int width,
                                         unsigned int height, double scale, zathura_device_factors_t factors,
                                         bool plain) {
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (!plain) {
    g_mutex_lock(&priv->raw_mutex);
    for (GList* link = priv->raw_pages.head; link; link = link->next) {
      RawPage* entry = link->data;
      if (entry->page == page && entry->width == width && entry->height == height && entry->scale == scale &&
          entry->factors.x == factors.x && entry->factors.y == factors.y) {
        cairo_surface_t* surface = cairo_surface_reference(entry->surface);
        g_queue_unlink(&priv->raw_pages, link);
        g_queue_push_head_link(&priv->raw_pages, link);
        g_mutex_unlock(&priv->raw_mutex);
        girara_debug("Reusing original pixels for page %u", zathura_page_get_index(page) + 1);
        return surface;
      }
    }
    g_mutex_unlock(&priv->raw_mutex);
  }
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  if (!plain) {
    cairo_surface_set_device_scale(surface, factors.x, factors.y);
  }
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS || !render_to_cairo_surface(surface, page, renderer, scale)) {
    cairo_surface_destroy(surface);
    return NULL;
  }
  const size_t bytes = (size_t)cairo_image_surface_get_stride(surface) * height;
  if (!plain) {
    RawPage* entry = g_new0(RawPage, 1);
    *entry = (RawPage){page, width, height, scale, factors, cairo_surface_reference(surface), bytes};
    g_mutex_lock(&priv->raw_mutex);
    /* A synchronous first render and a worker can finish the same page. Keep only one size per page. */
    for (GList* link = priv->raw_pages.head; link;) {
      GList* next = link->next;
      RawPage* old = link->data;
      if (old->page == page) {
        priv->raw_bytes -= old->bytes;
        g_queue_delete_link(&priv->raw_pages, link);
        raw_page_free(old);
      }
      link = next;
    }
    g_queue_push_head(&priv->raw_pages, entry);
    priv->raw_bytes += bytes;
    trim_raw_locked(priv);
    g_mutex_unlock(&priv->raw_mutex);
  }
  return surface;
}

/* Consume the caller's raw reference, cloning only when a filter will modify pixels. */
static cairo_surface_t* adjusted_surface(ZathuraRendererPrivate* priv, zathura_page_t* page, unsigned int width,
                                         unsigned int height, cairo_surface_t* raw, zathura_device_factors_t factors,
                                         const PageEffects* effects, render_job_t* job) {
  if (!priv->recolor.enabled && effects->brightness == 0 && effects->contrast == 0 && effects->gamma == 0 &&
      effects->saturation == 0) {
    return raw;
  }
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_surface_set_device_scale(surface, factors.x, factors.y);
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(raw);
    cairo_surface_destroy(surface);
    return NULL;
  }
  cairo_surface_flush(raw);
  const unsigned char* source = cairo_image_surface_get_data(raw);
  unsigned char* dest = cairo_image_surface_get_data(surface);
  const int source_stride = cairo_image_surface_get_stride(raw);
  const int dest_stride = cairo_image_surface_get_stride(surface);
  for (unsigned int y = 0; y < height; ++y) {
    memcpy(dest + (size_t)y * dest_stride, source + (size_t)y * source_stride, (size_t)width * 4);
  }
  cairo_surface_mark_dirty(surface);
  cairo_surface_destroy(raw);
  if (!postprocess_surface(priv, page, width, height, surface, factors, effects, job)) {
    cairo_surface_destroy(surface);
    return NULL;
  }
  return surface;
}

static bool render(render_job_t* job, ZathuraRenderRequest* request, ZathuraRenderer* renderer) {
  ZathuraRendererPrivate* priv              = zathura_renderer_get_instance_private(renderer);
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  zathura_page_t* page                      = request_priv->page;

  /* parse the page on first render */
  if (zathura_renderer_load_page(renderer, page) == false) {
    return false;
  }

  /* create cairo surface */
  unsigned int page_width  = 0;
  unsigned int page_height = 0;

  /* page size in points */
  zathura_document_t* document = zathura_page_get_document(page);
  const double height          = zathura_page_get_height(page);
  const double width           = zathura_page_get_width(page);

  zathura_device_factors_t device_factors = {0};
  double real_scale                       = 1;
  if (request_priv->render_plain == false) {
    /* page size in user pixels based on document zoom: if PPI information is
     * correct, 100% zoom will result in 72 documents points per inch of screen
     * (i.e. document size on screen matching the physical paper size). */
    real_scale = page_calc_height_width(document, page, &page_height, &page_width, false);

    device_factors = zathura_document_get_device_factors(document);
    page_width *= device_factors.x;
    page_height *= device_factors.y;
  } else {
    page_width  = width;
    page_height = height;
  }

  cairo_surface_t* surface = original_surface(renderer, page, page_width, page_height, real_scale, device_factors,
                                               request_priv->render_plain);
  if (!surface) {
    return false;
  }

  /* before recoloring, check if we've been aborted */
  if (priv->about_to_close || render_job_is_stale(job)) {
    girara_debug("Rendering of page %u aborted", zathura_page_get_index(request_priv->page) + 1);
    remove_job_and_free(job);
    cairo_surface_destroy(surface);
    return true;
  }

  if (!request_priv->render_plain) {
    surface = adjusted_surface(priv, page, page_width, page_height, surface, device_factors, &job->effects, job);
    if (!surface) {
      if (effect_job_cancelled(job)) {
        remove_job_and_free(job);
        return true;
      }
      return false;
    }
  }

  if (!invoke_completed_signal(job, surface)) {
    cairo_surface_destroy(surface);
    return false;
  }

  cairo_surface_destroy(surface);

  return true;
}

bool zathura_renderer_load_page(ZathuraRenderer* renderer, zathura_page_t* page) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer) == TRUE, false);

  /* zatura_page_load takes the document lock internally */
  return zathura_page_load(page, NULL);
}

/* render a page synchronously and return its surface */
cairo_surface_t* zathura_renderer_render_page(ZathuraRenderer* renderer, zathura_page_t* page) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), NULL);
  g_return_val_if_fail(page != NULL, NULL);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  zathura_document_t* document = zathura_page_get_document(page);

  /* parse the page on first render */
  if (zathura_renderer_load_page(renderer, page) == false) {
    return NULL;
  }

  unsigned int page_width = 0, page_height = 0;
  const double real_scale = page_calc_height_width(document, page, &page_height, &page_width, false);

  const zathura_device_factors_t device_factors = zathura_document_get_device_factors(document);
  page_width *= device_factors.x;
  page_height *= device_factors.y;

  cairo_surface_t* surface = original_surface(renderer, page, page_width, page_height, real_scale, device_factors, false);
  if (!surface) {
    return NULL;
  }
  const PageEffects effects = zathura_renderer_get_page_effects(renderer);
  surface = adjusted_surface(priv, page, page_width, page_height, surface, device_factors, &effects, NULL);

  return surface;
}

static void render_job(void* data, void* user_data) {
  render_job_t* job             = data;
  ZathuraRenderRequest* request = job->request;
  ZathuraRenderer* renderer     = user_data;

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (priv->about_to_close || render_job_is_stale(job)) {
    /* back out early */
    remove_job_and_free(job);
    return;
  }

  /* Collapse all changes that arrived while this job waited in the queue. */
  g_mutex_lock(&priv->effects_mutex);
  job->effects = priv->effects;
  job->effects_generation = priv->effects_generation;
  g_mutex_unlock(&priv->effects_mutex);

  ZathuraRenderRequestPrivate* request_private = zathura_render_request_get_instance_private(request);
  const unsigned int page_index                = zathura_page_get_index(request_private->page);
  girara_debug("Rendering page %u ...", page_index + 1);
  if (render(job, request, renderer) != true) {
    girara_error("Rendering failed (page %u)\n", page_index + 1);
    remove_job_and_free(job);
  }
}

void render_all(zathura_t* zathura) {
  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL) {
    return;
  }

  zathura_document_widget_compute_layout(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget));

  /* unmark all pages */
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zathura_page_t* page     = zathura_document_get_page(document, page_id);
    unsigned int page_height = 0, page_width = 0;

    page_calc_height_width(document, page, &page_height, &page_width, true);

    girara_debug("Queuing resize for page %u to %u x %u.", page_id, page_width, page_height);
    GtkWidget* widget = zathura_page_get_widget(zathura, page);
    if (widget != NULL) {
      zathura_page_widget_set_size_request(ZATHURA_PAGE_WIDGET(widget), page_width, page_height);
      gtk_widget_queue_resize(widget);
    }
  }
}

static gint render_thread_sort(gconstpointer a, gconstpointer b, gpointer UNUSED(data)) {
  if (a == NULL || b == NULL) {
    return 0;
  }

  const render_job_t* job_a = a;
  const render_job_t* job_b = b;

  const bool job_a_stale = render_job_is_stale(job_a);
  const bool job_b_stale = render_job_is_stale(job_b);
  // sort stale jobs first so that they are thrown out
  if (job_a_stale && job_b_stale) {
    // both jobs are stale, so the order does not matter
    return 0;
  } else if (job_a_stale) {
    return -1;
  } else if (job_b_stale) {
    return 1;
  }

  ZathuraRenderRequestPrivate* priv_a = zathura_render_request_get_instance_private(job_a->request);
  ZathuraRenderRequestPrivate* priv_b = zathura_render_request_get_instance_private(job_b->request);

  // handle jobs with more recent view time first
  return priv_a->last_view_time < priv_b->last_view_time ? 1
                                                         : (priv_a->last_view_time > priv_b->last_view_time ? -1 : 0);
}

/* cache functions */

static bool page_cache_is_cached(ZathuraRenderer* renderer, unsigned int page_index) {
  g_return_val_if_fail(renderer != NULL, false);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);

  if (priv->page_cache.num_cached_pages != 0) {
    for (size_t i = 0; i < priv->page_cache.size; ++i) {
      if (priv->page_cache.cache[i] >= 0 && page_index == (unsigned int)priv->page_cache.cache[i]) {
        girara_debug("Page %d is a cache hit", page_index + 1);
        return true;
      }
    }
  }

  girara_debug("Page %d is a cache miss", page_index + 1);
  return false;
}

static int find_request_by_page_index(const void* req, const void* data) {
  ZathuraRenderRequest* request = (void*)req;
  const unsigned int page_index = *((const int*)data);

  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  if (zathura_page_get_index(priv->page) == page_index) {
    return 0;
  }
  return 1;
}

static ssize_t page_cache_lru_invalidate(ZathuraRenderer* renderer) {
  g_return_val_if_fail(renderer != NULL, -1);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_return_val_if_fail(priv->page_cache.size != 0, -1);

  ssize_t lru_index             = 0;
  gint64 lru_view_time          = G_MAXINT64;
  ZathuraRenderRequest* request = NULL;
  for (size_t i = 0; i < priv->page_cache.size; ++i) {
    ZathuraRenderRequest* tmp_request =
        girara_list_find(priv->requests, find_request_by_page_index, &priv->page_cache.cache[i]);
    g_return_val_if_fail(tmp_request != NULL, -1);
    ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(tmp_request);

    if (request_priv->last_view_time < lru_view_time) {
      lru_view_time = request_priv->last_view_time;
      lru_index     = i;
      request       = tmp_request;
    }
  }

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);

  /* emit the signal */
  g_signal_emit(request, request_signals[REQUEST_CACHE_INVALIDATED], 0);
  girara_debug("Invalidated page %u at cache index %zd", zathura_page_get_index(request_priv->page) + 1, lru_index);
  priv->page_cache.cache[lru_index] = -1;
  --priv->page_cache.num_cached_pages;

  return lru_index;
}

static bool page_cache_is_full(ZathuraRenderer* renderer, bool* result) {
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer) && result != NULL, false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  *result                      = priv->page_cache.num_cached_pages == priv->page_cache.size;

  return true;
}

static void page_cache_invalidate_all(ZathuraRenderer* renderer) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  for (size_t i = 0; i < priv->page_cache.size; ++i) {
    priv->page_cache.cache[i] = -1;
  }
  priv->page_cache.num_cached_pages = 0;
}

void zathura_renderer_page_cache_add(ZathuraRenderer* renderer, unsigned int page_index) {
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));
  if (page_cache_is_cached(renderer, page_index) == true) {
    return;
  }

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  bool full                    = false;
  if (page_cache_is_full(renderer, &full) == false) {
    return;
  } else if (full == true) {
    const ssize_t idx = page_cache_lru_invalidate(renderer);
    if (idx == -1) {
      return;
    }

    priv->page_cache.cache[idx] = page_index;
    ++priv->page_cache.num_cached_pages;
    girara_debug("Page %d is cached at cache index %zd", page_index + 1, idx);
  } else {
    priv->page_cache.cache[priv->page_cache.num_cached_pages++] = page_index;
    girara_debug("Page %d is cached at cache index %zu", page_index + 1, priv->page_cache.num_cached_pages - 1);
  }

  ZathuraRenderRequest* request = girara_list_find(priv->requests, find_request_by_page_index, &page_index);
  g_return_if_fail(request != NULL);
  g_signal_emit(request, request_signals[REQUEST_CACHE_ADDED], 0);
}

void zathura_render_request_set_render_plain(ZathuraRenderRequest* request, bool render_plain) {
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  priv->render_plain                = render_plain;
}

bool zathura_render_request_get_render_plain(ZathuraRenderRequest* request) {
  g_return_val_if_fail(ZATHURA_IS_RENDER_REQUEST(request), false);

  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  return priv->render_plain;
}
