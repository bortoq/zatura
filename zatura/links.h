/* SPDX-License-Identifier: Zlib */

#ifndef LINK_H
#define LINK_H

#include "types.h"

/**
 * Creates a new zatura link
 *
 * @param type Type of the link
 * @param position Position of the link
 * @param target Target
 * @return New zatura link
 */
ZATURA_PLUGIN_API zatura_link_t* zatura_link_new(zatura_link_type_t type, zatura_rectangle_t position,
                                                    zatura_link_target_t target);

/**
 * Free link
 *
 * @param link The link
 */
ZATURA_PLUGIN_API void zatura_link_free(zatura_link_t* link);

/**
 * Returns the type of the link
 *
 * @param link The link
 * @return The target type of the link
 */
ZATURA_PLUGIN_API zatura_link_type_t zatura_link_get_type(zatura_link_t* link);

/**
 * Returns the position of the link
 *
 * @param link The link
 * @return The position of the link
 */
ZATURA_PLUGIN_API zatura_rectangle_t zatura_link_get_position(zatura_link_t* link);

/**
 * The target value of the link
 *
 * @param link The link
 * @return Returns the target of the link (depends on the link type)
 */
ZATURA_PLUGIN_API zatura_link_target_t zatura_link_get_target(zatura_link_t* link);

#endif // LINK_H
