/* SPDX-License-Identifier: Zlib */

#ifndef PAGE_H
#define PAGE_H

#include <girara/datastructures.h>
#include <cairo.h>

#include "types.h"

/**
 * Get the page object
 *
 * @param document The document
 * @param index Page number
 * @param error Optional error
 * @return Page object or NULL if an error occurred
 */
ZATURA_PLUGIN_API zatura_page_t* zatura_page_new(zatura_document_t* document, unsigned int index,
                                                    zatura_error_t* error);

/**
 * Frees the page object
 *
 * @param page The page object
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_page_free(zatura_page_t* page);

/**
 * Returns the associated document
 *
 * @param page The page object
 * @return The associated document
 * @return NULL if an error occurred
 */
ZATURA_PLUGIN_API zatura_document_t* zatura_page_get_document(zatura_page_t* page);

/**
 * Returns the set id of the page
 *
 * @param page The page object
 * @return The id of the page
 */
ZATURA_PLUGIN_API unsigned int zatura_page_get_index(zatura_page_t* page);

/**
 * Returns the width of the page
 *
 * @param page The page object
 * @return Width of the page
 * @return -1 If an error occurred
 */
ZATURA_PLUGIN_API double zatura_page_get_width(zatura_page_t* page);

/**
 * Sets the new width of the page
 *
 * @param page The page object
 * @param width The new width of the page
 */
ZATURA_PLUGIN_API void zatura_page_set_width(zatura_page_t* page, double width);

/**
 * Returns the height of the page
 *
 * @param page The page object
 * @return Height of the page
 * @return -1 If an error occurred
 */
ZATURA_PLUGIN_API double zatura_page_get_height(zatura_page_t* page);

/**
 * Sets the new height of the page
 *
 * @param page The page object
 * @param height The new height of the page
 */
ZATURA_PLUGIN_API void zatura_page_set_height(zatura_page_t* page, double height);

/**
 * Returns the zoom of the page
 *
 * @param page The page object
 * @return Zoom of the page
 * @return -1 If an error occurred
 */
ZATURA_PLUGIN_API double zatura_page_get_zoom(zatura_page_t* page);

/**
 * Sets the new zoom of the page
 *
 * @param page The page object
 * @param zoom The new zoom of the page
 */
ZATURA_PLUGIN_API void zatura_page_set_zoom(zatura_page_t* page, double zoom);

/**
 * Returns the visibility of the page
 *
 * @param page The page object
 * @return true if the page is visible
 * @return false if the page is hidden
 */
ZATURA_PLUGIN_API bool zatura_page_get_visibility(zatura_page_t* page);

/**
 * Sets the visibility of the page
 *
 * @param page The page object
 * @param visibility The new visibility value
 */
ZATURA_PLUGIN_API void zatura_page_set_visibility(zatura_page_t* page, bool visibility);

/**
 * Returns the custom data
 *
 * @param page The page object
 * @return The custom data or NULL
 */
ZATURA_PLUGIN_API void* zatura_page_get_data(zatura_page_t* page);

/**
 * Sets the custom data
 *
 * @param page The page object
 * @param data The custom data
 */
ZATURA_PLUGIN_API void zatura_page_set_data(zatura_page_t* page, void* data);

/**
 * Search page
 *
 * @param page The page object
 * @param text Search item
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return List of results
 */
ZATURA_PLUGIN_API girara_list_t* zatura_page_search_text(zatura_page_t* page, const char* text,
                                                           zatura_error_t* error);

/**
 * Get page links
 *
 * @param page The page object
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return List of links
 */
ZATURA_PLUGIN_API girara_list_t* zatura_page_links_get(zatura_page_t* page, zatura_error_t* error);

/**
 * Free page links
 *
 * @param list List of links
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_page_links_free(girara_list_t* list);

/**
 * Get list of form fields
 *
 * @param page The page object
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return List of form fields
 */
ZATURA_PLUGIN_API girara_list_t* zatura_page_form_fields_get(zatura_page_t* page, zatura_error_t* error);

/**
 * Free list of form fields
 *
 * @param list List of form fields
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_page_form_fields_free(girara_list_t* list);

/**
 * Get list of images
 *
 * @param page Page
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return List of images or NULL if an error occurred
 */
ZATURA_PLUGIN_API girara_list_t* zatura_page_images_get(zatura_page_t* page, zatura_error_t* error);

/**
 * Get image
 *
 * @param page Page
 * @param image Image identifier
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return The cairo image surface or NULL if an error occurred
 */
ZATURA_PLUGIN_API cairo_surface_t* zatura_page_image_get_cairo(zatura_page_t* page, zatura_image_t* image,
                                                                 zatura_error_t* error);

/**
 * Get text for selection
 * @param page Page
 * @param rectangle Selection
 * @param error Set to an error value (see \ref zatura_error_t) if an error
 * occurred
 * @return The selected text (needs to be deallocated with g_free)
 */
ZATURA_PLUGIN_API char* zatura_page_get_text(zatura_page_t* page, zatura_rectangle_t rectangle,
                                               zatura_error_t* error);

/**
 * Get rectangles from selection
 * @param page Page
 * @param rectangle Selection
 * @param error Set to an error value (see \ref zatura_error_t) if an error
 * occurred
 * @return List of rectangles or NULL if an error occurred
 */
ZATURA_PLUGIN_API girara_list_t* zatura_page_get_selection(zatura_page_t* page, zatura_rectangle_t rectangle,
                                                             zatura_error_t* error);

/**
 * Render page
 *
 * @param page The page object
 * @param cairo Cairo object
 * @param printing render for printing
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_page_render(zatura_page_t* page, cairo_t* cairo, bool printing);

/**
 * Get page label. Note that the page label might not exist, in this case NULL
 * is returned.
 *
 * @param page Page
 * @param error Set to an error value (see \ref zatura_error_t) if an error
 *    occurred.
 * @return Page label
 */
ZATURA_PLUGIN_API const char* zatura_page_get_label(zatura_page_t* page, zatura_error_t* error);

/**
 * Get whether the page label equals the page number
 *
 * @param page Page
 * @return Boolean indicating whether the page label equals the page number
 */
ZATURA_PLUGIN_API bool zatura_page_label_is_number(zatura_page_t* page);

/**
 * Get signatures of a page
 *
 * @param page Page
 * @param error Set to an error value (see \ref zatura_error_t) if an error
 *    occurred.
 * @return List of signatures
 */
ZATURA_PLUGIN_API girara_list_t* zatura_page_get_signatures(zatura_page_t* page, zatura_error_t* error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(zatura_page_t, zatura_page_free)

#endif // PAGE_H
