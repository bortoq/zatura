/* SPDX-License-Identifier: Zlib */

#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <cairo.h>

#include "types.h"
#include "page.h"
#include "document.h"
#include "links.h"
#include "zatura-version.h"

typedef struct zatura_plugin_functions_s zatura_plugin_functions_t;

/**
 * Opens a document
 */
typedef zatura_error_t (*zatura_plugin_document_open_t)(zatura_document_t* document);

/**
 * Frees the document
 */
typedef zatura_error_t (*zatura_plugin_document_free_t)(zatura_document_t* document, void* data);

/**
 * Generates the document index
 */
typedef girara_tree_node_t* (*zatura_plugin_document_index_generate_t)(zatura_document_t* document, void* data,
                                                                        zatura_error_t* error);

/**
 * Save the document
 */
typedef zatura_error_t (*zatura_plugin_document_save_as_t)(zatura_document_t* document, void* data,
                                                             const char* path);

/**
 * Get list of attachments
 */
typedef girara_list_t* (*zatura_plugin_document_attachments_get_t)(zatura_document_t* document, void* data,
                                                                    zatura_error_t* error);

/**
 * Save attachment to a file
 */
typedef zatura_error_t (*zatura_plugin_document_attachment_save_t)(zatura_document_t* document, void* data,
                                                                     const char* attachment, const char* file);

/**
 * Get document information
 */
typedef girara_list_t* (*zatura_plugin_document_get_information_t)(zatura_document_t* document, void* data,
                                                                    zatura_error_t* error);

/**
 * Gets the page object
 */
typedef zatura_error_t (*zatura_plugin_page_init_t)(zatura_page_t* page);

/**
 * Free page
 */
typedef zatura_error_t (*zatura_plugin_page_clear_t)(zatura_page_t* page, void* data);

/**
 * Search text
 */
typedef girara_list_t* (*zatura_plugin_page_search_text_t)(zatura_page_t* page, void* data, const char* text,
                                                            zatura_error_t* error);

/**
 * Get links on a page
 */
typedef girara_list_t* (*zatura_plugin_page_links_get_t)(zatura_page_t* page, void* data, zatura_error_t* error);

/**
 * Get form fields
 */
typedef girara_list_t* (*zatura_plugin_page_form_fields_get_t)(zatura_page_t* page, void* data,
                                                                zatura_error_t* error);

/**
 * Get list of images
 */
typedef girara_list_t* (*zatura_plugin_page_images_get_t)(zatura_page_t* page, void* data, zatura_error_t* error);

/**
 * Get the image
 */
typedef cairo_surface_t* (*zatura_plugin_page_image_get_cairo_t)(zatura_page_t* page, void* data,
                                                                  zatura_image_t* image, zatura_error_t* error);

/**
 * Get text for selection
 */
typedef char* (*zatura_plugin_page_get_text_t)(zatura_page_t* page, void* data, zatura_rectangle_t rectangle,
                                                zatura_error_t* error);

/**
 * Get rectangles from selection
 */
typedef girara_list_t* (*zatura_plugin_page_get_selection_t)(zatura_page_t* page, void* data,
                                                              zatura_rectangle_t rectangle, zatura_error_t* error);

/**
 * Renders the page to a cairo surface.
 */
typedef zatura_error_t (*zatura_plugin_page_render_cairo_t)(zatura_page_t* page, void* data, cairo_t* cairo,
                                                              bool printing);

/**
 * Get page label.
 */
typedef zatura_error_t (*zatura_plugin_page_get_label_t)(zatura_page_t* page, void* data, char** label);

/**
 * Get signatures
 */
typedef girara_list_t* (*zatura_plugin_page_get_signatures)(zatura_page_t* page, void* data, zatura_error_t* error);

struct zatura_plugin_functions_s {
  /**
   * Opens a document
   */
  zatura_plugin_document_open_t document_open;

  /**
   * Frees the document
   */
  zatura_plugin_document_free_t document_free;

  /**
   * Generates the document index
   */
  zatura_plugin_document_index_generate_t document_index_generate;

  /**
   * Save the document
   */
  zatura_plugin_document_save_as_t document_save_as;

  /**
   * Get list of attachments
   */
  zatura_plugin_document_attachments_get_t document_attachments_get;

  /**
   * Save attachment to a file
   */
  zatura_plugin_document_attachment_save_t document_attachment_save;

  /**
   * Get document information
   */
  zatura_plugin_document_get_information_t document_get_information;

  /**
   * Gets the page object
   */
  zatura_plugin_page_init_t page_init;

  /**
   * Free page
   */
  zatura_plugin_page_clear_t page_clear;

  /**
   * Search text
   */
  zatura_plugin_page_search_text_t page_search_text;

  /**
   * Get links on a page
   */
  zatura_plugin_page_links_get_t page_links_get;

  /**
   * Get form fields
   */
  zatura_plugin_page_form_fields_get_t page_form_fields_get;

  /**
   * Get list of images
   */
  zatura_plugin_page_images_get_t page_images_get;

  /**
   * Get the image
   */
  zatura_plugin_page_image_get_cairo_t page_image_get_cairo;

  /**
   * Get text for selection
   */
  zatura_plugin_page_get_text_t page_get_text;

  /**
   * Get text for selection
   */
  zatura_plugin_page_get_selection_t page_get_selection;

  /**
   * Renders the page to a cairo surface.
   */
  zatura_plugin_page_render_cairo_t page_render_cairo;

  /**
   * Get page label.
   */
  zatura_plugin_page_get_label_t page_get_label;

  /**
   * Get signatures.
   */
  zatura_plugin_page_get_signatures page_get_signatures;
};

typedef struct zatura_plugin_definition_s {
  const char* name;
  const char* version;
  zatura_plugin_functions_t functions;
  const size_t mime_types_size;
  const char** mime_types;
} zatura_plugin_definition_t;

#define JOIN(x, y) JOIN2(x, y)
#define JOIN2(x, y) x##_##y

#define ZATURA_PLUGIN_DEFINITION_SYMBOL JOIN(zatura_plugin, JOIN(ZATURA_API_VERSION, ZATURA_ABI_VERSION))

/**
 * Register a plugin.
 *
 * @param plugin_name the name of the plugin
 * @param version the plugin's version
 * @param plugin_functions function to register the plugin's document functions
 * @param mimetypes a char array of mime types supported by the plugin
 */
#define ZATURA_PLUGIN_REGISTER_WITH_FUNCTIONS(plugin_name, plugin_version, plugin_functions, mimetypes)               \
  static const char* zatura_plugin_mime_types[] = mimetypes;                                                          \
                                                                                                                       \
  ZATURA_PLUGIN_API const zatura_plugin_definition_t ZATURA_PLUGIN_DEFINITION_SYMBOL = {                            \
      .name            = plugin_name,                                                                                  \
      .version         = plugin_version,                                                                               \
      .functions       = plugin_functions,                                                                             \
      .mime_types_size = sizeof(zatura_plugin_mime_types) / sizeof(zatura_plugin_mime_types[0]),                     \
      .mime_types      = zatura_plugin_mime_types,                                                                    \
  };

#define ZATURA_PLUGIN_MIMETYPES(...) __VA_ARGS__
#define ZATURA_PLUGIN_FUNCTIONS(...) __VA_ARGS__

#endif // PLUGIN_API_H
