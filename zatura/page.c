/* SPDX-License-Identifier: Zlib */

#include "page.h"

#include <math.h>
#include <girara-gtk/session.h>
#include <girara/utils.h>
#include <glib/gi18n.h>

#include "document.h"
#include "plugin.h"
#include "utils.h"
#include "internal.h"
#include "types.h"

struct zatura_page_s {
  zatura_document_t* document; /**< Parent document */
  void* data;                   /**< Custom data */
  char* label;                  /**< Page label */
  double height;                /**< Page height */
  double width;                 /**< Page width */
  double zoom;                  /**< Page zoom */
  unsigned int index;           /**< Page number */
  bool visible;                 /**< Page is visible */
  bool label_is_number;         /**< Page label is the same as the page number */
  gint loaded;                  /**< Page has been parsed by the plugin, atomic */
};

zatura_page_t* zatura_page_new(zatura_document_t* document, unsigned int index, zatura_error_t* error) {
  if (document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  /* init page */
  g_autoptr(zatura_page_t) page = g_try_malloc0(sizeof(zatura_page_t));
  if (page == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  page->index           = index;
  page->visible         = false;
  page->document        = document;
  page->label_is_number = false;
  page->zoom            = 1.0;
  page->loaded          = 0;

  /* the page is parsed later when it is used, not here */
  return g_steal_pointer(&page);
}

bool zatura_page_load(zatura_page_t* page, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return false;
  }

  if (g_atomic_int_get(&page->loaded) == 1) {
    return true;
  }

  zatura_document_lock(page->document);

  /* another thread may have parsed the page while waiting for the lock */
  if (g_atomic_int_get(&page->loaded) == 1) {
    zatura_document_unlock(page->document);
    return true;
  }

  /* init plugin */
  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);

  zatura_error_t ret = functions->page_init(page);
  if (ret != ZATURA_ERROR_OK) {
    girara_error("Failed to initialize page %u: %d", page->index + 1, ret);
    zatura_check_set_error(error, ret);
    zatura_document_unlock(page->document);
    return false;
  }

  /* get the label from the plugin */
  if (functions->page_get_label != NULL) {
    ret = functions->page_get_label(page, page->data, &page->label);
    if (ret != ZATURA_ERROR_OK) {
      girara_info("Failed to get label of page %u: %d", page->index + 1, ret);
      zatura_check_set_error(error, ret);
      zatura_document_unlock(page->document);
      return false;
    }

    if (page->label != NULL) {
      char page_number_string[G_ASCII_DTOSTR_BUF_SIZE];
      g_ascii_dtostr(page_number_string, G_ASCII_DTOSTR_BUF_SIZE, page->index + 1);
      page->label_is_number = g_strcmp0(page->label, page_number_string) == 0;
    }
  }

  g_atomic_int_set(&page->loaded, 1);
  zatura_document_unlock(page->document);
  return true;
}

bool zatura_page_is_loaded(zatura_page_t* page) {
  return page != NULL && g_atomic_int_get(&page->loaded) == 1;
}

zatura_error_t zatura_page_free(zatura_page_t* page) {
  if (page == NULL) {
    return ZATURA_ERROR_INVALID_ARGUMENTS;
  }

  if (page->document == NULL) {
    g_free(page);
    return ZATURA_ERROR_INVALID_ARGUMENTS;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);

  /* the plugin only has data to clear when the page has been parsed */
  zatura_error_t error = ZATURA_ERROR_OK;
  if (g_atomic_int_get(&page->loaded) == 1) {
    error = functions->page_clear(page, page->data);
  }

  g_free(page->label);
  g_free(page);

  return error;
}

zatura_document_t* zatura_page_get_document(zatura_page_t* page) {
  if (page == NULL) {
    return NULL;
  }

  return page->document;
}

unsigned int zatura_page_get_index(zatura_page_t* page) {
  if (page == NULL) {
    return 0;
  }

  return page->index;
}

double zatura_page_get_width(zatura_page_t* page) {
  if (page == NULL) {
    return -1;
  }

  zatura_page_load(page, NULL);

  return page->width;
}

void zatura_page_set_width(zatura_page_t* page, double width) {
  if (page == NULL) {
    return;
  }

  if (!isfinite(width) || width < DBL_EPSILON) {
    girara_warning("Invalid page width: %f, falling back to default", width);
    return;
  }

  page->width = width;
}

