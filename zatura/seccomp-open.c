/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "seccomp-open.h"
#include <errno.h>
#include <fcntl.h>

int seccomp_restrict_open(scmp_filter_ctx ctx) {
  /* O_TMPFILE includes O_DIRECTORY; only its unique bit changes files. */
  const unsigned int tmpfile = O_TMPFILE & ~O_DIRECTORY;
  const unsigned int denied[] = {O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND, tmpfile};
  const unsigned int mask = O_ACCMODE | O_CREAT | O_TRUNC | O_APPEND | tmpfile;
  const int calls[] = {SCMP_SYS(open), SCMP_SYS(openat)};
  for (unsigned int i = 0; i < 2; ++i) {
    const unsigned int arg = i == 0 ? 1 : 2;
    int rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, calls[i], 1,
                             SCMP_CMP(arg, SCMP_CMP_MASKED_EQ, mask, 0));
    if (rc < 0) return rc;
    for (unsigned int j = 0; j < sizeof(denied) / sizeof(denied[0]); ++j) {
      rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), calls[i], 1,
                           SCMP_CMP(arg, SCMP_CMP_MASKED_EQ, denied[j], denied[j]));
      if (rc < 0) return rc;
    }
  }
  return 0;
}
