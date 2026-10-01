/* SPDX-License-Identifier: Zlib */

#include "callbacks.h"

#include <girara-gtk/statusbar.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#include "links-internal.h"
#include "zatura.h"
#include "render.h"
#include "document.h"
#include "document-widget.h"
#include "index-element-object.h"
#include "utils.h"
#include "shortcuts.h"
#include "page-widget.h"
#include "page.h"
#include "adjustment.h"
#include "synctex.h"
#include "dbus-interface.h"

gboolean cb_destroy(GtkWidget* widget, zatura_t* zatura) {
  /* hide the window on quit while the cleanup runs */
  if (widget == NULL && zatura != NULL && zatura->ui.session != NULL && zatura->ui.session->gtk.window != NULL) {
    gtk_widget_set_visible(zatura->ui.session->gtk.window, FALSE);
    GdkDisplay* display = gtk_widget_get_display(zatura->ui.session->gtk.window);
    if (display != NULL) {
      gdk_display_flush(display);
    }
  }

  /* genuine "destroy": the window and its child widgets are being finalized, so drop the borrowed pointers first */
  if (widget != NULL && zatura != NULL && zatura->ui.session != NULL) {
    zatura->ui.document_widget     = NULL;
    zatura->ui.session->gtk.window = NULL;
  }

  if (zatura_has_document(zatura) == true) {
    document_close(zatura, false);
  }

  GApplication* app = g_application_get_default();
  if (app != NULL) {
    g_application_quit(app);
  }

  return TRUE;
}

void cb_buffer_changed(girara_session_t* session) {
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);

  zatura_t* zatura = session->global.data;

  g_autofree char* buffer = girara_buffer_get(session);
  if (buffer != NULL) {
    girara_statusbar_item_set_text(zatura->ui.statusbar.buffer, buffer);
  } else {
    girara_statusbar_item_set_text(zatura->ui.statusbar.buffer, "");
  }
}

static bool in_single_page_mode(zatura_t* zatura) {
  if (zatura->ui.document_widget == NULL) {
    return false;
  }
  int layout_mode = DOCUMENT_WIDGET_GRID;
  g_object_get(zatura->ui.document_widget, "layout-mode", &layout_mode, NULL);
  return layout_mode == DOCUMENT_WIDGET_SINGLE;
}

/* the value is clamped at the ends of the document so there the ratio no longer marks the visible page */
static double page_position_y(GtkAdjustment* vadjustment) {
  const double value = gtk_adjustment_get_value(vadjustment);
  if (value <= gtk_adjustment_get_lower(vadjustment)) {
    return 0.0;
  }
  if (value >= gtk_adjustment_get_upper(vadjustment) - gtk_adjustment_get_page_size(vadjustment)) {
    return 1.0;
  }
  return zatura_adjustment_get_ratio(vadjustment);
}

void cb_view_hadjustment_value_changed(GtkAdjustment* adjustment, gpointer data) {
  zatura_t* zatura = data;
  if (zatura_has_document(zatura) == false || zatura->ui.document_widget == NULL) {
    return;
  }
  if (zatura_document_widget_mode_change_pending(zatura->ui.document_widget)) {
    girara_debug("Handling view adjustment change while processing page mode change.");
    return;
  }

  /* Do nothing in index mode */
  if (girara_mode_get(zatura->ui.session) == zatura->modes.index) {
    return;
  }

  zatura_document_widget_update_visible_pages(zatura->ui.document_widget);

  zatura_document_t* document = zatura_get_document(zatura);
  const double position_x    = zatura_adjustment_get_ratio(adjustment);
  const double position_y    = zatura_document_get_position_y(document);
  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zatura->ui.view));
  unsigned int page_id       = position_to_page_number(zatura, position_x, page_position_y(vadjustment));

  zatura_document_set_position_x(document, position_x);
  zatura_document_set_position_y(document, position_y);
  /* In single-page mode the page is selected explicitly so scrolling must not change it */
  if (in_single_page_mode(zatura) == false) {
    zatura_document_set_current_page_number(document, page_id);
  }

  statusbar_page_number_update(zatura);
}

