/* SPDX-License-Identifier: Zlib */

#include "plugin.h"

#include <stdlib.h>
#include <glib/gi18n.h>
#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara-gtk/statusbar.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

/**
 * Document plugin structure
 */
struct zatura_plugin_s {
  girara_list_t* content_types;         /**< List of supported content types */
  zatura_plugin_functions_t functions; /**< Document functions */
  GModule* handle;                      /**< DLL handle */
  char* path;                           /**< Path to the plugin */
  const zatura_plugin_definition_t* definition;
};

/**
 * Plugin mapping
 */
typedef struct zatura_type_plugin_mapping_s {
  char* type;               /**< Plugin type */
  zatura_plugin_t* plugin; /**< Mapped plugin */
} zatura_type_plugin_mapping_t;

/**
 * Plugin manager
 */
struct zatura_plugin_manager_s {
  girara_list_t* plugins;             /**< List of plugins */
  girara_list_t* path;                /**< List of plugin paths */
  girara_list_t* type_plugin_mapping; /**< List of type -> plugin mappings */
  girara_list_t* content_types;       /**< List of all registered content types */
};

static void zatura_type_plugin_mapping_free(void* data) {
  if (data != NULL) {
    zatura_type_plugin_mapping_t* mapping = data;

    g_free(mapping->type);
    g_free(mapping);
  }
}

static void zatura_plugin_free(void* data) {
  if (data != NULL) {
    zatura_plugin_t* plugin = data;

    g_free(plugin->path);
    g_module_close(plugin->handle);
    girara_list_free(plugin->content_types);
    g_free(plugin);
  }
}

static void set_plugin_dir(zatura_plugin_manager_t* plugin_manager, const char* dir) {
  if (dir == NULL || dir[0] == '\0') {
    return;
  }

  g_auto(GStrv) paths = g_strsplit(dir, ":", 0);
  for (size_t i = 0; paths[i] != NULL; ++i) {
    girara_list_append(plugin_manager->path, g_strdup(paths[i]));
  }
}

static void set_default_dirs(zatura_plugin_manager_t* plugin_manager) {
#ifdef ZATURA_PLUGINDIR
  set_plugin_dir(plugin_manager, ZATURA_PLUGINDIR);
#endif

  const char* env_paths = g_getenv("ZATURA_PLUGINS_PATH");
  if (env_paths != NULL) {
    set_plugin_dir(plugin_manager, env_paths);
  }
}

zatura_plugin_manager_t* zatura_plugin_manager_new(void) {
  zatura_plugin_manager_t* plugin_manager = g_try_malloc0(sizeof(zatura_plugin_manager_t));
  if (plugin_manager == NULL) {
    return NULL;
  }

  plugin_manager->plugins             = girara_list_new_with_free(zatura_plugin_free);
  plugin_manager->path                = girara_list_new_with_free(g_free);
  plugin_manager->type_plugin_mapping = girara_list_new_with_free(zatura_type_plugin_mapping_free);
  plugin_manager->content_types       = girara_list_new_with_free(g_free);

  if (plugin_manager->plugins == NULL || plugin_manager->path == NULL || plugin_manager->type_plugin_mapping == NULL ||
      plugin_manager->content_types == NULL) {
    zatura_plugin_manager_free(plugin_manager);
    return NULL;
  }

  set_default_dirs(plugin_manager);
  return plugin_manager;
}

void zatura_plugin_manager_set_dir(zatura_plugin_manager_t* plugin_manager, const char* dir) {
  g_return_if_fail(plugin_manager != NULL);

  if (dir != NULL) {
    set_plugin_dir(plugin_manager, dir);
  }
}

static bool check_suffix(const char* path) {
#ifdef __APPLE__
  if (g_str_has_suffix(path, ".dylib")) {
    return true;
  }
#else
  if (g_str_has_suffix(path, ".so")) {
    return true;
  }
#endif

  return false;
}

static void plugin_add_mimetype(zatura_plugin_t* plugin, const char* mime_type) {
  if (plugin == NULL || mime_type == NULL) {
    return;
  }

  char* content_type = g_content_type_from_mime_type(mime_type);
  if (content_type == NULL) {
    girara_warning("plugin: unable to convert mime type: %s", mime_type);
  } else {
    girara_list_append(plugin->content_types, content_type);
  }
}

