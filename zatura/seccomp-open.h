/* SPDX-License-Identifier: Zlib */
#ifndef ZATURA_SECCOMP_OPEN_H
#define ZATURA_SECCOMP_OPEN_H
#include <seccomp.h>
/* Shared by the strict sandbox and its syscall regression tests. */
int seccomp_restrict_open(scmp_filter_ctx ctx);
int seccomp_restrict_metadata(scmp_filter_ctx ctx);
int seccomp_restrict_ioctl(scmp_filter_ctx ctx);
#endif