void cb_view_vadjustment_value_changed(GtkAdjustment* adjustment, gpointer data) {
  zatura_t* zatura = data;
  if (zatura_has_document(zatura) == false || zatura->ui.document_widget == NULL) {
    return;
  }
  if (zatura_document_widget_mode_change_pending(zatura->ui.document_widget)) {
    girara_debug("Handling view adjustment change while processing page mode change.");
    return;
  }

  /* Do nothing in index mode */
  if (girara_mode_get(zatura->ui.session) == zatura->modes.index) {
    return;
  }

  zatura_document_widget_update_visible_pages(zatura->ui.document_widget);

  zatura_document_t* document = zatura_get_document(zatura);
  const double position_x    = zatura_document_get_position_x(document);
  const double position_y    = zatura_adjustment_get_ratio(adjustment);
  const unsigned int page_id = position_to_page_number(zatura, position_x, page_position_y(adjustment));

  zatura_document_set_position_x(document, position_x);
  zatura_document_set_position_y(document, position_y);
  /* In single-page mode the page is selected explicitly so scrolling must not change it */
  if (in_single_page_mode(zatura) == false) {
    zatura_document_set_current_page_number(document, page_id);
  }

  statusbar_page_number_update(zatura);
}

static void cb_view_adjustment_changed(GtkAdjustment* adjustment, zatura_t* zatura, bool width) {
  /* Do nothing in index mode */
  if (girara_mode_get(zatura->ui.session) == zatura->modes.index) {
    return;
  }

  zatura_document_t* document            = zatura_get_document(zatura);
  const zatura_adjust_mode_t adjust_mode = zatura_document_get_adjust_mode(document);

  /* Don't scroll, we're focusing the inputbar. */
  if (adjust_mode == ZATURA_ADJUST_INPUTBAR) {
    return;
  }

  /* Save the viewport size */
  const unsigned int size = floor(gtk_adjustment_get_page_size(adjustment));
  if (width == true) {
    zatura_document_set_viewport_width(document, size);
  } else {
    zatura_document_set_viewport_height(document, size);
  }

  // bounds from the old layout must not replace the pending page anchor.
  if (zatura_document_widget_mode_change_pending(zatura->ui.document_widget)) {
    girara_debug("Handling view adjustment change while processing page mode change.");
    return;
  }

  /* store the position that was actually applied so it stays valid after the view is sized */
  const double extent = gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_lower(adjustment);
  if (extent > size) {
    const double applied = zatura_adjustment_get_ratio(adjustment);
    if (width == true) {
      zatura_document_set_position_x(document, applied);
    } else {
      zatura_document_set_position_y(document, applied);
    }
  }
}

void cb_view_hadjustment_changed(GtkAdjustment* adjustment, gpointer data) {
  zatura_t* zatura = data;
  g_return_if_fail(zatura != NULL);

  cb_view_adjustment_changed(adjustment, zatura, true);
}

void cb_view_vadjustment_changed(GtkAdjustment* adjustment, gpointer data) {
  zatura_t* zatura = data;
  g_return_if_fail(zatura != NULL);

  cb_view_adjustment_changed(adjustment, zatura, false);
}

void cb_refresh_view(GtkWidget* GIRARA_UNUSED(view), gpointer data) {
  zatura_t* zatura = data;
  if (zatura_has_document(zatura) == false || zatura->ui.document_widget == NULL) {
    return;
  }

  zatura_document_t* document = zatura_get_document(zatura);
  unsigned int page_id         = zatura_document_get_current_page_number(document);
  zatura_page_t* page         = zatura_document_get_page(document, page_id);
  if (page == NULL) {
    return;
  }

  if (zatura_document_widget_page_has_surface(zatura->ui.document_widget, page_id)) {
    document_predecessor_free(zatura);
  }

  GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zatura->ui.view));
  GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zatura->ui.view));

  const double position_x = zatura_document_get_position_x(document);
  const double position_y = zatura_document_get_position_y(document);

  zatura_adjustment_set_value_from_ratio(vadj, position_y);
  zatura_adjustment_set_value_from_ratio(hadj, position_x);

  statusbar_page_number_update(zatura);
}

