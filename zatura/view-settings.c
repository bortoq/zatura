/* SPDX-License-Identifier: Zlib */
#include "view-settings.h"
#include "zatura.h"
#include "document.h"
#include <girara-gtk/settings.h>
#include <json-glib/json-glib.h>
#include <math.h>

/* Only viewing preferences belong in a document's history. */
static const char* names[] = {
  "reflow-font-size", "reflow-margin-top", "reflow-margin-bottom", "reflow-margin-outer", "reflow-margin-inner",
  "page-brightness", "page-contrast", "page-gamma", "page-saturation",
  "recolor", "recolor-keephue", "recolor-reverse-video", "recolor-adjust-lightness",
  "recolor-darkcolor", "recolor-lightcolor", "single-page-mode", "page-mode",
  "page-h-padding", "page-v-padding", "pages-per-row", "first-page-column", "page-right-to-left",
  "vertical-center", "zoom-center", "statusbar-page-percent", "statusbar-show-time", "guioptions"
};

static JsonParser* parse(const char* json) {
  if (!json || !*json) { return NULL; }
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, json, -1, NULL) ||
      !JSON_NODE_HOLDS_OBJECT(json_parser_get_root(parser))) {
    g_object_unref(parser); return NULL;
  }
  return parser;
}

char* zatura_view_settings_capture(zathura_t* app, zathura_document_t* document) {
  JsonObject* object = json_object_new();
  for (size_t i = 0; i < G_N_ELEMENTS(names); ++i) {
    girara_setting_t* setting = girara_setting_find(app->ui.session, names[i]);
    if (!setting) { continue; }
    switch (girara_setting_get_type(setting)) {
    case BOOLEAN: {
      bool value = false; girara_setting_get_value(setting, &value);
      json_object_set_boolean_member(object, names[i], value); break;
    }
    case INT: {
      int value = 0; girara_setting_get_value(setting, &value);
      json_object_set_int_member(object, names[i], value); break;
    }
    case UINT: {
      unsigned value = 0; girara_setting_get_value(setting, &value);
      json_object_set_int_member(object, names[i], value); break;
    }
    case STRING: {
      g_autofree char* value = NULL; girara_setting_get_value(setting, &value);
      json_object_set_string_member(object, names[i], value ? value : ""); break;
    }
    default: break;
    }
  }
  if (document) {
    json_object_set_int_member(object, "_adjust-mode", zathura_document_get_adjust_mode(document));
    float width, height, font;
    zatura_reflow_margins_t m;
    if (zathura_document_get_reflow_layout(document, &width, &height, &font, &m)) {
      JsonObject* layout = json_object_new();
      json_object_set_double_member(layout, "width", width);
      json_object_set_double_member(layout, "height", height);
      json_object_set_double_member(layout, "font", font);
      json_object_set_double_member(layout, "top", m.top);
      json_object_set_double_member(layout, "bottom", m.bottom);
      json_object_set_double_member(layout, "outer", m.outer);
      json_object_set_double_member(layout, "inner", m.inner);
      json_object_set_int_member(layout, "columns", m.columns);
      json_object_set_int_member(layout, "first-column", m.first_column);
      json_object_set_boolean_member(layout, "rtl", m.right_to_left);
      json_object_set_object_member(object, "_layout", layout);
    }
  }
  g_autoptr(JsonNode) root = json_node_new(JSON_NODE_OBJECT);
  json_node_take_object(root, object);
  g_autoptr(JsonGenerator) generator = json_generator_new();
  json_generator_set_root(generator, root);
  return json_generator_to_data(generator, NULL);
}

