/* SPDX-License-Identifier: Zlib */

#ifndef CONFIG_H
#define CONFIG_H

#include "zatura.h"

/**
 * This function loads the default values of the configuration
 *
 * @param zatura The zatura session
 */
void config_load_default(zatura_t* zatura);

/**
 * Loads and evaluates a configuration file
 *
 * @param zatura The zatura session
 */
void config_load_files(zatura_t* zatura);

#endif // CONFIG_H