static bool plugin_mapping_new(zatura_plugin_manager_t* plugin_manager, const gchar* type, zatura_plugin_t* plugin) {
  g_return_val_if_fail(plugin_manager != NULL, false);
  g_return_val_if_fail(type != NULL, false);
  g_return_val_if_fail(plugin != NULL, false);

  for (size_t idx = 0; idx != girara_list_size(plugin_manager->type_plugin_mapping); ++idx) {
    zatura_type_plugin_mapping_t* mapping = girara_list_nth(plugin_manager->type_plugin_mapping, idx);
    if (g_content_type_equals(type, mapping->type)) {
      return false;
    }
  }

  zatura_type_plugin_mapping_t* mapping = g_try_malloc0(sizeof(zatura_type_plugin_mapping_t));
  if (mapping == NULL) {
    return false;
  }

  mapping->type   = g_strdup(type);
  mapping->plugin = plugin;
  girara_list_append(plugin_manager->type_plugin_mapping, mapping);
  girara_list_append(plugin_manager->content_types, g_strdup(type));

  return true;
}

static bool register_plugin(zatura_plugin_manager_t* plugin_manager, zatura_plugin_t* plugin) {
  if (plugin == NULL || plugin->content_types == NULL || plugin_manager == NULL || plugin_manager->plugins == NULL) {
    girara_error("plugin: could not register");
    return false;
  }

  bool at_least_one = false;
  for (size_t idx = 0; idx != girara_list_size(plugin->content_types); ++idx) {
    gchar* type = girara_list_nth(plugin->content_types, idx);
    if (plugin_mapping_new(plugin_manager, type, plugin) == false) {
      girara_error("plugin: filetype already registered: %s", type);
    } else {
      girara_debug("plugin: filetype mapping added: %s", type);
      at_least_one = true;
    }
  }

  if (at_least_one == true) {
    girara_list_append(plugin_manager->plugins, plugin);
  }

  return at_least_one;
}

static void load_plugin(zatura_plugin_manager_t* plugin_manager, const char* plugindir, const char* name) {
  g_autofree char* path = g_build_filename(plugindir, name, NULL);
  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == 0) {
    girara_debug("'%s' is not a regular file. Skipping.", path);
    return;
  }

  if (check_suffix(path) == false) {
    girara_debug("'%s' is not a plugin file. Skipping.", path);
    return;
  }

  /* load plugin */
  GModule* handle = g_module_open(path, G_MODULE_BIND_LOCAL);
  if (handle == NULL) {
    girara_error("Could not load plugin '%s' (%s).", path, g_module_error());
    return;
  }

  /* resolve symbols and check API and ABI version*/
  const zatura_plugin_definition_t* plugin_definition = NULL;
  if (!g_module_symbol(handle, G_STRINGIFY(ZATURA_PLUGIN_DEFINITION_SYMBOL), (void**)&plugin_definition) ||
      plugin_definition == NULL) {
    girara_error("Could not find '%s' in plugin %s - is not a plugin or needs to be rebuilt.",
                 G_STRINGIFY(ZATURA_PLUGIN_DEFINITION_SYMBOL), path);
    g_module_close(handle);
    return;
  }

  /* check name */
  if (plugin_definition->name == NULL) {
    girara_error("Plugin has no name.");
    g_module_close(handle);
    return;
  }

  /* check mime type */
  if (plugin_definition->mime_types == NULL || plugin_definition->mime_types_size == 0) {
    girara_error("Plugin does not handle any mime types.");
    g_module_close(handle);
    return;
  }

  if (plugin_definition->functions.document_open == NULL || plugin_definition->functions.document_free == NULL ||
      plugin_definition->functions.page_init == NULL || plugin_definition->functions.page_clear == NULL ||
      plugin_definition->functions.page_render_cairo == NULL) {
    girara_error("Plugin is missing required functions.");
    g_module_close(handle);
    return;
  }

  zatura_plugin_t* plugin = g_try_malloc0(sizeof(zatura_plugin_t));
  if (plugin == NULL) {
    girara_error("Failed to allocate memory for plugin.");
    g_module_close(handle);
    return;
  }

  plugin->definition    = plugin_definition;
  plugin->functions     = plugin_definition->functions;
  plugin->content_types = girara_list_new_with_free(g_free);
  plugin->handle        = handle;
  plugin->path          = path;

  // plugin took ownership of path
  path = NULL;

  // register mime types
  for (size_t s = 0; s != plugin_definition->mime_types_size; ++s) {
    plugin_add_mimetype(plugin, plugin_definition->mime_types[s]);
  }

  bool ret = register_plugin(plugin_manager, plugin);
  if (ret == false) {
    girara_error("Could not register plugin '%s'.", plugin->path);
    zatura_plugin_free(plugin);
  } else {
    girara_debug("Successfully loaded plugin from '%s'.", plugin->path);
    girara_debug("plugin %s: version %s", plugin_definition->name, plugin_definition->version);
  }
}

