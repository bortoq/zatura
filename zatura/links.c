/* SPDX-License-Identifier: Zlib */

#include "links-internal.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <girara/utils.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

#include "adjustment.h"
#include "zatura.h"
#include "document.h"
#include "document-widget.h"
#include "utils.h"
#include "page.h"
#include "render.h"

struct zatura_link_s {
  zatura_rectangle_t position; /**< Position of the link */
  zatura_link_target_t target; /**< Link target */
  zatura_link_type_t type;     /**< Link type */
};

zatura_link_t* zatura_link_new(zatura_link_type_t type, zatura_rectangle_t position, zatura_link_target_t target) {
  zatura_link_t* link = g_try_malloc0(sizeof(zatura_link_t));
  if (!link) {
    return NULL;
  }

  link->position = position;
  link->target   = target;
  link->type     = type;

  /* duplicate target.value if necessary */
  switch (type) {
  case ZATURA_LINK_NONE:
  case ZATURA_LINK_GOTO_DEST:
    if (target.value != NULL) {
      link->target.value = g_strdup(target.value);
    }
    break;
  case ZATURA_LINK_GOTO_REMOTE:
  case ZATURA_LINK_URI:
  case ZATURA_LINK_LAUNCH:
  case ZATURA_LINK_NAMED:
    /* target.value is required for these cases */
    if (target.value == NULL) {
      g_free(link);
      return NULL;
    }

    link->target.value = g_strdup(target.value);
    break;
  default:
    g_free(link);
    return NULL;
  }

  return link;
}

void zatura_link_free(zatura_link_t* link) {
  if (!link) {
    return;
  }

  switch (link->type) {
  case ZATURA_LINK_NONE:
  case ZATURA_LINK_GOTO_DEST:
  case ZATURA_LINK_GOTO_REMOTE:
  case ZATURA_LINK_URI:
  case ZATURA_LINK_LAUNCH:
  case ZATURA_LINK_NAMED:
    if (link->target.value != NULL) {
      g_free(link->target.value);
    }
    break;
  default:
    break;
  }

  g_free(link);
}

zatura_link_type_t zatura_link_get_type(zatura_link_t* link) {
  if (!link) {
    return ZATURA_LINK_INVALID;
  }

  return link->type;
}

zatura_rectangle_t zatura_link_get_position(zatura_link_t* link) {
  if (!link) {
    const zatura_rectangle_t position = {0, 0, 0, 0};
    return position;
  }

  return link->position;
}

zatura_link_target_t zatura_link_get_target(zatura_link_t* link) {
  if (!link) {
    const zatura_link_target_t target = {0, NULL, 0, 0, 0, 0, 0, 0};
    return target;
  }

  return link->target;
}

static void link_goto_dest(zatura_t* zatura, const zatura_link_t* link) {
  if (link->target.destination_type == ZATURA_LINK_DESTINATION_UNKNOWN) {
    girara_warning("link destination type unknown");
    return;
  }

  bool link_zoom = true;
  girara_setting_get(zatura->ui.session, "link-zoom", &link_zoom);

  zatura_document_t* document = zatura_get_document(zatura);
  if (link->target.zoom >= DBL_EPSILON && link_zoom) {
    zatura_document_set_zoom(document, zatura_correct_zoom_value(zatura->ui.session, link->target.zoom));
    adjust_view(zatura);
    zatura_document_widget_render_all(zatura->ui.document_widget);
  }

  /* get page */
  zatura_page_t* page = zatura_document_get_page(document, link->target.page_number);
  if (!page) {
    girara_warning("link to non-existing page %u", link->target.page_number);
    return;
  }

  /* compute the position with the page aligned to the top and left
     of the viewport */
  double pos_x = 0;
  double pos_y = 0;
  page_number_to_position(zatura, link->target.page_number, 0.0, 0.0, &pos_x, &pos_y);

  /* correct to place the target position at the top of the viewport     */
  /* NOTE: link->target is in page units, needs to be scaled and rotated */
  unsigned int cell_height = 0;
  unsigned int cell_width  = 0;
  zatura_document_widget_get_cell_size(ZATURA_DOCUMENT_WIDGET(zatura->ui.document_widget), link->target.page_number,
                                        &cell_height, &cell_width);

  unsigned int doc_height = 0;
  unsigned int doc_width  = 0;
  zatura_document_widget_get_document_size(ZATURA_DOCUMENT_WIDGET(zatura->ui.document_widget), &doc_height,
                                            &doc_width);

  bool link_hadjust = true;
  girara_setting_get(zatura->ui.session, "link-hadjust", &link_hadjust);

  /* scale and rotate */
  const double scale = zatura_document_get_scale(document);
  double shiftx      = link->target.left * scale / cell_width;
  double shifty      = link->target.top * scale / cell_height;
  page_calc_position(document, shiftx, shifty, &shiftx, &shifty);

  /* shift the position or set to auto */
  if (link->target.destination_type == ZATURA_LINK_DESTINATION_XYZ && link->target.left != -1 && link_hadjust) {
    pos_x += shiftx * cell_width / doc_width;
  } else {
    pos_x = -1; /* -1 means automatic */
  }

  if (link->target.destination_type == ZATURA_LINK_DESTINATION_XYZ && link->target.top != -1) {
    pos_y += shifty * cell_height / doc_height;
  } else {
    pos_y = -1; /* -1 means automatic */
  }

  /* move to position */
  zatura_jumplist_add(zatura);
  zatura_document_set_current_page_number(document, link->target.page_number);
  position_set(zatura, pos_x, pos_y);
  zatura_jumplist_add(zatura);
}

