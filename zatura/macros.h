/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_MACROS_H
#define ZATURA_MACROS_H

#include <girara/macros.h>

#define UNUSED(x) GIRARA_UNUSED(x)
#define DEPRECATED(x) GIRARA_DEPRECATED(x)
#define ZATURA_PLUGIN_API GIRARA_VISIBLE

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#endif