double zatura_page_get_height(zatura_page_t* page) {
  if (page == NULL) {
    return -1;
  }

  zatura_page_load(page, NULL);

  return page->height;
}

void zatura_page_set_height(zatura_page_t* page, double height) {
  if (page == NULL) {
    return;
  }

  if (!isfinite(height) || height < DBL_EPSILON) {
    girara_warning("Invalid page height: %f, falling back to default", height);
    return;
  }

  page->height = height;
}

double zatura_page_get_zoom(zatura_page_t* page) {
  if (page == NULL) {
    return -1;
  }

  return page->zoom;
}

void zatura_page_set_zoom(zatura_page_t* page, double zoom) {
  if (page == NULL) {
    return;
  }

  page->zoom = zoom;
}

bool zatura_page_get_visibility(zatura_page_t* page) {
  if (page == NULL) {
    return false;
  }

  return page->visible;
}

void zatura_page_set_visibility(zatura_page_t* page, bool visibility) {
  if (page == NULL) {
    return;
  }

  page->visible = visibility;
}

void* zatura_page_get_data(zatura_page_t* page) {
  if (page == NULL) {
    return NULL;
  }

  return page->data;
}

void zatura_page_set_data(zatura_page_t* page, void* data) {
  if (page == NULL) {
    return;
  }

  page->data = data;
}

girara_list_t* zatura_page_search_text(zatura_page_t* page, const char* text, zatura_error_t* error) {
  if (page == NULL || page->document == NULL || text == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_search_text == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_search_text(page, page->data, text, error);
}

girara_list_t* zatura_page_links_get(zatura_page_t* page, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_links_get == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_links_get(page, page->data, error);
}

zatura_error_t zatura_page_links_free(girara_list_t* UNUSED(list)) {
  return false;
}

girara_list_t* zatura_page_form_fields_get(zatura_page_t* page, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_form_fields_get == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->page_form_fields_get(page, page->data, error);
}

zatura_error_t zatura_page_form_fields_free(girara_list_t* UNUSED(list)) {
  return ZATURA_ERROR_NOT_IMPLEMENTED;
}

girara_list_t* zatura_page_images_get(zatura_page_t* page, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_images_get == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_images_get(page, page->data, error);
}

cairo_surface_t* zatura_page_image_get_cairo(zatura_page_t* page, zatura_image_t* image, zatura_error_t* error) {
  if (page == NULL || page->document == NULL || image == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_image_get_cairo == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->page_image_get_cairo(page, page->data, image, error);
}

char* zatura_page_get_text(zatura_page_t* page, zatura_rectangle_t rectangle, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_get_text == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_get_text(page, page->data, rectangle, error);
}

girara_list_t* zatura_page_get_selection(zatura_page_t* page, zatura_rectangle_t rectangle, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_get_selection == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_get_selection(page, page->data, rectangle, error);
}

zatura_error_t zatura_page_render(zatura_page_t* page, cairo_t* cairo, bool printing) {
  if (page == NULL || page->document == NULL || cairo == NULL) {
    return ZATURA_ERROR_INVALID_ARGUMENTS;
  }

  zatura_error_t error = ZATURA_ERROR_OK;
  if (zatura_page_load(page, &error) == false) {
    return error;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);

  return functions->page_render_cairo(page, page->data, cairo, printing);
}

const char* zatura_page_get_label(zatura_page_t* page, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  return page->label;
}

bool zatura_page_label_is_number(zatura_page_t* page) {
  if (page == NULL) {
    return false;
  }

  return page->label_is_number;
}

girara_list_t* zatura_page_get_signatures(zatura_page_t* page, zatura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zatura_plugin_t* plugin              = zatura_document_get_plugin(page->document);
  const zatura_plugin_functions_t* functions = zatura_plugin_get_functions(plugin);
  if (functions->page_get_signatures == NULL) {
    zatura_check_set_error(error, ZATURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  zatura_error_t e = ZATURA_ERROR_OK;
  if (zatura_page_load(page, error) == false) {
    return NULL;
  }

  g_autoptr(girara_list_t) ret = functions->page_get_signatures(page, page->data, &e);
  if (e != ZATURA_ERROR_OK) {
    zatura_check_set_error(error, e);
    return NULL;
  }

  return g_steal_pointer(&ret);
}
