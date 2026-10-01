/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include "types.h"

/**
 * The document view widget.
 */
struct zatura_document_widget_s {
  GtkWidget parent;
};

struct zatura_document_widget_class_s {
  GtkWidgetClass parent_class;
};

#define ZATURA_TYPE_DOCUMENT_WIDGET (zatura_document_widget_get_type())
#define ZATURA_DOCUMENT_WIDGET(obj)                                                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_DOCUMENT_WIDGET, ZaturaDocumentWidget))
#define ZATURA_DOCUMENT_WIDGET_CLASS(obj)                                                                             \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_DOCUMENT_WIDGET, ZaturaDocumentWidgetClass))
#define ZATURA_IS_DOCUMENT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_DOCUMENT_WIDGET))
#define ZATURA_IS_DOCUMENT_WIDGET_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_DOCUMENT_WIDGET))
#define ZATURA_DOCUMENT_WIDGET_GET_CLASS(obj)                                                                         \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_DOCUMENT_WIDGET, ZaturaDocumentWidgetClass))

/**
 * Returns the type of the document view widget.
 *
 * @return the type
 */
GType zatura_document_widget_get_type(void);

/**
 * Create a document view widget.
 *
 * @param zatura the zatura instance
 * @param zatura_document the associated document, or NULL for an empty widget
 * @return a document view widget
 */
GtkWidget* zatura_document_widget_new(zatura_t* zatura, zatura_document_t* document);

/**
 * Associate a document with the widget and initialize its page storage.
 * Existing page widgets are released first.
 *
 * @param document the document widget
 * @param zatura_document the document, or NULL to clear the widget
 * @return true on success
 */
bool zatura_document_widget_set_document(ZaturaDocumentWidget* document_widget, zatura_document_t* document);

/**
 * Return the document associated with the widget.
 *
 * @param document the document widget
 * @return the associated document
 */
zatura_document_t* zatura_document_widget_get_document(ZaturaDocumentWidget* document);

/**
 * Return a page widget by page number.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return the page widget, or NULL if it has not been created
 */
GtkWidget* zatura_document_widget_get_page(ZaturaDocumentWidget* document, unsigned int page_number);

/**
 * Create a page widget if necessary and attach it to the document grid.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return the page widget, or NULL on error
 */
GtkWidget* zatura_document_widget_ensure_page(ZaturaDocumentWidget* document, unsigned int page_number);

/**
 * Schedule creation of all missing page widgets at low idle priority.
 * Any active preload is restarted. The "page-widgets-loaded" signal is emitted
 * after every page widget has been created.
 *
 * @param document the document widget
 */
void zatura_document_widget_start_page_widget_preload(ZaturaDocumentWidget* document);

/**
 * Cancel page-widget preloading and reset its completion state.
 *
 * @param document the document widget
 */
void zatura_document_widget_stop_page_widget_preload(ZaturaDocumentWidget* document);

/**
 * Return whether the most recent page-widget preload completed.
 *
 * @param document the document widget
 * @return true if all page widgets were created by the preload
 */
bool zatura_document_widget_page_widgets_loaded(ZaturaDocumentWidget* document);

/**
 * Recalculate page visibility from the viewport. This creates newly visible
 * page widgets, updates render priority and caching, and aborts render requests
 * for pages that left the viewport.
 *
 * @param document the document widget
 */
void zatura_document_widget_update_visible_pages(ZaturaDocumentWidget* document);

/**
 * Render the document's current page synchronously and install the resulting
 * surface in its page widget. Does nothing if the renderer or page widget is
 * unavailable.
 *
 * @param document the document widget
 */
void zatura_document_widget_render_current_page(ZaturaDocumentWidget* document);

/**
 * Check whether a page widget exists and has a rendered surface.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return true if the page widget has a rendered surface
 */
bool zatura_document_widget_page_has_surface(ZaturaDocumentWidget* document, unsigned int page_number);

/**
 * Enable or disable signature information on all existing page widgets and on
 * page widgets created later.
 *
 * @param document the document widget
 * @param draw whether signature information should be drawn
 */
void zatura_document_widget_set_draw_signatures(ZaturaDocumentWidget* document, bool draw);

/**
 * Enable or disable search-result highlighting on all existing page widgets.
 *
 * @param document the document widget
 * @param draw whether search results should be drawn
 */
void zatura_document_widget_set_draw_search_results(ZaturaDocumentWidget* document, bool draw);

/**
 * Prepare link hints for the visible pages. Search-result highlighting is
 * disabled and link indices are made continuous across those pages.
 *
 * @param document the document widget
 * @return true if at least one visible page contains a link
 */
bool zatura_document_widget_prepare_links(ZaturaDocumentWidget* document);

/**
 * Disable link hints on all existing page widgets.
 *
 * @param document the document widget
 */
void zatura_document_widget_hide_links(ZaturaDocumentWidget* document);

/**
 * Find a link by its displayed index among the visible page widgets.
 * The returned link remains owned by its page widget.
 *
 * @param document the document widget
 * @param index the displayed link index
 * @return the matching link, or NULL if no visible page contains it
 */
zatura_link_t* zatura_document_widget_get_visible_link(ZaturaDocumentWidget* document, unsigned int index);

/**
 * Count search results on page widgets before a given page. The upper bound is
 * clamped to the document's number of pages, and pages without widgets count as
 * zero.
 *
 * @param document the document widget
 * @param end_page exclusive upper page bound
 * @return the number of search results in pages [0, end_page)
 */
