/* SPDX-License-Identifier: Zlib */

#ifndef PLUGIN_H
#define PLUGIN_H

#include <girara/types.h>
#include <gmodule.h>

#include "types.h"
#include "plugin-api.h"
#include "zatura-version.h"
#include "zatura.h"

/**
 * Creates a new instance of the plugin manager
 *
 * @return A plugin manager object or NULL if an error occurred
 */
zatura_plugin_manager_t* zatura_plugin_manager_new(void);

/**
 * Frees the plugin manager
 *
 * @param plugin_manager
 */
void zatura_plugin_manager_free(zatura_plugin_manager_t* plugin_manager);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(zatura_plugin_manager_t, zatura_plugin_manager_free)

/**
 * Add colon-separated list of directories to the plugin manager's plugin search path
 *
 * @param plugin_manager  The plain manager
 * @param dir Colon-separated list of directories
 */
void zatura_plugin_manager_set_dir(zatura_plugin_manager_t* plugin_manager, const char* dir);

/**
 * Loads all plugins available in the previously given directories
 *
 * @param plugin_manager The plugin manager
 * @return Success if some plugins have been loaded, false otherwise
 */
bool zatura_plugin_manager_load(zatura_plugin_manager_t* plugin_manager);

/**
 * Returns the (if available) associated plugin
 *
 * @param plugin_manager The plugin manager
 * @param type The document type
 * @return The plugin or NULL if no matching plugin is available
 */
const zatura_plugin_t* zatura_plugin_manager_get_plugin(const zatura_plugin_manager_t* plugin_manager,
                                                          const char* type);

/**
 * Returns a list with the plugin objects
 *
 * @param plugin_manager The plugin manager
 * @return List of plugins or NULL
 */
girara_list_t* zatura_plugin_manager_get_plugins(const zatura_plugin_manager_t* plugin_manager);

/**
 * Return a list of supported content types
 *
 * @param plugin_manager The plugin manager
 * @return List of plugins or NULL
 */
girara_list_t* zatura_plugin_manager_get_content_types(const zatura_plugin_manager_t* plugin_manager);

/**
 * Returns the plugin functions
 *
 * @param plugin The plugin
 * @return The plugin functions
 */
const zatura_plugin_functions_t* zatura_plugin_get_functions(const zatura_plugin_t* plugin);

/**
 * Returns the name of the plugin
 *
 * @param plugin The plugin
 * @return The name of the plugin or NULL
 */
const char* zatura_plugin_get_name(const zatura_plugin_t* plugin);

/**
 * Returns the path to the plugin
 *
 * @param plugin The plugin
 * @return The path of the plugin or NULL
 */
const char* zatura_plugin_get_path(const zatura_plugin_t* plugin);

/**
 * Returns the version information of the plugin
 *
 * @param plugin The plugin
 * @return The version information of the plugin
 */
const char* zatura_plugin_get_version(const zatura_plugin_t* plugin);

#endif // PLUGIN_H