void zatura_view_settings_restore(zathura_t* app, const char* json) {
  g_autoptr(JsonParser) parser = parse(json);
  if (!parser) { return; }
  JsonObject* object = json_node_get_object(json_parser_get_root(parser));
  for (size_t i = 0; i < G_N_ELEMENTS(names); ++i) {
    if (!json_object_has_member(object, names[i])) { continue; }
    JsonNode* node = json_object_get_member(object, names[i]);
    if (!JSON_NODE_HOLDS_VALUE(node)) { continue; }
    girara_setting_t* setting = girara_setting_find(app->ui.session, names[i]);
    if (!setting) { continue; }
    const GType type = json_node_get_value_type(node);
    switch (girara_setting_get_type(setting)) {
    case BOOLEAN:
      if (type == G_TYPE_BOOLEAN) {
        const bool value = json_node_get_boolean(node);
        girara_setting_set(app->ui.session, names[i], &value);
      } break;
    case INT:
      if (type == G_TYPE_INT64) {
        const int value = CLAMP(json_node_get_int(node), G_MININT, G_MAXINT);
        girara_setting_set(app->ui.session, names[i], &value);
      } break;
    case UINT:
      if (type == G_TYPE_INT64) {
        const unsigned value = CLAMP(json_node_get_int(node), 0, G_MAXUINT);
        girara_setting_set(app->ui.session, names[i], &value);
      } break;
    case STRING:
      if (type == G_TYPE_STRING) { girara_setting_set(app->ui.session, names[i], json_node_get_string(node)); }
      break;
    default: break;
    }
  }
}

static double number(JsonObject* object, const char* name) {
  if (!json_object_has_member(object, name)) { return NAN; }
  JsonNode* node = json_object_get_member(object, name);
  if (!JSON_NODE_HOLDS_VALUE(node)) { return NAN; }
  const GType type = json_node_get_value_type(node);
  return type == G_TYPE_DOUBLE || type == G_TYPE_INT64 ? json_node_get_double(node) : NAN;
}

void zatura_view_settings_restore_layout(zathura_document_t* document, const char* json) {
  if (!zathura_document_is_reflowable(document)) { return; }
  g_autoptr(JsonParser) parser = parse(json);
  if (!parser) { return; }
  JsonObject* object = json_node_get_object(json_parser_get_root(parser));
  if (!json_object_has_member(object, "_layout")) { return; }
  JsonNode* node = json_object_get_member(object, "_layout");
  if (!JSON_NODE_HOLDS_OBJECT(node)) { return; }
  JsonObject* layout = json_node_get_object(node);
  const double width = number(layout, "width"), height = number(layout, "height"), font = number(layout, "font");
  const double columns = number(layout, "columns"), first = number(layout, "first-column");
  if (!(width >= 80 && width <= 100000 && height >= 120 && height <= 100000 && font >= 6 && font <= 72 &&
      columns >= 1 && columns <= 1000 && first >= 1 && first <= columns)) { return; }
  zatura_reflow_margins_t m = {.top = number(layout, "top"), .bottom = number(layout, "bottom"),
      .outer = number(layout, "outer"), .inner = number(layout, "inner"),
      .columns = columns, .first_column = first};
  if (!(m.top >= 0 && m.bottom >= 0 && m.outer >= 0 && m.inner >= 0 &&
      m.top + m.bottom <= height - 71 && m.outer + m.inner <= width - 71)) { return; }
  if (json_object_has_member(layout, "rtl")) {
    JsonNode* rtl = json_object_get_member(layout, "rtl");
    if (json_node_get_value_type(rtl) == G_TYPE_BOOLEAN) { m.right_to_left = json_node_get_boolean(rtl); }
  }
  zathura_document_reflow(document, width, height, font, &m);
}

zathura_adjust_mode_t zatura_view_settings_adjust_mode(const char* json) {
  g_autoptr(JsonParser) parser = parse(json);
  if (!parser) { return ZATHURA_ADJUST_NONE; }
  const double mode = number(json_node_get_object(json_parser_get_root(parser)), "_adjust-mode");
  return mode >= ZATHURA_ADJUST_NONE && mode <= ZATHURA_ADJUST_WIDTH ? (zathura_adjust_mode_t)mode : ZATHURA_ADJUST_NONE;
}