void cb_monitors_changed(GListModel* UNUSED(model), guint UNUSED(position), guint UNUSED(removed), guint UNUSED(added),
                         gpointer data) {
  zatura_t* zatura = data;
  if (zatura == NULL) {
    return;
  }

  zatura_update_view_ppi(zatura);
}

void cb_scale_factor(GObject* UNUSED(object), GParamSpec* UNUSED(pspec), gpointer data) {
  zatura_t* zatura = data;
  if (zatura_has_document(zatura) == false || zatura->ui.document_widget == NULL) {
    return;
  }

  GtkWidget* view     = zatura->ui.session->gtk.view;
  GtkNative* native   = gtk_widget_get_native(view);
  GdkSurface* surface = native != NULL ? gtk_native_get_surface(native) : NULL;
  /* read the exact display scale instead of the rounded whole number */
  const double new_factor = surface != NULL ? gdk_surface_get_scale(surface) : gtk_widget_get_scale_factor(view);
  if (new_factor <= 0.0) {
    return;
  }

  zatura_document_t* document     = zatura_get_document(zatura);
  zatura_device_factors_t current = zatura_document_get_device_factors(document);
  if (fabs(new_factor - current.x) >= DBL_EPSILON || fabs(new_factor - current.y) >= DBL_EPSILON) {
    zatura_document_set_device_factors(document, new_factor, new_factor);
    girara_debug("New device scale factor: %0.2f", new_factor);
    zatura_update_view_ppi(zatura);
    /* the viewport is not yet resized for the new scale, the size allocation that follows renders it */
    if (zatura->sync.initial_render_held == true) {
      zatura->sync.scale_settled = true;
      return;
    }
    zatura_document_widget_render_all(zatura->ui.document_widget);
  }
}

void cb_view_realized(GtkWidget* widget, gpointer data) {
  zatura_t* zatura = data;

  GtkNative* native = gtk_widget_get_native(widget);
  if (native == NULL) {
    return;
  }
  GdkSurface* surface = gtk_native_get_surface(native);
  if (surface == NULL) {
    return;
  }

  /* run the scale handler when the display scale changes */
  g_signal_connect(G_OBJECT(surface), "notify::scale", G_CALLBACK(cb_scale_factor), zatura);
  cb_scale_factor(NULL, NULL, zatura);
}

