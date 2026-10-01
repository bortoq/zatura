/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <stdint.h>

#include <girara/types.h>

#include "types.h"

#define DOCUMENT_DIGEST_SIZE 16

/**
 * Open the document
 *
 * @param plugin_manager The zatura instance
 * @param path Path to the document
 * @param password Password of the document or NULL
 * @param error Optional error parameter
 * @return The document object and NULL if an error occurs
 */
zatura_document_t* zatura_document_open(zatura_t* zatura, const char* path, const char* uri, const char* password,
                                          zatura_error_t* error);

/**
 * Free the document
 *
 * @param document
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_document_free(zatura_document_t* document);

/**
 * Returns the path of the document
 *
 * @param document The document
 * @return The file path of the document
 */
ZATURA_PLUGIN_API const char* zatura_document_get_path(zatura_document_t* document);

/**
 * Returns the URI of the document
 *
 * @param document The document
 * @return The URI of the document
 */
ZATURA_PLUGIN_API const char* zatura_document_get_uri(zatura_document_t* document);

/**
 * Returns the basename of the document
 *
 * @param document The document
 * @return The basename of the document
 */
ZATURA_PLUGIN_API const char* zatura_document_get_basename(zatura_document_t* document);

/**
 * Returns the SHA256 hash of the document
 *
 * @param document The document
 * @return The SHA256 hash of the document
 */
ZATURA_PLUGIN_API const uint8_t* zatura_document_get_hash(zatura_document_t* document);

/**
 * Returns the password of the document
 *
 * @param document The document
 * @return Returns the password of the document
 */
ZATURA_PLUGIN_API const char* zatura_document_get_password(zatura_document_t* document);

/**
 * Returns the page at the given index
 *
 * @param document The document
 * @param index The index of the page
 * @return The page or NULL if an error occurred
 */
ZATURA_PLUGIN_API zatura_page_t* zatura_document_get_page(zatura_document_t* document, unsigned int index);

/**
 * Returns the number of pages
 *
 * @param document The document
 * @return Number of pages
 */
ZATURA_PLUGIN_API unsigned int zatura_document_get_number_of_pages(zatura_document_t* document);

/**
 * Sets the number of pages
 *
 * @param document The document
 * @param number_of_pages Number of pages
 */
ZATURA_PLUGIN_API void zatura_document_set_number_of_pages(zatura_document_t* document,
                                                             unsigned int number_of_pages);

/**
 * Returns the current page number
 *
 * @param document The document
 * @return Current page
 */
ZATURA_PLUGIN_API unsigned int zatura_document_get_current_page_number(zatura_document_t* document);

/**
 * Sets the number of pages
 *
 * @param document The document
 * @param current_page The current page number
 */
ZATURA_PLUGIN_API void zatura_document_set_current_page_number(zatura_document_t* document,
                                                                 unsigned int current_page);

/**
 * Returns the X position, as a value relative to the document width (0=left,
 * 1=right).
 *
 * @param document The document
 * @return X adjustment
 */
ZATURA_PLUGIN_API double zatura_document_get_position_x(zatura_document_t* document);

/**
 * Returns the Y position as value relative to the document height (0=top,
 * 1=bottom)
 *
 * @param document The document
 * @return Y adjustment
 */
ZATURA_PLUGIN_API double zatura_document_get_position_y(zatura_document_t* document);

/**
 * Sets the X position as a value relative to the document width (0=left,
 * 1=right)
 *
 * @param document The document
 * @param position_x the X adjustment
 */
ZATURA_PLUGIN_API void zatura_document_set_position_x(zatura_document_t* document, double position_x);

/**
 * Sets the Y position as a value relative to the document height (0=top,
 * 1=bottom)
 *
 * @param document The document
 * @param position_y the Y adjustment
 */
ZATURA_PLUGIN_API void zatura_document_set_position_y(zatura_document_t* document, double position_y);

/**
 * Returns the current zoom value of the document
 *
 * @param document The document
 * @return The current zoom value
 */
ZATURA_PLUGIN_API double zatura_document_get_zoom(zatura_document_t* document);

/**
 * Returns the current scale value of the document (based on zoom and screen
 * PPI)
 *
 * @param document The document
 * @return The current scale value, in pixels per point
 */
ZATURA_PLUGIN_API double zatura_document_get_scale(zatura_document_t* document);

/**
 * Sets the new zoom value of the document
 *
 * @param document The document
 * @param zoom The new zoom value
 */
ZATURA_PLUGIN_API void zatura_document_set_zoom(zatura_document_t* document, double zoom);

/**
 * Returns the rotation value of zatura (0..360)
 *
 * @param document The document
 * @return The current rotation value
 */
ZATURA_PLUGIN_API unsigned int zatura_document_get_rotation(zatura_document_t* document);

/**
 * Sets the new rotation value
 *
 * @param document The document
 * @param rotation The new rotation value
 */
