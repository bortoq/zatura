/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include <linux/landlock.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
int main(void) {
  long abi = syscall(SYS_landlock_create_ruleset, NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
  printf("%ld\n", abi);
  return 0;
}
