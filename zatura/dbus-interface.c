/* SPDX-License-Identifier: Zlib */

#include "dbus-interface.h"

#include <gio/gio.h>
#include <girara-gtk/commands.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "adjustment.h"
#include "config.h"
#include "document.h"
#include "internal.h"
#include "links.h"
#include "macros.h"
#include "resources.h"
#include "synctex.h"
#include "utils.h"
#include "zatura.h"

static const char DBUS_XML_FILENAME[] = "/io/github/bortoq/zatura/DBus/io.github.bortoq.zatura.xml";

static GBytes* load_xml_data(void) {
  GResource* resource = zatura_resources_get_resource();
  if (resource != NULL) {
    return g_resource_lookup_data(resource, DBUS_XML_FILENAME, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  }

  return NULL;
}

typedef struct private_s {
  zatura_t* zatura;
  GDBusNodeInfo* introspection_data;
  GDBusConnection* connection;
  guint owner_id;
  guint registration_id;
  char* bus_name;
} ZaturaDbusPrivate;

G_DEFINE_TYPE_WITH_CODE(ZaturaDbus, zatura_dbus, G_TYPE_OBJECT, G_ADD_PRIVATE(ZaturaDbus))

/* template for bus name */
static const char DBUS_NAME_TEMPLATE[] = "io.github.bortoq.zatura.PID-%d";
/* object path */
static const char DBUS_OBJPATH[] = "/io/github/bortoq/zatura";
/* interface name */
static const char DBUS_INTERFACE[] = "io.github.bortoq.zatura";

static const GDBusInterfaceVTable interface_vtable;

static void finalize(GObject* object) {
  ZaturaDbus* dbus        = ZATURA_DBUS(object);
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(dbus);

  if (priv->connection != NULL && priv->registration_id > 0) {
    g_dbus_connection_unregister_object(priv->connection, priv->registration_id);
  }

  if (priv->owner_id > 0) {
    g_bus_unown_name(priv->owner_id);
  }

  if (priv->introspection_data != NULL) {
    g_dbus_node_info_unref(priv->introspection_data);
  }

  g_free(priv->bus_name);

  G_OBJECT_CLASS(zatura_dbus_parent_class)->finalize(object);
}

static void zatura_dbus_class_init(ZaturaDbusClass* class) {
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = finalize;
}

static void zatura_dbus_init(ZaturaDbus* dbus) {
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(dbus);
  priv->zatura            = NULL;
  priv->introspection_data = NULL;
  priv->connection         = NULL;
  priv->owner_id           = 0;
  priv->registration_id    = 0;
  priv->bus_name           = NULL;
}

static void gdbus_connection_closed(GDBusConnection* UNUSED(connection), gboolean UNUSED(remote_peer_vanished),
                                    GError* error, void* UNUSED(data)) {
  if (error != NULL) {
    girara_debug("D-Bus connection closed: %s", error->message);
  }
}

static void bus_acquired(GDBusConnection* connection, const gchar* name, void* data) {
  girara_debug("Bus acquired at '%s'.", name);

  /* register callback for GDBusConnection's closed signal */
  g_signal_connect(G_OBJECT(connection), "closed", G_CALLBACK(gdbus_connection_closed), NULL);

  ZaturaDbus* dbus        = data;
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(dbus);

  g_autoptr(GError) error = NULL;
  priv->registration_id   = g_dbus_connection_register_object(
      connection, DBUS_OBJPATH, priv->introspection_data->interfaces[0], &interface_vtable, dbus, NULL, &error);
  if (priv->registration_id == 0) {
    girara_warning("Failed to register object on D-Bus connection: %s", error->message);
    return;
  }

  priv->connection = connection;
}

static void name_acquired(GDBusConnection* UNUSED(connection), const gchar* name, void* UNUSED(data)) {
  girara_debug("Acquired '%s' on session bus.", name);
}

static void name_lost(GDBusConnection* UNUSED(connection), const gchar* name, void* UNUSED(data)) {
  girara_debug("Lost connection or failed to acquire '%s' on session bus.", name);
}

ZaturaDbus* zatura_dbus_new(zatura_t* zatura) {
  g_autoptr(GObject) obj = g_object_new(ZATURA_TYPE_DBUS, NULL);
  if (obj == NULL) {
    return NULL;
  }

  ZaturaDbus* dbus        = ZATURA_DBUS(obj);
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(dbus);
  priv->zatura            = zatura;

  g_autoptr(GBytes) xml_data = load_xml_data();
  if (xml_data == NULL) {
    girara_warning("Failed to load introspection data.");
    return NULL;
  }

  g_autoptr(GError) error  = NULL;
  priv->introspection_data = g_dbus_node_info_new_for_xml((const char*)g_bytes_get_data(xml_data, NULL), &error);

  if (priv->introspection_data == NULL) {
    girara_warning("Failed to parse introspection data: %s", error->message);
    return NULL;
  }

  char* well_known_name = g_strdup_printf(DBUS_NAME_TEMPLATE, getpid());
  priv->bus_name        = well_known_name;
  priv->owner_id        = g_bus_own_name(G_BUS_TYPE_SESSION, well_known_name, G_BUS_NAME_OWNER_FLAGS_NONE, bus_acquired,
                                         name_acquired, name_lost, dbus, NULL);

  // dbus takes ownership of obj
  obj = NULL;
  return dbus;
}

const char* zatura_dbus_get_name(zatura_t* zatura) {
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(zatura->dbus);

  return priv->bus_name;
}

void zatura_dbus_edit(zatura_t* zatura, unsigned int page, unsigned int x, unsigned int y) {
  ZaturaDbus* edit        = zatura->dbus;
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(edit);

  const char* filename = zatura_document_get_path(zatura_get_document(priv->zatura));

  g_autofree char* input_file = NULL;
  unsigned int line           = 0;
  unsigned int column         = 0;

  if (synctex_get_input_line_column(zatura, filename, page, x, y, &input_file, &line, &column) == false) {
    return;
  }

  g_autoptr(GError) error = NULL;
  g_dbus_connection_emit_signal(priv->connection, NULL, DBUS_OBJPATH, DBUS_INTERFACE, "Edit",
                                g_variant_new("(suu)", input_file, line, column), &error);

  if (error != NULL) {
    girara_debug("Failed to emit 'Edit' signal: %s", error->message);
  }
}

/* D-Bus handler */

static void handle_open_document(zatura_t* zatura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  g_autofree gchar* filename = NULL;
  g_autofree gchar* password = NULL;
  gint page                  = ZATURA_PAGE_NUMBER_UNSPECIFIED;
  g_variant_get(parameters, "(ssi)", &filename, &password, &page);

  document_close(zatura, false);
  document_open_idle(zatura, filename, strlen(password) > 0 ? password : NULL, page, NULL, NULL, NULL, NULL);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_close_document(zatura_t* zatura, GVariant* UNUSED(parameters), GDBusMethodInvocation* invocation) {
  const bool ret = document_close(zatura, false);

  GVariant* result = g_variant_new("(b)", ret);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_goto_page(zatura_t* zatura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  const unsigned int number_of_pages = zatura_document_get_number_of_pages(zatura_get_document(zatura));

  guint page = 0;
  g_variant_get(parameters, "(u)", &page);

  bool ret = true;
  if (page >= number_of_pages) {
    ret = false;
  } else {
    page_set(zatura, page);
  }

  GVariant* result = g_variant_new("(b)", ret);
  g_dbus_method_invocation_return_value(invocation, result);
}

typedef struct {
  zatura_t* zatura;
  girara_list_t** rectangles;
  unsigned int page;
  unsigned int number_of_pages;
} highlights_rect_data_t;

static gboolean synctex_highlight_rects_impl(gpointer ptr) {
  highlights_rect_data_t* data = ptr;

  /* synctex_highlight_rects transfers ownership of each rectangles[i] to the
   * page widget via g_object_set "search-results"; only free the array. */
  synctex_highlight_rects(data->zatura, data->page, data->rectangles);

  g_free(data->rectangles);
  g_free(data);
  return false;
}

static void synctex_highlight_rects_idle(zatura_t* zatura, girara_list_t** rectangles, unsigned int page,
                                         unsigned number_of_pages) {
  highlights_rect_data_t* data = g_try_malloc0(sizeof(highlights_rect_data_t));
  if (data == NULL) {
    for (unsigned int i = 0; i != number_of_pages; ++i) {
      girara_list_free(rectangles[i]);
    }
    g_free(rectangles);
    return;
  }
  data->zatura         = zatura;
  data->rectangles      = rectangles;
  data->page            = page;
  data->number_of_pages = number_of_pages;

  g_idle_add(synctex_highlight_rects_impl, data);
}

static void handle_highlight_rects(zatura_t* zatura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  const unsigned int number_of_pages = zatura_document_get_number_of_pages(zatura_get_document(zatura));

  guint page                             = 0;
  g_autoptr(GVariantIter) iter           = NULL;
  g_autoptr(GVariantIter) secondary_iter = NULL;
  g_variant_get(parameters, "(ua(dddd)a(udddd))", &page, &iter, &secondary_iter);

  if (page >= number_of_pages) {
    girara_debug("Got invalid page number.");
    GVariant* result = g_variant_new("(b)", false);
    g_dbus_method_invocation_return_value(invocation, result);
    return;
  }

  /* get rectangles */
  girara_list_t** rectangles = g_try_malloc0(number_of_pages * sizeof(girara_list_t*));
  if (rectangles == NULL) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                          "Failed to allocate memory.");
    return;
  }

  rectangles[page] = girara_list_new_with_free(g_free);
  if (rectangles[page] == NULL) {
    g_free(rectangles);
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                          "Failed to allocate memory.");
    return;
  }

  zatura_rectangle_t temp_rect = {0, 0, 0, 0};
  while (g_variant_iter_loop(iter, "(dddd)", &temp_rect.x1, &temp_rect.x2, &temp_rect.y1, &temp_rect.y2)) {
    zatura_rectangle_t* rect = g_try_malloc0(sizeof(zatura_rectangle_t));
    if (rect == NULL) {
      girara_list_free(rectangles[page]);
      g_free(rectangles);
      g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                            "Failed to allocate memory.");
      return;
    }

    *rect = temp_rect;
    girara_list_append(rectangles[page], rect);
  }

  /* get secondary rectangles */
  guint temp_page = 0;
  while (g_variant_iter_loop(secondary_iter, "(udddd)", &temp_page, &temp_rect.x1, &temp_rect.x2, &temp_rect.y1,
                             &temp_rect.y2)) {
    if (temp_page >= number_of_pages) {
      /* error out here? */
      girara_debug("Got invalid page number.");
      continue;
    }

    if (rectangles[temp_page] == NULL) {
      rectangles[temp_page] = girara_list_new_with_free(g_free);
    }

    zatura_rectangle_t* rect = g_try_malloc0(sizeof(zatura_rectangle_t));
    if (rect == NULL || rectangles[temp_page] == NULL) {
      for (unsigned int p = 0; p != number_of_pages; ++p) {
        girara_list_free(rectangles[p]);
      }
      g_free(rectangles);
      g_free(rect);
      g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                            "Failed to allocate memory.");
      return;
    }

    *rect = temp_rect;
    girara_list_append(rectangles[temp_page], rect);
  }

  /* run synctex_highlight_rects in main thread when idle */
  synctex_highlight_rects_idle(zatura, rectangles, page, number_of_pages);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