unsigned int zatura_document_widget_get_search_result_count(ZaturaDocumentWidget* document, unsigned int end_page);

/**
 * Update internal layout structures when pages-per-row,
 * first page column or document changes.
 *
 * @param document ZaturaDocumentWidget
 */
void zatura_document_widget_refresh_layout(ZaturaDocumentWidget* document);

void zatura_document_widget_update_mode(ZaturaDocumentWidget* document);

/** Whether a layout transition is waiting to restore the selected page during allocation. */
bool zatura_document_widget_mode_change_pending(ZaturaDocumentWidget* document);

/**
 * Calculate the position of each grid cell.
 * Required when any page size is changed.
 *
 * @param document ZaturaDocumentWidget
 */
void zatura_document_widget_compute_layout(ZaturaDocumentWidget* document);

/**
 * Return the position of a cell from the document's layout table in pixels.
 * It takes the current scale into account.
 * Valid after a call to zatura_document_widget_compute_layout.
 *
 * @param document   ZaturaDocumentWidget
 * @param page_index index of the page
 * @return pos_x     pixel offset in the x direction
 * @return pos_y     pixel offset in the y direction
 */
void zatura_document_widget_get_cell_pos(ZaturaDocumentWidget* document, unsigned int page_index, unsigned int* pos_x,
                                          unsigned int* pos_y);

/**
 * Return the size of a cell from the document's layout table in pixels.
 * It takes the current scale into account.
 * Valid after a call to zatura_document_widget_compute_layout.
 *
 * @param document   ZaturaDocumentWidget
 * @param page_index index of the page
 * @return height    cell height
 * @return width     cell width
 */
void zatura_document_widget_get_cell_size(ZaturaDocumentWidget* document, unsigned int page_index,
                                           unsigned int* height, unsigned int* width);

/**
 * The position and size of a row in the document widget.
 * Valid after a call to zatura_document_widget_compute_layout.
 *
 * @param document   ZaturaDocumentWidget
 * @param row        row number, indexed from 0.
 * @return pos       pixel offset
 * @return size      row size
 */
void zatura_document_widget_get_row(ZaturaDocumentWidget* document, unsigned int row, unsigned int* pos,
                                     unsigned int* size);

/**
 * The position and size of a column in the document widget.
 * Valid after a call to zatura_document_widget_compute_layout.
 *
 * @param document   ZaturaDocumentWidget
 * @param col        column number, indexed from 0.
 * @return pos       pixel offset
 * @return size      col size
 */
void zatura_document_widget_get_col(ZaturaDocumentWidget* document, unsigned int col, unsigned int* pos,
                                     unsigned int* size);

/**
 * Get the size of the entire document to be displayed in pixels.
 * Takes into account the scale, layout of the pages, and padding
 * between them. Valid after a call to zatura_document_widget_compute_layout.
 *
 * @param document ZaturaDocumentWidget
 * @return height  document height in pixels
 * @return width   document width in pixels
 */
void zatura_document_widget_get_document_size(ZaturaDocumentWidget* document, unsigned int* height,
                                               unsigned int* width);

/**
 * Release all page widgets and clear the associated document and layout.
 *
 * @param document ZaturaDocumentWidget
 */
void zatura_document_widget_clear_pages(ZaturaDocumentWidget* document);

/**
 * Clear all thumbnails.
 *
 * @param document ZaturaDocumentWidget
 */
void zatura_document_widget_clear_thumbnails(ZaturaDocumentWidget* document);

/**
 * This function is used to unmark all pages as not rendered. This should
 * be used if all pages should be rendered again (e.g.: the zoom level or the
 * colors have changed)
 *
 * @param zatura Zatura object
 */
void zatura_document_widget_render_all(ZaturaDocumentWidget* document);

/**
 * Sets the layout of the pages in the document
 *
 * @param[in]  document          The document instance
 * @param[in]  page_v_padding      pixels of vertical padding between pages
 * @param[in]  page_h_padding      pixels of horizontal padding between pages
 * @param[in]  pages_per_row     number of pages per row
 * @param[in]  first_page_column column of the first page (first column is 1)
 */
void zatura_document_widget_set_page_layout(ZaturaDocumentWidget* document, unsigned int page_v_padding,
                                             unsigned int page_h_padding, unsigned int pages_per_row,
                                             unsigned int first_page_column);

/**
 * Returns the vertical padding in pixels between pages
 *
 * @param document The document
 * @return The padding in pixels between pages
 */
unsigned int zatura_document_widget_get_page_v_padding(ZaturaDocumentWidget* document);

/**
 * Returns the horizontal padding in pixels between pages
 *
 * @param document The document
 * @return The padding in pixels between pages
 */
unsigned int zatura_document_widget_get_page_h_padding(ZaturaDocumentWidget* document);

/**
 * Returns the number of pages per row
 *
 * @param document The document
 * @return The number of pages per row
 */
unsigned int zatura_document_widget_get_pages_per_row(ZaturaDocumentWidget* document);

/**
 * Returns the column for the first page (first column = 1)
 *
 * @param document The document
 * @return The column for the first page
 */
unsigned int zatura_document_widget_get_first_page_column(ZaturaDocumentWidget* document);

#endif // DOCUMENT_WIDGET_H
