/* SPDX-License-Identifier: Zlib */

#include "print.h"

#include <girara/utils.h>
#include <girara-gtk/statusbar.h>
#include <girara-gtk/session.h>
#include <glib/gi18n.h>

#include "document.h"
#include "render.h"
#include "page.h"
#include "internal.h"

static void cb_print_end(GtkPrintOperation* UNUSED(print_operation), GtkPrintContext* UNUSED(context),
                         zatura_t* zatura) {
  if (zatura_has_document(zatura) == false || zatura->ui.session == NULL) {
    return;
  }

  g_autofree char* file_path = get_formatted_filename(zatura, true);
  girara_statusbar_item_set_text(zatura->ui.statusbar.file, file_path);
}

static bool draw_page_cairo(cairo_t* cairo, zatura_t* zatura, zatura_page_t* page) {
  /* Try to render the page without a temporary surface. This only works with
   * plugins that support rendering to any surface.  */
  zatura_renderer_lock(zatura->sync.render_thread);
  const zatura_error_t err = zatura_page_render(page, cairo, true);
  zatura_renderer_unlock(zatura->sync.render_thread);

  return err == ZATURA_ERROR_OK;
}

static bool draw_page_image(cairo_t* cairo, GtkPrintContext* context, zatura_t* zatura, zatura_page_t* page) {
  /* Try to render the page on a temporary image surface. */
  const double width  = gtk_print_context_get_width(context);
  const double height = gtk_print_context_get_height(context);

  /* Render to a surface up to 5x larger; clamp to cairo's 32767 px limit. */
  const double page_height = MIN(32767.0, zatura_page_get_height(page) * 5.0);
  const double page_width  = MIN(32767.0, zatura_page_get_width(page) * 5.0);
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, page_width, page_height);
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    girara_warning("Failed to allocate %dx%d cairo surface for printing.", (int)page_width, (int)page_height);
    cairo_surface_destroy(surface);
    return false;
  }

  cairo_t* temp_cairo = cairo_create(surface);
  if (cairo_status(temp_cairo) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface);
    return false;
  }

  /* Draw a white background. */
  cairo_save(temp_cairo);
  cairo_set_source_rgb(temp_cairo, 1, 1, 1);
  cairo_rectangle(temp_cairo, 0, 0, page_width, page_height);
  cairo_fill(temp_cairo);
  cairo_restore(temp_cairo);

  /* Render the page to the temporary surface */
  zatura_renderer_lock(zatura->sync.render_thread);
  const zatura_error_t err = zatura_page_render(page, temp_cairo, true);
  zatura_renderer_unlock(zatura->sync.render_thread);
  if (err != ZATURA_ERROR_OK) {
    cairo_destroy(temp_cairo);
    cairo_surface_destroy(surface);
    return false;
  }

  /* Rescale the page and keep the aspect ratio */
  const gdouble scale = MIN(width / page_width, height / page_height);
  cairo_scale(cairo, scale, scale);

  /* Blit temporary surface to original cairo object. */
  cairo_set_source_surface(cairo, surface, 0.0, 0.0);
  cairo_paint(cairo);
  cairo_destroy(temp_cairo);
  cairo_surface_destroy(surface);

  return true;
}

static void cb_print_draw_page(GtkPrintOperation* print_operation, GtkPrintContext* context, gint page_number,
                               zatura_t* zatura) {
  if (context == NULL || zatura_has_document(zatura) == false || zatura->ui.session == NULL ||
      zatura->ui.statusbar.file == NULL) {
    gtk_print_operation_cancel(print_operation);
    return;
  }

  /* Update statusbar. */
  g_autofree char* tmp = g_strdup_printf(_("Printing page %u ..."), page_number + 1);
  girara_statusbar_item_set_text(zatura->ui.statusbar.file, tmp);

  /* Get the page and cairo handle.  */
  zatura_page_t* page = zatura_document_get_page(zatura_get_document(zatura), page_number);
  cairo_t* cairo       = gtk_print_context_get_cairo_context(context);
  if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS || page == NULL) {
    gtk_print_operation_cancel(print_operation);
    return;
  }

  girara_debug("printing page %u ...", page_number);
  if (draw_page_cairo(cairo, zatura, page) == true) {
    return;
  }

  girara_debug("printing page %u (fallback) ...", page_number);
  if (draw_page_image(cairo, context, zatura, page) == false) {
    gtk_print_operation_cancel(print_operation);
  }
}

static void cb_print_request_page_setup(GtkPrintOperation* UNUSED(print_operation), GtkPrintContext* UNUSED(context),
                                        gint page_number, GtkPageSetup* setup, zatura_t* zatura) {
  if (!zatura_has_document(zatura)) {
    return;
  }

  zatura_page_t* page = zatura_document_get_page(zatura_get_document(zatura), page_number);
  if (!page) {
    return;
  }

  double width  = zatura_page_get_width(page);
  double height = zatura_page_get_height(page);

  if (width > height) {
    gtk_page_setup_set_orientation(setup, GTK_PAGE_ORIENTATION_LANDSCAPE);
  } else {
    gtk_page_setup_set_orientation(setup, GTK_PAGE_ORIENTATION_PORTRAIT);
  }
}

void print(zatura_t* zatura) {
  g_return_if_fail(zatura_has_document(zatura) == true);

#ifdef WITH_SANDBOX
  /* disable printing in sandbox mode */
  girara_notify(zatura->ui.session, GIRARA_ERROR, _("Printing is not permitted in strict sandbox mode"));
  return;
#endif

  zatura_document_t* document                 = zatura_get_document(zatura);
  g_autoptr(GtkPrintOperation) print_operation = gtk_print_operation_new();

  /* print operation settings */
  gtk_print_operation_set_job_name(print_operation, zatura_document_get_path(document));
  gtk_print_operation_set_allow_async(print_operation, TRUE);
  gtk_print_operation_set_n_pages(print_operation, zatura_document_get_number_of_pages(document));
  gtk_print_operation_set_current_page(print_operation, zatura_document_get_current_page_number(document));
  gtk_print_operation_set_use_full_page(print_operation, TRUE);

  if (zatura->print.settings) {
    gtk_print_operation_set_print_settings(print_operation, zatura->print.settings);
  }

  if (zatura->print.page_setup) {
    gtk_print_operation_set_default_page_setup(print_operation, zatura->print.page_setup);
  }
  gtk_print_operation_set_embed_page_setup(print_operation, TRUE);

  /* print operation signals */
  g_signal_connect(print_operation, "draw-page", G_CALLBACK(cb_print_draw_page), zatura);
  g_signal_connect(print_operation, "end-print", G_CALLBACK(cb_print_end), zatura);
  g_signal_connect(print_operation, "request-page-setup", G_CALLBACK(cb_print_request_page_setup), zatura);

  /* print */
  g_autoptr(GError) error        = NULL;
  GtkPrintOperationResult result = gtk_print_operation_run(print_operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                                           GTK_WINDOW(zatura->ui.session->gtk.window), &error);

  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    girara_notify(zatura->ui.session, GIRARA_ERROR, _("Printing failed: %s"), error->message);
  } else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    g_clear_object(&zatura->print.settings);
    g_clear_object(&zatura->print.page_setup);

    /* save previous settings */
    zatura->print.settings   = g_object_ref(gtk_print_operation_get_print_settings(print_operation));
    zatura->print.page_setup = g_object_ref(gtk_print_operation_get_default_page_setup(print_operation));
  }
}
