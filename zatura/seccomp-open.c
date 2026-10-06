/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "seccomp-open.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

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

int seccomp_restrict_metadata(scmp_filter_ctx ctx) {
  /* fchmod also works on read-only file descriptors. mkdir is needed only
   * before enforcement, not for X11 communication while parsing documents. */
  const int calls[] = {SCMP_SYS(fchmod), SCMP_SYS(fchmodat), SCMP_SYS(mkdir), SCMP_SYS(mkdirat)};
  for (unsigned int i = 0; i < sizeof(calls) / sizeof(calls[0]); ++i) {
    int rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), calls[i], 0);
    if (rc < 0) return rc;
  }
  return 0;
}

int seccomp_restrict_ioctl(scmp_filter_ctx ctx) {
  /* FD numbers can be reused: permit only terminal attribute/size reads. */
  const unsigned int requests[] = {TCGETS, TIOCGWINSZ};
  int rc;
  for (unsigned int fd = 1; fd <= 2; ++fd) {
    for (unsigned int i = 0; i < sizeof(requests) / sizeof(requests[0]); ++i) {
      rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
                            SCMP_CMP(0, SCMP_CMP_EQ, fd),
                            SCMP_CMP(1, SCMP_CMP_EQ, requests[i]));
      if (rc < 0) return rc;
    }
  }
  return 0;
}