typedef struct {
  zatura_t* zatura;
  gchar* input_file;
  unsigned int line;
  unsigned int column;
} view_data_t;

static gboolean synctex_view_impl(gpointer ptr) {
  view_data_t* data = ptr;

  synctex_view(data->zatura, data->input_file, data->line, data->column);

  g_free(data->input_file);
  g_free(data);
  return false;
}

static void synctex_view_idle(zatura_t* zatura, gchar* input_file, unsigned int line, unsigned int column) {
  view_data_t* data = g_try_malloc0(sizeof(view_data_t));
  if (data == NULL) {
    g_free(input_file);
    return;
  }
  data->zatura    = zatura;
  data->input_file = input_file;
  data->line       = line;
  data->column     = column;

  g_idle_add(synctex_view_impl, data);
}

static void handle_synctex_view(zatura_t* zatura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  gchar* input_file = NULL;
  guint line        = 0;
  guint column      = 0;
  g_variant_get(parameters, "(suu)", &input_file, &line, &column);

  synctex_view_idle(zatura, input_file, line, column);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_execute_command(zatura_t* zatura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  g_autofree gchar* input = NULL;
  g_variant_get(parameters, "(s)", &input);

  const bool ret   = girara_command_run(zatura->ui.session, input);
  GVariant* result = g_variant_new("(b)", ret);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_source_config(zatura_t* zatura, GVariant* GIRARA_UNUSED(parameters),
                                 GDBusMethodInvocation* invocation) {
  config_load_files(zatura);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_source_config_from_dir(zatura_t* zatura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  g_autofree gchar* input = NULL;
  g_variant_get(parameters, "(s)", &input);

  zatura_set_config_dir(zatura, input);
  config_load_files(zatura);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_method_call(GDBusConnection* UNUSED(connection), const gchar* UNUSED(sender),
                               const gchar* object_path, const gchar* interface_name, const gchar* method_name,
                               GVariant* parameters, GDBusMethodInvocation* invocation, void* data) {
  ZaturaDbus* dbus        = data;
  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(dbus);

  girara_debug("Handling call '%s.%s' on '%s'.", interface_name, method_name, object_path);

  static const struct {
    const char* method;
    void (*handler)(zatura_t*, GVariant*, GDBusMethodInvocation*);
    bool needs_document;
    bool present_window;
  } handlers[] = {
      {"OpenDocument", handle_open_document, false, true},
      {"CloseDocument", handle_close_document, false, false},
      {"GotoPage", handle_goto_page, true, true},
      {"HighlightRects", handle_highlight_rects, true, true},
      {"SynctexView", handle_synctex_view, true, true},
      {"ExecuteCommand", handle_execute_command, false, false},
      {"SourceConfig", handle_source_config, false, false},
      {"SourceConfigFromDirectory", handle_source_config_from_dir, false, false},
  };

  for (size_t idx = 0; idx != sizeof(handlers) / sizeof(handlers[0]); ++idx) {
    if (g_strcmp0(method_name, handlers[idx].method) != 0) {
      continue;
    }

    if (handlers[idx].needs_document == true && zatura_has_document(priv->zatura) == false) {
      g_dbus_method_invocation_return_dbus_error(invocation, "io.github.bortoq.zatura.NoOpenDocument",
                                                 "No document has been opened.");
      return;
    }

    (*handlers[idx].handler)(priv->zatura, parameters, invocation);

    if (handlers[idx].present_window == true) {
      bool present_window = true;
      girara_setting_get(priv->zatura->ui.session, "dbus-raise-window", &present_window);
      if (present_window == true) {
        gtk_window_present(GTK_WINDOW(priv->zatura->ui.session->gtk.window));
      }
    }

    return;
  }
}

static void json_document_info_add_node(JsonBuilder* builder, girara_tree_node_t* index) {
  girara_list_t* list = girara_node_get_children(index);
  for (size_t idx = 0; idx != girara_list_size(list); ++idx) {
    girara_tree_node_t* node               = girara_list_nth(list, idx);
    zatura_index_element_t* index_element = girara_node_get_data(node);

    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "title");
    json_builder_add_string_value(builder, index_element->title);

    zatura_link_type_t type     = zatura_link_get_type(index_element->link);
    zatura_link_target_t target = zatura_link_get_target(index_element->link);
    if (type == ZATURA_LINK_GOTO_DEST) {
      json_builder_set_member_name(builder, "page");
      json_builder_add_int_value(builder, target.page_number + 1);
    } else {
      json_builder_set_member_name(builder, "target");
      json_builder_add_string_value(builder, target.value);
    }

    if (girara_node_get_num_children(node) > 0) {
      json_builder_set_member_name(builder, "sub-index");
      json_builder_begin_array(builder);
      json_document_info_add_node(builder, node);
      json_builder_end_array(builder);
    }
    json_builder_end_object(builder);
  }
}

static void emit_document_signal(zatura_t* zatura, const char* signal, const char* file_path) {
  if (zatura->dbus == NULL) {
    return;
  }

  ZaturaDbusPrivate* priv = zatura_dbus_get_instance_private(zatura->dbus);
  if (priv->connection == NULL || g_dbus_connection_is_closed(priv->connection)) {
    return;
  }

  g_autoptr(GError) error = NULL;
  g_dbus_connection_emit_signal(priv->connection, NULL, DBUS_OBJPATH, DBUS_INTERFACE, signal,
                                g_variant_new("(s)", file_path), &error);
  if (error != NULL) {
    girara_debug("Failed to emit '%s' signal: %s", signal, error->message);
  }
}

void zatura_dbus_document_open(zatura_t* zatura, const char* file_path) {
  emit_document_signal(zatura, "DocumentOpen", file_path);
}

void zatura_dbus_document_close(zatura_t* zatura, const char* file_path) {
  emit_document_signal(zatura, "DocumentClose", file_path);
}

static void json_document_metadata(JsonBuilder* builder, zatura_document_t* document) {
  static const struct {
    zatura_document_information_type_t type;
    const char* name;
  } fields[] = {
      {ZATURA_DOCUMENT_INFORMATION_TITLE, "title"},
      {ZATURA_DOCUMENT_INFORMATION_AUTHOR, "author"},
      {ZATURA_DOCUMENT_INFORMATION_SUBJECT, "subject"},
      {ZATURA_DOCUMENT_INFORMATION_KEYWORDS, "keywords"},
      {ZATURA_DOCUMENT_INFORMATION_CREATOR, "creator"},
      {ZATURA_DOCUMENT_INFORMATION_PRODUCER, "producer"},
      {ZATURA_DOCUMENT_INFORMATION_CREATION_DATE, "creation_date"},
      {ZATURA_DOCUMENT_INFORMATION_MODIFICATION_DATE, "modification_date"},
      {ZATURA_DOCUMENT_INFORMATION_OTHER, "other"},
      {ZATURA_DOCUMENT_INFORMATION_FORMAT, "format"},
  };

  json_builder_begin_object(builder);
  g_autoptr(girara_list_t) information = zatura_document_get_information(document, NULL);
  if (information != NULL) {
    for (size_t i = 0; i < girara_list_size(information); ++i) {
      const zatura_document_information_entry_t* entry = girara_list_nth(information, i);
      if (entry == NULL || entry->value == NULL) {
        continue;
      }
      for (size_t j = 0; j < LENGTH(fields); ++j) {
        if (entry->type == fields[j].type) {
          json_builder_set_member_name(builder, fields[j].name);
          json_builder_add_string_value(builder, entry->value);
          break;
        }
      }
    }
  }
  json_builder_end_object(builder);
}

static GVariant* json_document_info(zatura_t* zatura) {
  zatura_document_t* document = zatura_get_document(zatura);

  g_autoptr(JsonBuilder) builder = json_builder_new();
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "filename");
  json_builder_add_string_value(builder, zatura_document_get_path(document));
  json_builder_set_member_name(builder, "number-of-pages");
  json_builder_add_int_value(builder, zatura_document_get_number_of_pages(document));

  json_builder_set_member_name(builder, "metadata");
  json_document_metadata(builder, document);

  json_builder_set_member_name(builder, "index");
  json_builder_begin_array(builder);
  g_autoptr(girara_tree_node_t) index = zatura_document_index_generate(document, NULL);
  if (index != NULL) {
    json_document_info_add_node(builder, index);
  }
  json_builder_end_array(builder);

  json_builder_end_object(builder);

  g_autoptr(JsonNode) root = json_builder_get_root(builder);
  char* serialized_root    = json_to_string(root, true);

  return g_variant_new_take_string(serialized_root);
}

static GVariant* handle_get_property(GDBusConnection* UNUSED(connection), const gchar* UNUSED(sender),
                                     const gchar* UNUSED(object_path), const gchar* UNUSED(interface_name),
                                     const gchar* property_name, GError** error, void* data) {
  ZaturaDbus* dbus            = data;
  ZaturaDbusPrivate* priv     = zatura_dbus_get_instance_private(dbus);
  zatura_document_t* document = zatura_get_document(priv->zatura);

  if (document == NULL) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "No document open.");
    return NULL;
  }

  if (g_strcmp0(property_name, "filename") == 0) {
    return g_variant_new_string(zatura_document_get_path(document));
  } else if (g_strcmp0(property_name, "pagenumber") == 0) {
    return g_variant_new_uint32(zatura_document_get_current_page_number(document));
  } else if (g_strcmp0(property_name, "numberofpages") == 0) {
    return g_variant_new_uint32(zatura_document_get_number_of_pages(document));
  } else if (g_strcmp0(property_name, "documentinfo") == 0) {
    return json_document_info(priv->zatura);
  }

  return NULL;
}