void cb_page_layout_value_changed(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type),
                                  const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zatura_t* zatura = session->global.data;

  /* pages-per-row must not be 0 */
  if (g_strcmp0(name, "pages-per-row") == 0) {
    unsigned int pages_per_row = *((const unsigned int*)value);
    if (pages_per_row == 0) {
      pages_per_row = 1;
      girara_setting_set(session, name, &pages_per_row);
      girara_notify(session, GIRARA_WARNING, _("'%s' must not be 0. Set to 1."), name);
      return;
    }
  }

  if (zatura_has_document(zatura) == false || zatura->ui.document_widget == NULL) {
    /* no document has been openend yet */
    return;
  }

  unsigned int pages_per_row = 1;
  girara_setting_get(session, "pages-per-row", &pages_per_row);

  /* get list of first_page_column settings */
  g_autofree char* first_page_column_list = NULL;
  girara_setting_get(session, "first-page-column", &first_page_column_list);

  /* find value for first_page_column */
  unsigned int first_page_column = find_first_page_column(first_page_column_list, pages_per_row);

  unsigned int page_v_padding = 1;
  girara_setting_get(zatura->ui.session, "page-v-padding", &page_v_padding);

  unsigned int page_h_padding = 1;
  girara_setting_get(zatura->ui.session, "page-h-padding", &page_h_padding);

  bool page_right_to_left = false;
  girara_setting_get(zatura->ui.session, "page-right-to-left", &page_right_to_left);

  ZaturaDocumentWidget* document_widget = ZATURA_DOCUMENT_WIDGET(zatura->ui.document_widget);
  zatura_document_widget_set_page_layout(document_widget, page_v_padding, page_h_padding, pages_per_row,
                                          first_page_column);

  g_auto(GValue) page_right_to_left_value = G_VALUE_INIT;
  g_value_init(&page_right_to_left_value, G_TYPE_BOOLEAN);
  g_value_set_boolean(&page_right_to_left_value, page_right_to_left);
  g_object_set_property(G_OBJECT(zatura->ui.document_widget), "pages-right-to-left", &page_right_to_left_value);

  zatura_document_widget_refresh_layout(document_widget);

  /* padding/column changes shift each page's relative position, so re-anchor on the current page;
     otherwise its stale ratio leaves it off-screen and unrendered until the next manual scroll */
  zatura_document_t* document    = zatura_get_document(zatura);
  const unsigned int current_page = zatura_document_get_current_page_number(document);
  double anchor_x = 0.0, anchor_y = 0.0;
  page_number_to_position(zatura, current_page, 0.5, 0.5, &anchor_x, &anchor_y);
  zatura_document_set_position_x(document, anchor_x);
  zatura_document_set_position_y(document, anchor_y);
  refresh_view(zatura);
}

void cb_index_row_activated(GtkListView* view, guint position, void* data) {
  zatura_t* zatura = data;
  if (view == NULL || zatura == NULL || zatura->ui.session == NULL) {
    return;
  }

  GListModel* model             = G_LIST_MODEL(gtk_list_view_get_model(view));
  g_autoptr(GtkTreeListRow) row = g_list_model_get_item(model, position);
  if (row == NULL) {
    return;
  }
  g_autoptr(ZaturaIndexElementObject) item = gtk_tree_list_row_get_item(row);
  if (item == NULL || item->element == NULL) {
    return;
  }

  sc_toggle_index(zatura->ui.session, NULL, NULL, 0);
  zatura_link_evaluate(zatura, item->element->link);
}

typedef enum zatura_link_action_e {
  ZATURA_LINK_ACTION_FOLLOW,
  ZATURA_LINK_ACTION_COPY,
  ZATURA_LINK_ACTION_DISPLAY
} zatura_link_action_t;

static gboolean handle_link(const char* input, girara_session_t* session, zatura_link_action_t action) {
  g_return_val_if_fail(session != NULL, FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);

  zatura_t* zatura = session->global.data;

  if (input == NULL || strlen(input) == 0) {
    return FALSE;
  }

  unsigned int index = 0;
  guint64 num;
  if (g_ascii_string_to_unsigned(input, 10, 1, UINT_MAX, &num, NULL)) {
    index = num - 1;
  } else {
    girara_notify(session, GIRARA_WARNING, _("Invalid input '%s' given."), input);
    return FALSE;
  }

  zatura_link_t* link = zatura_document_widget_get_visible_link(zatura->ui.document_widget, index);

  if (link == NULL) {
    girara_notify(session, GIRARA_WARNING, _("Invalid index '%s' given."), input);
  } else {
    switch (action) {
    case ZATURA_LINK_ACTION_FOLLOW:
      zatura_link_evaluate(zatura, link);
      break;
    case ZATURA_LINK_ACTION_DISPLAY:
      zatura_link_display(zatura, link);
      break;
    case ZATURA_LINK_ACTION_COPY: {
      GdkClipboard* selection = get_selection(zatura);
      if (selection == NULL) {
        break;
      }

      zatura_link_copy(zatura, link, selection);
      break;
    }
    }
  }

  return TRUE;
}

gboolean cb_sc_follow(GiraraDialog* UNUSED(inputbar), const char* input, void* data) {
  girara_session_t* session = data;
  return handle_link(input, session, ZATURA_LINK_ACTION_FOLLOW);
}

