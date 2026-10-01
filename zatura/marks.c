/* SPDX-License-Identifier: Zlib */

#include "marks.h"

#include <stdlib.h>
#include <string.h>
#include <girara-gtk/session.h>
#include <girara-gtk/callbacks.h>
#include <girara/datastructures.h>

#include "callbacks.h"
#include "database.h"
#include "document.h"
#include "document-widget.h"
#include "render.h"
#include "utils.h"

static void mark_add(zatura_t* zatura, int key);
static void mark_evaluate(zatura_t* zatura, int key);

static gboolean cb_marks_one_shot(GtkEventControllerKey* controller, guint keyval, guint UNUSED(keycode),
                                  GdkModifierType UNUSED(state), gpointer user_data) {
  girara_session_t* session = user_data;
  g_return_val_if_fail(session != NULL && session->global.data != NULL, FALSE);
  zatura_t* zatura = session->global.data;

  GtkEventController* ctrl = GTK_EVENT_CONTROLLER(controller);
  gboolean evaluate        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(ctrl), "evaluate"));
  GtkWidget* win           = gtk_event_controller_get_widget(ctrl);

  /* remove the controller from its own callback so it only fires once */
  gtk_widget_remove_controller(win, ctrl);

  if (((keyval >= '0' && keyval <= '9') || (keyval >= 'a' && keyval <= 'z') || (keyval >= 'A' && keyval <= 'Z')) ==
      false) {
    return TRUE;
  }

  if (evaluate) {
    mark_evaluate(zatura, keyval);
  } else {
    mark_add(zatura, keyval);
  }
  return TRUE;
}

/* set up a one-shot controller, evaluate selects between add and evaluate */
static void marks_install_one_shot(girara_session_t* session, gboolean evaluate) {
  GtkEventController* ctrl = gtk_event_controller_key_new();
  gtk_event_controller_set_propagation_phase(ctrl, GTK_PHASE_CAPTURE);
  g_object_set_data(G_OBJECT(ctrl), "evaluate", GINT_TO_POINTER(evaluate));
  g_signal_connect(ctrl, "key-pressed", G_CALLBACK(cb_marks_one_shot), session);
  gtk_widget_add_controller(session->gtk.window, ctrl);
}

bool sc_mark_add(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                 unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.view != NULL, false);

  marks_install_one_shot(session, FALSE);
  return true;
}

bool sc_mark_evaluate(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                      unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.view != NULL, false);

  marks_install_one_shot(session, TRUE);
  return true;
}

bool cmd_marks_add(girara_session_t* session, girara_list_t* argument_list) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zatura_t* zatura = (zatura_t*)session->global.data;

  if (girara_list_size(argument_list) < 1) {
    return false;
  }

  const char* key_string = girara_list_nth(argument_list, 0);

  if (key_string == NULL || strlen(key_string) != 1) {
    return false;
  }

  const char key = key_string[0];
  if (((key >= 0x41 && key <= 0x5A) || (key >= 0x61 && key <= 0x7A)) == false) {
    return false;
  }

  mark_add(zatura, key);

  return true;
}

bool cmd_marks_delete(girara_session_t* session, girara_list_t* argument_list) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zatura_t* zatura = (zatura_t*)session->global.data;

  if (girara_list_size(argument_list) < 1) {
    return false;
  }

  if (girara_list_size(zatura->global.marks) == 0) {
    return false;
  }

  for (size_t idx = 0; idx != girara_list_size(argument_list); ++idx) {
    char* key_string = girara_list_nth(argument_list, idx);
    if (key_string == NULL) {
      continue;
    }

    for (unsigned int i = 0; i < strlen(key_string); i++) {
      char key = key_string[i];
      if (((key >= 0x41 && key <= 0x5A) || (key >= 0x61 && key <= 0x7A)) == false) {
        continue;
      }

      /* search for existing mark */
      for (size_t inner_idx = girara_list_size(zatura->global.marks); inner_idx; --inner_idx) {
        zatura_mark_t* mark = girara_list_nth(zatura->global.marks, inner_idx - 1);
        if (mark == NULL) {
          continue;
        }

        if (mark->key == key) {
          girara_list_remove(zatura->global.marks, mark);
        }
      }
    }
  }

  return true;
}

static void mark_add(zatura_t* zatura, int key) {
  if (zatura_has_document(zatura) == false || zatura->global.marks == NULL) {
    return;
  }

  zatura_document_t* document = zatura_get_document(zatura);
  unsigned int page_id         = zatura_document_get_current_page_number(document);
  double position_x            = zatura_document_get_position_x(document);
  double position_y            = zatura_document_get_position_y(document);

  double zoom = zatura_document_get_zoom(document);

  /* search for existing mark */
  for (size_t idx = 0; idx != girara_list_size(zatura->global.marks); ++idx) {
    zatura_mark_t* mark = girara_list_nth(zatura->global.marks, idx);
    if (mark->key == key) {
      mark->page       = page_id;
      mark->position_x = position_x;
      mark->position_y = position_y;
      mark->zoom       = zoom;
      return;
    }
  }

  /* add new mark */
  zatura_mark_t* mark = g_try_malloc0(sizeof(zatura_mark_t));
  if (mark == NULL) {
    return;
  }

  mark->key        = key;
  mark->page       = page_id;
  mark->position_x = position_x;
  mark->position_y = position_y;
  mark->zoom       = zoom;

  girara_list_append(zatura->global.marks, mark);
}

static void mark_evaluate(zatura_t* zatura, int key) {
  if (zatura == NULL || zatura->global.marks == NULL) {
    return;
  }

  /* search for existing mark */
  for (size_t idx = 0; idx != girara_list_size(zatura->global.marks); ++idx) {
    zatura_mark_t* mark = girara_list_nth(zatura->global.marks, idx);
    if (mark != NULL && mark->key == key) {
      zatura_document_set_zoom(zatura_get_document(zatura),
                                zatura_correct_zoom_value(zatura->ui.session, mark->zoom));

      adjust_view(zatura);
      zatura_document_widget_render_all(zatura->ui.document_widget);

      zatura_jumplist_add(zatura);
      page_set(zatura, mark->page);
      position_set(zatura, mark->position_x, mark->position_y);
      zatura_jumplist_add(zatura);

      return;
    }
  }
}

bool zatura_quickmarks_load(zatura_t* zatura, const gchar* file) {
  g_return_val_if_fail(zatura, false);
  g_return_val_if_fail(file, false);

  if (zatura->database == NULL) {
    return false;
  }

  girara_list_t* marks = zatura_db_load_quickmarks(zatura->database, file);
  if (marks == NULL) {
    return false;
  }

  girara_list_free(zatura->global.marks);
  zatura->global.marks = marks;

  return true;
}