static const GDBusInterfaceVTable interface_vtable = {
    .method_call  = handle_method_call,
    .get_property = handle_get_property,
    .set_property = NULL,
};

static const unsigned int TIMEOUT = 3000;

static bool call_synctex_view(GDBusConnection* connection, const char* filename, const char* name,
                              const char* input_file, unsigned int line, unsigned int column) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) vfilename =
      g_dbus_connection_call_sync(connection, name, DBUS_OBJPATH, "org.freedesktop.DBus.Properties", "Get",
                                  g_variant_new("(ss)", DBUS_INTERFACE, "filename"), G_VARIANT_TYPE("(v)"),
                                  G_DBUS_CALL_FLAGS_NONE, TIMEOUT, NULL, &error);
  if (vfilename == NULL) {
    girara_error("Failed to query 'filename' property from '%s': %s", name, error->message);
    return false;
  }

  g_autoptr(GVariant) tmp = NULL;
  g_variant_get(vfilename, "(v)", &tmp);
  g_autofree gchar* remote_filename = g_variant_dup_string(tmp, NULL);
  girara_debug("Filename from '%s': %s", name, remote_filename);

  if (g_strcmp0(filename, remote_filename) != 0) {
    return false;
  }

  g_autoptr(GVariant) ret = g_dbus_connection_call_sync(
      connection, name, DBUS_OBJPATH, DBUS_INTERFACE, "SynctexView", g_variant_new("(suu)", input_file, line, column),
      G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, TIMEOUT, NULL, &error);
  if (ret == NULL) {
    girara_error("Failed to run SynctexView on '%s': %s", name, error->message);
    return false;
  }
  return true;
}