gboolean cb_sc_display_link(GiraraDialog* UNUSED(inputbar), const char* input, void* data) {
  girara_session_t* session = data;
  return handle_link(input, session, ZATURA_LINK_ACTION_DISPLAY);
}

gboolean cb_sc_copy_link(GiraraDialog* UNUSED(inputbar), const char* input, void* data) {
  girara_session_t* session = data;
  return handle_link(input, session, ZATURA_LINK_ACTION_COPY);
}

static gboolean file_monitor_reload(void* data) {
  sc_reload((girara_session_t*)data, NULL, NULL, 0);
  return FALSE;
}

void cb_file_monitor(ZaturaFileMonitor* monitor, girara_session_t* session) {
  g_return_if_fail(monitor != NULL);
  g_return_if_fail(session != NULL);

  g_main_context_invoke(NULL, file_monitor_reload, session);
}

static void password_dialog_info_free(zatura_password_dialog_info_t* dialog) {
  if (dialog == NULL) {
    return;
  }
  g_free(dialog->path);
  g_free(dialog->uri);
  g_free(dialog);
}

static void cb_password_dialog_hide(GtkWidget* UNUSED(w), void* data) {
  password_dialog_info_free(data);
}

static void password_dialog_arm_hide(zatura_password_dialog_info_t* dialog) {
  if (dialog == NULL || dialog->zatura == NULL || dialog->zatura->ui.session == NULL ||
      dialog->zatura->ui.session->gtk.dialog == NULL) {
    return;
  }
  g_signal_connect(dialog->zatura->ui.session->gtk.dialog, "hide", G_CALLBACK(cb_password_dialog_hide), dialog);
}

gboolean document_open_password_dialog(gpointer data) {
  zatura_password_dialog_info_t* dialog = data;

  GiraraDialog* widget = girara_dialog(dialog->zatura->ui.session, _("Enter password:"), true);
  password_dialog_arm_hide(dialog);
  g_signal_connect(widget, "activate", G_CALLBACK(cb_password_dialog), dialog);
  return FALSE;
}

static gboolean password_dialog(gpointer data) {
  zatura_password_dialog_info_t* dialog = data;

  if (dialog != NULL) {
    GiraraDialog* widget = girara_dialog(dialog->zatura->ui.session, "Incorrect password. Enter password:", true);
    password_dialog_arm_hide(dialog);
    g_signal_connect(widget, "activate", G_CALLBACK(cb_password_dialog), dialog);
  }

  return FALSE;
}

gboolean cb_password_dialog(GiraraDialog* inputbar, const char* input, void* data) {
  g_signal_handlers_disconnect_matched(inputbar, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                       g_signal_lookup("hide", GTK_TYPE_WIDGET), 0, NULL, NULL, data);
  zatura_password_dialog_info_t* dialog = data;
  if (input == NULL || dialog == NULL || dialog->path == NULL || dialog->zatura == NULL) {
    password_dialog_info_free(dialog);
    return false;
  }

  /* no or empty password: ask again */
  if (input == NULL || strlen(input) == 0) {
    g_idle_add(password_dialog, dialog);
    return false;
  }

  /* try to open document again */
  if (document_open(dialog->zatura, dialog->path, dialog->uri, input, ZATURA_PAGE_NUMBER_UNSPECIFIED, NULL) ==
      false) {
    g_idle_add(password_dialog, dialog);
  } else {
    password_dialog_info_free(dialog);
  }

  return true;
}

void cb_setting_recolor_change(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type),
                               const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zatura_t* zatura = session->global.data;

  const bool bool_value = *((const bool*)value);

  if (zatura->sync.render_thread != NULL &&
      zatura_renderer_recolor_enabled(zatura->sync.render_thread) != bool_value) {
    zatura_renderer_enable_recolor(zatura->sync.render_thread, bool_value);
    zatura_document_widget_render_all(zatura->ui.document_widget);

    // the old page thumbnails have been rendered with the old setting, so we need to drop all of them
    // TODO: move this somewhere it makes sense
    if (zatura->ui.document_widget) {
      zatura_document_widget_clear_thumbnails(ZATURA_DOCUMENT_WIDGET(zatura->ui.document_widget));
    }
  }
}

