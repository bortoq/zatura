/* SPDX-License-Identifier: Zlib */

#include "document.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <glib.h>
#include <gio/gio.h>
#include <math.h>
#include <xxhash.h>

#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>

#include "zatura.h"
#include "page.h"
#include "plugin.h"
#include "content-type.h"
#include "internal.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XXH3_state_t, XXH3_freeState)

/**
 * Document
 */
struct zatura_document_s {
  void* data;                              /**< Custom data */
  zatura_page_t** pages;                  /**< All pages of the document */
  GMutex lock;                             /**< Document lock */
  const zatura_plugin_t* plugin;          /**< Used plugin */
  char* file_path;                         /**< File path of the document */
  char* uri;                               /**< URI of the document */
  char* basename;                          /**< Basename of the document */
  const char* password;                    /**< Password of the document */
  XXH128_canonical_t hash;                 /**< XXH3 hash of the document */
  unsigned int current_page_number;        /**< Current page number */
  unsigned int number_of_pages;            /**< Number of pages */
  double zoom;                             /**< Zoom value */
  unsigned int rotate;                     /**< Rotation */
  zatura_adjust_mode_t adjust_mode;       /**< Adjust mode (best-fit, width) */
  int page_offset;                         /**< Page offset */
  unsigned int view_width;                 /**< width of current viewport */
  unsigned int view_height;                /**< height of current viewport */
  double view_ppi;                         /**< PPI of the current viewport */
  zatura_device_factors_t device_factors; /**< x and y device scale factors (for e.g. HiDPI) */
  double position_x;                       /**< X adjustment */
  double position_y;                       /**< Y adjustment */
  bool hash_computed;                      /**< Whether the hash has been computed yet */
};

static bool hash_file(XXH128_canonical_t* dst, const char* path) {
  g_autoptr(GFile) f = g_file_new_for_path(path);
  if (!f) {
    return false;
  }

  g_autoptr(GFileInputStream) stream = g_file_read(f, NULL, NULL);
  if (!stream) {
    return false;
  }

  g_autoptr(XXH3_state_t) state = XXH3_createState();
  if (!state) {
    return false;
  }
  XXH3_128bits_reset(state);

  uint8_t buf[MAX(BUFSIZ, 4096)];
  gssize read;
  while ((read = g_input_stream_read(G_INPUT_STREAM(stream), buf, sizeof(buf), NULL, NULL)) > 0) {
    XXH3_128bits_update(state, buf, read);
  }

  /* read is zero on a clean end of stream and negative on an error */
  if (read < 0) {
    return false;
  }

  XXH128_canonicalFromHash(dst, XXH3_128bits_digest(state));
  return true;
}