static int iterate_instances_call_synctex_view(const char* filename, const char* input_file, unsigned int line,
                                               unsigned int column, pid_t hint) {
  if (!filename) {
    return -1;
  }

  g_autoptr(GError) error               = NULL;
  g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
  if (!connection) {
    girara_error("Could not connect to session bus: %s", error->message);
    return -1;
  }

  if (hint != -1) {
    g_autofree char* well_known_name = g_strdup_printf(DBUS_NAME_TEMPLATE, hint);
    const bool ret = call_synctex_view(connection, filename, well_known_name, input_file, line, column);
    return ret ? 1 : -1;
  }

  g_autoptr(GVariant) vnames = g_dbus_connection_call_sync(
      connection, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames", NULL,
      G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, TIMEOUT, NULL, &error);
  if (!vnames) {
    girara_error("Could not list available names: %s", error->message);
    return -1;
  }

  g_autoptr(GVariantIter) iter = NULL;
  g_variant_get(vnames, "(as)", &iter);

  gchar* name    = NULL;
  bool found_one = false;
  while (found_one == false && g_variant_iter_loop(iter, "s", &name)) {
    if (!g_str_has_prefix(name, "io.github.bortoq.zatura.PID")) {
      continue;
    }
    girara_debug("Found name: %s", name);

    found_one = call_synctex_view(connection, filename, name, input_file, line, column);
  }

  return found_one ? 1 : 0;
}

int zatura_dbus_synctex_position(const char* filename, const char* input_file, int line, int column, pid_t hint) {
  if (filename == NULL || input_file == NULL || line < 0 || column < 0) {
    return -1;
  }

  return iterate_instances_call_synctex_view(filename, input_file, line, column, hint);
}