void cb_setting_recolor_keep_hue_change(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type),
                                        const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zatura_t* zatura = session->global.data;

  const bool bool_value = *((const bool*)value);

  if (zatura->sync.render_thread != NULL &&
      zatura_renderer_recolor_hue_enabled(zatura->sync.render_thread) != bool_value) {
    zatura_renderer_enable_recolor_hue(zatura->sync.render_thread, bool_value);
    zatura_document_widget_render_all(zatura->ui.document_widget);
  }
}

void cb_setting_recolor_keep_reverse_video_change(girara_session_t* session, const char* name,
                                                  girara_setting_type_t UNUSED(type), const void* value,
                                                  void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zatura_t* zatura = session->global.data;

  const bool bool_value = *((const bool*)value);

  if (zatura->sync.render_thread != NULL &&
      zatura_renderer_recolor_reverse_video_enabled(zatura->sync.render_thread) != bool_value) {
    zatura_renderer_enable_recolor_reverse_video(zatura->sync.render_thread, bool_value);
    zatura_document_widget_render_all(zatura->ui.document_widget);
  }
}

bool cb_unknown_command(girara_session_t* session, const char* input) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  g_return_val_if_fail(input != NULL, false);

  zatura_t* zatura = session->global.data;
  if (zatura_has_document(zatura) == false) {
    return false;
  }

  unsigned int index = 0;
  guint64 num;
  if (g_ascii_string_to_unsigned(input, 10, 1, UINT_MAX, &num, NULL)) {
    index = num - 1;
  } else {
    return false;
  }

  zatura_jumplist_add(zatura);
  page_set(zatura, index);
  zatura_jumplist_add(zatura);

  return true;
}

void cb_page_widget_text_selected(ZaturaPageWidget* page, const char* text, void* data) {
  g_return_if_fail(page != NULL);
  g_return_if_fail(text != NULL);
  g_return_if_fail(data != NULL);

  zatura_t* zatura = data;
  girara_mode_t mode = girara_mode_get(zatura->ui.session);
  if (mode != zatura->modes.normal && mode != zatura->modes.fullscreen) {
    return;
  }

  GdkClipboard* selection = get_selection(zatura);
  if (selection == NULL) {
    return;
  }

  /* copy to clipboard */
  gdk_clipboard_set_text(selection, text);

  bool notification = true;
  girara_setting_get(zatura->ui.session, "selection-notification", &notification);

  if (notification == true) {
    g_autofree char* target = NULL;
    girara_setting_get(zatura->ui.session, "selection-clipboard", &target);

    g_autofree char* stripped_text = g_strdelimit(g_strdup(text), "\n\t\r\n", ' ');
    g_autofree char* escaped_text =
        g_markup_printf_escaped(_("Copied selected text to selection %s: %s"), target, stripped_text);

    girara_notify(zatura->ui.session, GIRARA_INFO, "%s", escaped_text);
  }
}

void cb_page_widget_image_selected(ZaturaPageWidget* page, GdkTexture* texture, void* data) {
  g_return_if_fail(page != NULL);
  g_return_if_fail(texture != NULL);
  g_return_if_fail(data != NULL);

  zatura_t* zatura      = data;
  GdkClipboard* selection = get_selection(zatura);
  if (selection == NULL) {
    return;
  }

  gdk_clipboard_set(selection, GDK_TYPE_TEXTURE, texture);

  bool notification = true;
  girara_setting_get(zatura->ui.session, "selection-notification", &notification);

  if (notification == true) {
    g_autofree char* target = NULL;
    girara_setting_get(zatura->ui.session, "selection-clipboard", &target);

    g_autofree char* escaped_text = g_markup_printf_escaped(_("Copied selected image to selection %s"), target);

    girara_notify(zatura->ui.session, GIRARA_INFO, "%s", escaped_text);
  }
}