#ifndef WITH_SANDBOX
static void link_remote(zatura_t* zatura, const char* file) {
  if (!zatura_has_document(zatura) || !file) {
    return;
  }

  const char* path     = zatura_document_get_path(zatura_get_document(zatura));
  g_autofree char* dir = g_path_get_dirname(path);
  g_autofree char* uri = g_build_filename(file, NULL);

  char* argv[] = {*zatura->global.arguments, uri, NULL};

  g_autoptr(GError) error = NULL;
  if (!g_spawn_async(dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
    girara_error("Failed to execute command: %s", error->message);
  }
}

static void link_launch(zatura_t* zatura, const char* link) {
  if (link == NULL) {
    return;
  }

  const char* document = zatura_document_get_path(zatura_get_document(zatura));
  g_autofree char* dir = g_path_get_dirname(document);

  if (girara_xdg_open_with_working_directory(link, dir) == false) {
    girara_notify(zatura->ui.session, GIRARA_ERROR, _("Failed to run xdg-open."));
  }
}

/**
 * Context passed to the external link confirmation dialog callback
 */
typedef struct {
  zatura_t* zatura;
  zatura_link_type_t type;
  char* value; /**< Owned copy of the link target */
} link_confirm_data_t;

static void link_confirm_data_free(link_confirm_data_t* data) {
  if (!data) {
    return;
  }
  g_free(data->value);
  g_free(data);
}

static void cb_link_confirm_hide(GtkWidget* UNUSED(w), void* data) {
  link_confirm_data_free(data);
}

/**
 * Callback for the external link confirmation dialog
 * Opens the link only if the user pressed Enter with empty input or 'y'
 */
static gboolean cb_link_confirm(GiraraDialog* inputbar, const char* input, void* data) {
  g_signal_handlers_disconnect_matched(inputbar, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                       g_signal_lookup("hide", GTK_TYPE_WIDGET), 0, NULL, NULL, data);
  link_confirm_data_t* ctx = data;
  if (!input || !ctx) {
    link_confirm_data_free(ctx);
    return true;
  }

  /* Accept: empty string (bare Enter), or confirmation string */
  const bool confirmed = (!input || input[0] == '\0' || g_strcmp0(input, _("y")) == 0 || g_strcmp0(input, _("Y")) == 0);

  if (confirmed) {
    switch (ctx->type) {
    case ZATURA_LINK_GOTO_REMOTE:
      link_remote(ctx->zatura, ctx->value);
      break;
    case ZATURA_LINK_URI:
    case ZATURA_LINK_LAUNCH: {
      link_launch(ctx->zatura, ctx->value);
      break;
    }
    default:
      break;
    }
  } else {
    girara_notify(ctx->zatura->ui.session, GIRARA_INFO, _("Cancelled."));
  }

  link_confirm_data_free(ctx);
  return true;
}

/**
 * Prompt the user for confirmation before opening an external link
 */
static gboolean link_confirm_spawn(void* data) {
  link_confirm_data_t* ctx = data;

  g_autofree gchar* escaped = g_markup_escape_text(ctx->value, -1);
  g_autofree gchar* prompt  = g_strdup_printf(_("Open external link <b>%s</b>? [Y/n]"), escaped);

  GiraraDialog* dialog = girara_dialog(ctx->zatura->ui.session, prompt, false);
  g_signal_connect(dialog, "hide", G_CALLBACK(cb_link_confirm_hide), ctx);
  g_signal_connect(dialog, "activate", G_CALLBACK(cb_link_confirm), ctx);

  return G_SOURCE_REMOVE;
}

static void link_confirm(zatura_t* zatura, zatura_link_type_t type, const char* value) {
  if (!value) {
    return;
  }

  link_confirm_data_t* ctx = g_try_malloc0(sizeof(link_confirm_data_t));
  if (!ctx) {
    return;
  }

  ctx->zatura = zatura;
  ctx->type    = type;
  ctx->value   = g_strdup(value);

  g_idle_add(link_confirm_spawn, ctx);
}
#endif

void zatura_link_evaluate(zatura_t* zatura, zatura_link_t* link) {
  if (!zatura_has_document(zatura) || !link) {
    return;
  }

#ifdef WITH_SANDBOX
  if (link->type != ZATURA_LINK_GOTO_DEST) {
    girara_notify(zatura->ui.session, GIRARA_ERROR,
                  _("Opening external applications in strict sandbox mode is not permitted"));
    return;
  }
#endif

  switch (link->type) {
  case ZATURA_LINK_GOTO_DEST:
    girara_debug("Going to link destination: page: %u", link->target.page_number);
    link_goto_dest(zatura, link);
    break;
#ifndef WITH_SANDBOX
  case ZATURA_LINK_GOTO_REMOTE:
  case ZATURA_LINK_URI:
  case ZATURA_LINK_LAUNCH:
    bool confirm = true;
    girara_setting_get(zatura->ui.session, "open-link-confirm", &confirm);
    girara_debug("Opening link: %s (type = %u)", link->target.value, (unsigned int)link->target.destination_type);
    if (confirm) {
      link_confirm(zatura, link->type, link->target.value);
    } else if (link->type == ZATURA_LINK_GOTO_REMOTE) {
      link_remote(zatura, link->target.value);
    } else {
      link_launch(zatura, link->target.value);
    }
    break;
#endif
  default:
    girara_error("Unhandled link type: %u", link->type);
    break;
  }
}

void zatura_link_display(zatura_t* zatura, zatura_link_t* link) {
  zatura_link_type_t type     = zatura_link_get_type(link);
  zatura_link_target_t target = zatura_link_get_target(link);
  switch (type) {
  case ZATURA_LINK_GOTO_DEST:
    girara_notify(zatura->ui.session, GIRARA_INFO, _("Link: page %u"), target.page_number);
    break;
  case ZATURA_LINK_GOTO_REMOTE:
  case ZATURA_LINK_URI:
  case ZATURA_LINK_LAUNCH:
  case ZATURA_LINK_NAMED: {
    g_autofree gchar* escaped_value = g_markup_escape_text(target.value, -1);
    girara_notify(zatura->ui.session, GIRARA_INFO, _("Link: %s"), escaped_value);
    break;
  }
  default:
    girara_notify(zatura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}

void zatura_link_copy(zatura_t* zatura, zatura_link_t* link, GdkClipboard* selection) {
  zatura_link_type_t type     = zatura_link_get_type(link);
  zatura_link_target_t target = zatura_link_get_target(link);
  switch (type) {
  case ZATURA_LINK_GOTO_DEST: {
    g_autofree gchar* tmp = g_strdup_printf("%u", target.page_number);
    gdk_clipboard_set_text(selection, tmp);
    girara_notify(zatura->ui.session, GIRARA_INFO, _("Copied page number: %u"), target.page_number);
    break;
  }
  case ZATURA_LINK_GOTO_REMOTE:
  case ZATURA_LINK_URI:
  case ZATURA_LINK_LAUNCH:
  case ZATURA_LINK_NAMED: {
    gdk_clipboard_set_text(selection, target.value);
    g_autofree gchar* escaped_value = g_markup_escape_text(target.value, -1);
    girara_notify(zatura->ui.session, GIRARA_INFO, _("Copied link: %s"), escaped_value);
    break;
  }
  default:
    girara_notify(zatura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}
