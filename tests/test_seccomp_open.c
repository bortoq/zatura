/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "zatura/seccomp-open.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { perror(#condition); exit(1); } } while (0)

int main(void) {
  char directory[] = "/tmp/zatura-seccomp-XXXXXX";
  CHECK(mkdtemp(directory) != NULL);
  char existing[256], missing[256];
  snprintf(existing, sizeof(existing), "%s/existing", directory);
  snprintf(missing, sizeof(missing), "%s/missing", directory);
  const char content[] = "preserve this document";
  int fd = open(existing, O_WRONLY | O_CREAT | O_EXCL, 0600);
  CHECK(fd >= 0);
  CHECK(write(fd, content, sizeof(content)) == sizeof(content));
  CHECK(close(fd) == 0);
  const int flags[] = {O_RDONLY, O_RDONLY | O_CLOEXEC, O_RDONLY | O_TRUNC,
                       O_RDONLY | O_CREAT, O_RDONLY | O_CREAT | O_EXCL,
                       O_RDONLY | O_APPEND, O_WRONLY, O_RDWR, O_TMPFILE | O_RDONLY};
  const long calls[] = {SYS_open, SYS_openat};
  for (unsigned int call = 0; call < 2; ++call) {
    for (unsigned int flag = 0; flag < sizeof(flags) / sizeof(flags[0]); ++flag) {
      const char* path = (flags[flag] & O_CREAT) ? missing :
                         ((flags[flag] & O_TMPFILE) == O_TMPFILE ? directory : existing);
      pid_t child = fork();
      CHECK(child >= 0);
      if (child == 0) {
        /* No Landlock and no display server: load the production open rules. */
        scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
        if (ctx == NULL || seccomp_restrict_open(ctx) < 0 ||
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0) < 0 ||
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0) < 0 ||
            seccomp_load(ctx) < 0) _exit(2);
        errno = 0;
        long result = calls[call] == SYS_open ? syscall(SYS_open, path, flags[flag], 0600) :
                      syscall(SYS_openat, AT_FDCWD, path, flags[flag], 0600);
        int ok = flag < 2 ? result >= 0 : result == -1 && errno == EACCES;
        if (result >= 0) close((int)result);
        _exit(ok ? 0 : 3);
      }
      int status;
      CHECK(waitpid(child, &status, 0) == child);
      CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
      CHECK(access(missing, F_OK) == -1 && errno == ENOENT);
      char actual[sizeof(content) + 1];
      fd = open(existing, O_RDONLY);
      CHECK(fd >= 0);
      CHECK(read(fd, actual, sizeof(actual)) == sizeof(content));
      CHECK(memcmp(actual, content, sizeof(content)) == 0);
      CHECK(close(fd) == 0);
    }
  }
  CHECK(unlink(existing) == 0);
  CHECK(rmdir(directory) == 0);
  return 0;
}
