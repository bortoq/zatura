/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "zatura/landlock.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef LANDLOCK_RESTRICT_SELF_TSYNC
#define LANDLOCK_RESTRICT_SELF_TSYNC (1U << 3)
#endif
static int reported_abi, creates, restricts;
static unsigned int restricted_flags;
/* Deterministic ABI boundary tests, complementing the real-kernel thread test.
 * Only this executable wraps syscalls; production has no override mechanism. */
long __wrap_syscall(long call, ...) {
  va_list args;
  va_start(args, call);
  long result = -1;
  if (call == SYS_landlock_create_ruleset) {
    const void* attr = va_arg(args, const void*);
    (void)va_arg(args, size_t);
    unsigned int flags = va_arg(args, unsigned int);
    if (!attr && flags == LANDLOCK_CREATE_RULESET_VERSION) {
      errno = ENOSYS;
      result = reported_abi ? reported_abi : -1;
    } else {
      ++creates;
      result = open("/dev/null", O_RDONLY);
    }
  } else if (call == SYS_landlock_restrict_self) {
    (void)va_arg(args, int);
    restricted_flags = va_arg(args, unsigned int);
    ++restricts;
    result = 0;
  } else { errno = ENOSYS; }
  va_end(args);
  return result;
}
int main(void) {
  const int versions[] = {0, 1, 2, 3, 6, 7, 8, 9};
  for (unsigned int i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
    reported_abi = versions[i];
    creates = restricts = 0;
    restricted_flags = 0;
    int result = landlock_drop_write();
    int ok = reported_abi < 8 ? result == 1 && creates == 0 && restricts == 0 :
             result == 0 && creates == 1 && restricts == 1 && restricted_flags == LANDLOCK_RESTRICT_SELF_TSYNC;
    if (!ok) { fprintf(stderr, "Landlock ABI %d policy failed\n", reported_abi); return 1; }
  }
  return 0;
}