ZATURA_PLUGIN_API void zatura_document_set_rotation(zatura_document_t* document, unsigned int rotation);

/**
 * Returns the adjust mode of the document
 *
 * @param document The document
 * @return The adjust mode
 */
ZATURA_PLUGIN_API zatura_adjust_mode_t zatura_document_get_adjust_mode(zatura_document_t* document);

/**
 * Sets the new adjust mode of the document
 *
 * @param document The document
 * @param mode The new adjust mode
 */
ZATURA_PLUGIN_API void zatura_document_set_adjust_mode(zatura_document_t* document, zatura_adjust_mode_t mode);

/**
 * Returns the page offset of the document
 *
 * @param document The document
 * @return The page offset
 */
ZATURA_PLUGIN_API int zatura_document_get_page_offset(zatura_document_t* document);

/**
 * Sets the new page offset of the document
 *
 * @param document The document
 * @param page_offset The new page offset
 */
ZATURA_PLUGIN_API void zatura_document_set_page_offset(zatura_document_t* document, unsigned int page_offset);

/**
 * Returns the private data of the document
 *
 * @param document The document
 * @return The private data or NULL
 */
ZATURA_PLUGIN_API void* zatura_document_get_data(zatura_document_t* document);

/**
 * Sets the private data of the document
 *
 * @param document The document
 * @param data The new private data
 */
ZATURA_PLUGIN_API void zatura_document_set_data(zatura_document_t* document, void* data);

/**
 * Sets the width of the viewport in pixels.
 *
 * @param[in] document     The document instance
 * @param[in] width        The width of the viewport
 */
void ZATURA_PLUGIN_API zatura_document_set_viewport_width(zatura_document_t* document, unsigned int width);

/**
 * Sets the height of the viewport in pixels.
 *
 * @param[in] document     The document instance
 * @param[in] height       The height of the viewport
 */
void ZATURA_PLUGIN_API zatura_document_set_viewport_height(zatura_document_t* document, unsigned int height);

/**
 * Return the size of the viewport in pixels.
 *
 * @param[in]  document     The document instance
 * @param[out] height,width The width and height of the viewport
 */
void ZATURA_PLUGIN_API zatura_document_get_viewport_size(zatura_document_t* document, unsigned int* height,
                                                           unsigned int* width);

/**
 Sets the viewport PPI (pixels per inch: the resolution of the monitor, after
 scaling with the device factor).
 *
 * @param[in] document     The document instance
 * @param[in] height       The viewport PPI
 */
void ZATURA_PLUGIN_API zatura_document_set_viewport_ppi(zatura_document_t* document, double ppi);

/**
 * Return the viewport PPI (pixels per inch: the resolution of the monitor,
 * after scaling with the device factor).
 *
 * @param[in] document     The document instance
 * @return    The viewport PPI
 */
double ZATURA_PLUGIN_API zatura_document_get_viewport_ppi(zatura_document_t* document);

/**
 * Set the device scale factors (e.g. for HiDPI). These are generally integers
 * and equal for x and y. These scaling factors are only used when rendering to
 * the screen.
 *
 * @param[in] x_factor,yfactor The x and y scale factors
 */
void ZATURA_PLUGIN_API zatura_document_set_device_factors(zatura_document_t* document, double x_factor,
                                                            double y_factor);
/**
 * Return the current device scale factors (guaranteed to be non-zero).
 *
 * @return The x and y device scale factors
 */
ZATURA_PLUGIN_API zatura_device_factors_t zatura_document_get_device_factors(zatura_document_t* document);

/**
 * Save the document
 *
 * @param document The document object
 * @param path Path for the saved file
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_document_save_as(zatura_document_t* document, const char* path);

/**
 * Generate the document index
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return Generated index
 */
ZATURA_PLUGIN_API girara_tree_node_t* zatura_document_index_generate(zatura_document_t* document,
                                                                       zatura_error_t* error);

/**
 * Get list of attachments
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return List of attachments
 */
ZATURA_PLUGIN_API girara_list_t* zatura_document_attachments_get(zatura_document_t* document,
                                                                   zatura_error_t* error);

/**
 * Save document attachment
 *
 * @param document The document objects
 * @param attachment name of the attachment
 * @param file the target filename
 * @return ZATURA_ERROR_OK when no error occurred, otherwise see
 *    zatura_error_t
 */
ZATURA_PLUGIN_API zatura_error_t zatura_document_attachment_save(zatura_document_t* document,
                                                                    const char* attachment, const char* file);

/**
 * Returns a string of the requested information
 *
 * @param document The zatura document
 * @param error Set to an error value (see \ref zatura_error_t) if an
 *   error occurred
 * @return List of document information entries or NULL if information could not be retrieved
 */
ZATURA_PLUGIN_API girara_list_t* zatura_document_get_information(zatura_document_t* document,
                                                                   zatura_error_t* error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(zatura_document_t, zatura_document_free)

#endif // DOCUMENT_H