static void load_dir(void* data, void* userdata) {
  const char* plugindir                    = data;
  zatura_plugin_manager_t* plugin_manager = userdata;

  GDir* dir = g_dir_open(plugindir, 0, NULL);
  if (dir == NULL) {
    girara_debug("Could not open plugin directory: %s", plugindir);
  } else {
    const char* name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
      load_plugin(plugin_manager, plugindir, name);
    }
    g_dir_close(dir);
  }
}

bool zatura_plugin_manager_load(zatura_plugin_manager_t* plugin_manager) {
  if (plugin_manager == NULL || plugin_manager->path == NULL) {
    return false;
  }

  /* read all files in the plugin directory */
  girara_list_foreach(plugin_manager->path, load_dir, plugin_manager);
  return girara_list_size(plugin_manager->plugins) > 0;
}

const zatura_plugin_t* zatura_plugin_manager_get_plugin(const zatura_plugin_manager_t* plugin_manager,
                                                          const char* type) {
  if (plugin_manager == NULL || plugin_manager->type_plugin_mapping == NULL || type == NULL) {
    return NULL;
  }

  for (size_t idx = 0; idx != girara_list_size(plugin_manager->type_plugin_mapping); ++idx) {
    zatura_type_plugin_mapping_t* mapping = girara_list_nth(plugin_manager->type_plugin_mapping, idx);
    if (g_content_type_equals(type, mapping->type)) {
      return mapping->plugin;
    }
  }

  return NULL;
}

girara_list_t* zatura_plugin_manager_get_plugins(const zatura_plugin_manager_t* plugin_manager) {
  if (plugin_manager == NULL) {
    return NULL;
  }

  return plugin_manager->plugins;
}

girara_list_t* zatura_plugin_manager_get_content_types(const zatura_plugin_manager_t* plugin_manager) {
  if (plugin_manager == NULL) {
    return NULL;
  }

  return plugin_manager->content_types;
}

void zatura_plugin_manager_free(zatura_plugin_manager_t* plugin_manager) {
  if (plugin_manager != NULL) {
    girara_list_free(plugin_manager->content_types);
    girara_list_free(plugin_manager->type_plugin_mapping);
    girara_list_free(plugin_manager->path);
    girara_list_free(plugin_manager->plugins);

    g_free(plugin_manager);
  }
}

const zatura_plugin_functions_t* zatura_plugin_get_functions(const zatura_plugin_t* plugin) {
  if (plugin != NULL) {
    return &plugin->functions;
  } else {
    return NULL;
  }
}

const char* zatura_plugin_get_name(const zatura_plugin_t* plugin) {
  if (plugin != NULL && plugin->definition != NULL) {
    return plugin->definition->name;
  } else {
    return NULL;
  }
}

const char* zatura_plugin_get_path(const zatura_plugin_t* plugin) {
  if (plugin != NULL) {
    return plugin->path;
  } else {
    return NULL;
  }
}

const char* zatura_plugin_get_version(const zatura_plugin_t* plugin) {
  if (plugin && plugin->definition && plugin->definition->version) {
    return plugin->definition->version;
  }

  return "unknown";
}
