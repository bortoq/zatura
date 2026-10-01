/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_CONTENT_TYPE_H
#define ZATURA_CONTENT_TYPE_H

#include <girara/datastructures.h>

typedef struct zatura_content_type_context_s zatura_content_type_context_t;

/**
 * Create new context for content type detection.
 *
 * @return new context
 */
zatura_content_type_context_t* zatura_content_type_new(void);

/**
 * Free content type detection context.
 *
 * @param context The context.
 */
void zatura_content_type_free(zatura_content_type_context_t* context);

/**
 * "Guess" the content type of a file. Various methods are tried depending on
 * the available libraries.
 *
 * @param path file name
 * @return content type of path, needs to freeed with g_free.
 */
char* zatura_content_type_guess(zatura_content_type_context_t* context, const char* path,
                                 const girara_list_t* supported_content_types);

#endif
