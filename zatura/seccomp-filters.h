/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_SECCOMP_FILTERS_H
#define ZATURA_SECCOMP_FILTERS_H

#include "zatura.h"

/* strict filter before document parsing */
/* this filter is to be enabled after most of the initialisation of zatura has finished */
int seccomp_enable_strict_filter(zatura_t* zatura);

#endif