void cb_page_widget_link(ZaturaPageWidget* page, void* data) {
  g_return_if_fail(page != NULL);

  bool enter = (bool)data;

  GdkCursor* cursor = gdk_cursor_new_from_name(enter == true ? "pointer" : "default", NULL);
  gtk_widget_set_cursor(GTK_WIDGET(page), cursor);
  if (cursor != NULL) {
    g_object_unref(cursor);
  }
}

void cb_page_widget_scaled_button_release(ZaturaPageWidget* page_widget, scaled_button_release_event_t* event,
                                          void* data) {
  zatura_t* zatura   = data;
  zatura_page_t* page = zatura_page_widget_get_page(page_widget);

  if (event->button != GDK_BUTTON_PRIMARY) {
    return;
  }

  /* set page number (but don't scroll there. it was clicked on, so it's visible) */
  zatura_document_set_current_page_number(zatura_get_document(zatura), zatura_page_get_index(page));
  refresh_view(zatura);

  if (event->state & zatura->global.synctex_edit_modmask) {
    bool synctex = false;
    girara_setting_get(zatura->ui.session, "synctex", &synctex);
    if (synctex == false) {
      return;
    }

    if (zatura->dbus != NULL) {
      zatura_dbus_edit(zatura, zatura_page_get_index(page), event->x, event->y);
    }

    g_autofree char* editor = NULL;
    girara_setting_get(zatura->ui.session, "synctex-editor-command", &editor);
    if (editor == NULL || *editor == '\0') {
      girara_debug("No SyncTeX editor specified.");
      return;
    }

    synctex_edit(zatura, editor, page, event->x, event->y);
  }
}

void cb_gesture_zoom_begin(GtkGesture* UNUSED(self), GdkEventSequence* UNUSED(sequence), void* data) {
  zatura_t* zatura = data;
  if (zatura_has_document(zatura) == false) {
    return;
  }
  zatura->gesture.initial_zoom = zatura_document_get_zoom(zatura_get_document(zatura));
}

void cb_gesture_zoom_scale_changed(GtkGestureZoom* UNUSED(self), gdouble scale, void* data) {
  zatura_t* zatura = data;
  if (zatura_has_document(zatura) == false) {
    return;
  }

  const double next_zoom     = zatura->gesture.initial_zoom * scale;
  girara_argument_t argument = {.n = ZOOM_SPECIFIC};

  sc_zoom(zatura->ui.session, &argument, NULL, next_zoom * 100);
}

gboolean cb_drop_file(GtkDropTarget* UNUSED(self), const GValue* value, double UNUSED(x), double UNUSED(y),
                      void* data) {
  zatura_t* zatura = data;
  if (zatura == NULL || value == NULL) {
    return FALSE;
  }

  /* the dropped value holds the list of file GFiles (matched GDK_TYPE_FILE_LIST) */
  GSList* files = NULL;
  if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
    files = g_value_get_boxed(value);
  }

  const GSList* file = files;
  while (file != NULL && g_file_is_native(G_FILE(file->data)) == FALSE) {
    file = file->next;
  }
  if (file == NULL) {
    return FALSE;
  }

  g_autofree char* path = g_file_get_path(G_FILE(file->data));
  if (path == NULL) {
    return FALSE;
  }

  if (zatura_has_document(zatura) == true) {
    document_close(zatura, false);
  }

  document_open_idle(zatura, path, NULL, ZATURA_PAGE_NUMBER_UNSPECIFIED, NULL, NULL, NULL, NULL);
  return TRUE;
}

void cb_hide_links(GtkWidget* widget, gpointer data) {
  g_return_if_fail(widget != NULL);
  g_return_if_fail(data != NULL);

  zatura_t* zatura = data;
  zatura_document_widget_hide_links(zatura->ui.document_widget);
}