zatura_document_t* zatura_document_open(zatura_t* zatura, const char* path, const char* uri, const char* password,
                                          zatura_error_t* error) {
  if (zatura == NULL || path == NULL) {
    return NULL;
  }

  g_autoptr(GFile) file = g_file_new_for_path(path);
  if (file == NULL) {
    girara_error("Error while handling path '%s'.", path);
    zatura_check_set_error(error, ZATURA_ERROR_UNKNOWN);
    return NULL;
  }

  g_autofree char* real_path = g_file_get_path(file);
  if (real_path == NULL) {
    girara_error("Error while handling path '%s'.", path);
    zatura_check_set_error(error, ZATURA_ERROR_UNKNOWN);
    return NULL;
  }

  g_autofree char* content_type = zatura_content_type_guess(
      zatura->content_type_context, real_path, zatura_plugin_manager_get_content_types(zatura->plugins.manager));
  if (content_type == NULL) {
    girara_error("Could not determine file type.");
    zatura_check_set_error(error, ZATURA_ERROR_UNKNOWN);
    return NULL;
  }

  const zatura_plugin_t* plugin = zatura_plugin_manager_get_plugin(zatura->plugins.manager, content_type);
  if (plugin == NULL) {
    girara_error("Unknown file type: '%s'", content_type);
    zatura_check_set_error(error, ZATURA_ERROR_UNKNOWN);
    return NULL;
  }

  g_autoptr(zatura_document_t) document = g_try_malloc0(sizeof(zatura_document_t));
  if (document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  g_mutex_init(&document->lock);

  document->file_path = g_steal_pointer(&real_path);
  document->uri       = g_strdup(uri);
  if (document->uri == NULL) {
    document->basename = g_file_get_basename(file);
  } else {
    g_autoptr(GFile) gf = g_file_new_for_uri(document->uri);
    document->basename  = g_file_get_basename(gf);
  }
  document->password         = password;
  document->zoom             = 1.0;
  document->plugin           = plugin;
  document->adjust_mode      = ZATURA_ADJUST_NONE;
  document->view_height      = 0;
  document->view_width       = 0;
  document->view_ppi         = 0.0;
  document->device_factors.x = 1.0;
  document->device_factors.y = 1.0;
  document->position_x       = 0.0;
  document->position_y       = 0.0;

  /* open document */
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);

  zatura_error_t int_error = functions->document_open(document);
  if (int_error != ZATURA_ERROR_OK) {
    zatura_check_set_error(error, int_error);
    girara_error("could not open document\n");
    return NULL;
  }

  /* allocate the pages without parsing them */
  document->pages = g_try_malloc0_n(document->number_of_pages, sizeof(zatura_page_t*));
  if (document->pages == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zatura_page_t* page = zatura_page_new(document, page_id, NULL);
    if (page == NULL) {
      zatura_check_set_error(error, ZATURA_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    document->pages[page_id] = page;
  }

  return g_steal_pointer(&document);
}

zatura_error_t zatura_document_free(zatura_document_t* document) {
  if (!document || !document->plugin) {
    g_free(document);
    return ZATURA_ERROR_INVALID_ARGUMENTS;
  }

  if (document->pages) {
    /* free pages */
    for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
      zatura_page_free(document->pages[page_id]);
      document->pages[page_id] = NULL;
    }
    g_free(document->pages);
  }

  /* free document */
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(document->plugin);

  zatura_error_t error = functions->document_free(document, document->data);

  g_free(document->file_path);
  g_free(document->uri);
  g_free(document->basename);
  g_mutex_clear(&document->lock);
  g_free(document);

  return error;
}

const char* zatura_document_get_path(zatura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->file_path;
}

const uint8_t* zatura_document_get_hash(zatura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  if (!document->hash_computed) {
    if (!hash_file(&document->hash, document->file_path)) {
      girara_warning("Failed to hash file '%s'; fileinfo lookup may be unreliable.", document->file_path);
    }
    document->hash_computed = true;
  }

  return document->hash.digest;
}

const char* zatura_document_get_uri(zatura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->uri;
}

const char* zatura_document_get_basename(zatura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->basename;
}

const char* zatura_document_get_password(zatura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->password;
}

zatura_page_t* zatura_document_get_page(zatura_document_t* document, unsigned int index) {
  if (document == NULL || document->pages == NULL) {
    return NULL;
  }

  g_return_val_if_fail(index < document->number_of_pages, NULL);
  return document->pages[index];
}

void* zatura_document_get_data(zatura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->data;
}

void zatura_document_set_data(zatura_document_t* document, void* data) {
  if (document == NULL) {
    return;
  }

  document->data = data;
}

unsigned int zatura_document_get_number_of_pages(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->number_of_pages;
}

void zatura_document_set_number_of_pages(zatura_document_t* document, unsigned int number_of_pages) {
  if (document == NULL) {
    return;
  }

  document->number_of_pages = number_of_pages;
}

unsigned int zatura_document_get_current_page_number(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->current_page_number;
}

void zatura_document_set_current_page_number(zatura_document_t* document, unsigned int current_page) {
  if (document == NULL) {
    return;
  }

  document->current_page_number = current_page;
}

double zatura_document_get_position_x(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->position_x;
}

double zatura_document_get_position_y(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->position_y;
}

void zatura_document_set_position_x(zatura_document_t* document, double position_x) {
  if (document == NULL) {
    return;
  }

  document->position_x = position_x;
}

void zatura_document_set_position_y(zatura_document_t* document, double position_y) {
  if (document == NULL) {
    return;
  }

  document->position_y = position_y;
}

double zatura_document_get_zoom(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->zoom;
}

void zatura_document_set_zoom(zatura_document_t* document, double zoom) {
  if (document == NULL) {
    return;
  }

  document->zoom = zoom;
}

double zatura_document_get_scale(zatura_document_t* document) {
  if (!document) {
    return 0;
  }

  double ppi = document->view_ppi;
  if (ppi < DBL_EPSILON) {
    /* No PPI information -> use a typical value */
    ppi = 100;
  }

  /* scale = pixels per point, and there are 72 points in one inch */
  return document->zoom * ppi / 72.0;
}

unsigned int zatura_document_get_rotation(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->rotate;
}

void zatura_document_set_rotation(zatura_document_t* document, unsigned int rotation) {
  if (!document) {
    return;
  }

  rotation = rotation % 360;
  if (rotation == 0 || rotation > 270) {
    document->rotate = 0;
  } else if (rotation <= 90) {
    document->rotate = 90;
  } else if (rotation <= 180) {
    document->rotate = 180;
  } else {
    document->rotate = 270;
  }
}

zatura_adjust_mode_t zatura_document_get_adjust_mode(zatura_document_t* document) {
  if (document == NULL) {
    return ZATURA_ADJUST_NONE;
  }

  return document->adjust_mode;
}

void zatura_document_set_adjust_mode(zatura_document_t* document, zatura_adjust_mode_t mode) {
  if (document == NULL) {
    return;
  }

  document->adjust_mode = mode;
}

int zatura_document_get_page_offset(zatura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->page_offset;
}

void zatura_document_set_page_offset(zatura_document_t* document, unsigned int page_offset) {
  if (document == NULL) {
    return;
  }

  document->page_offset = page_offset;
}

void zatura_document_set_viewport_width(zatura_document_t* document, unsigned int width) {
  if (document == NULL) {
    return;
  }
  document->view_width = width;
}

void zatura_document_set_viewport_height(zatura_document_t* document, unsigned int height) {
  if (document == NULL) {
    return;
  }
  document->view_height = height;
}

void zatura_document_set_viewport_ppi(zatura_document_t* document, double ppi) {
  if (document == NULL) {
    return;
  }
  document->view_ppi = ppi;
}

void zatura_document_get_viewport_size(zatura_document_t* document, unsigned int* height, unsigned int* width) {
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  *height = document->view_height;
  *width  = document->view_width;
}

double zatura_document_get_viewport_ppi(zatura_document_t* document) {
  if (document == NULL) {
    return 0.0;
  }
  return document->view_ppi;
}

void zatura_document_set_device_factors(zatura_document_t* document, double x_factor, double y_factor) {
  if (!document) {
    return;
  }

  if (fabs(x_factor) < DBL_EPSILON || fabs(y_factor) < DBL_EPSILON) {
    girara_debug("Ignoring new device factors %0.2f and %0.2f: too small", x_factor, y_factor);
    return;
  }

  document->device_factors.x = x_factor;
  document->device_factors.y = y_factor;
}

zatura_device_factors_t zatura_document_get_device_factors(zatura_document_t* document) {
  if (document == NULL) {
    /* The function is guaranteed to not return zero values */
    return (zatura_device_factors_t){1.0, 1.0};
  }

  return document->device_factors;
}

zatura_error_t zatura_document_save_as(zatura_document_t* document, const char* path) {
  if (document == NULL || document->plugin == NULL || path == NULL) {
    return ZATURA_ERROR_UNKNOWN;
  }

  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(document->plugin);
  if (functions->document_save_as == NULL) {
    return ZATURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_save_as(document, document->data, path);
}

girara_tree_node_t* zatura_document_index_generate(zatura_document_t* document, zatura_error_t* error) {
  if (document == NULL || document->plugin == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(document->plugin);
  if (functions->document_index_generate == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->document_index_generate(document, document->data, error);
}

girara_list_t* zatura_document_attachments_get(zatura_document_t* document, zatura_error_t* error) {
  if (document == NULL || document->plugin == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(document->plugin);
  if (functions->document_attachments_get == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->document_attachments_get(document, document->data, error);
}

zatura_error_t zatura_document_attachment_save(zatura_document_t* document, const char* attachment,
                                                 const char* file) {
  if (document == NULL || document->plugin == NULL) {
    return ZATURA_ERROR_INVALID_ARGUMENTS;
  }

  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(document->plugin);
  if (functions->document_attachment_save == NULL) {
    return ZATURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_attachment_save(document, document->data, attachment, file);
}

girara_list_t* zatura_document_get_information(zatura_document_t* document, zatura_error_t* error) {
  if (document == NULL || document->plugin == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(document->plugin);
  if (functions->document_get_information == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  girara_list_t* result = functions->document_get_information(document, document->data, error);
  if (result != NULL) {
    girara_list_set_free_function(result, zatura_document_information_entry_free);
  }

  return result;
}

const zatura_plugin_t* zatura_document_get_plugin(zatura_document_t* document) {
  g_return_val_if_fail(document != NULL, NULL);

  return document->plugin;
}

void zatura_document_lock(zatura_document_t* document) {
  g_return_if_fail(document != NULL);

  g_mutex_lock(&document->lock);
}

void zatura_document_unlock(zatura_document_t* document) {
  g_return_if_fail(document != NULL);

  g_mutex_unlock(&document->lock);
}
