/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "zatura/sandbox-fds.h"
#include "zatura/seccomp-open.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(c) do { if (!(c)) { perror(#c); exit(1); } } while (0)

static int child_check(int inherited, int stdio, const char* path) {
  if (stdio >= 0 && dup2(inherited, stdio) < 0) return 2;
  if (sandbox_prepare_inherited_fds() != 0) return 3;
  struct iovec buffer = {.iov_base = "EVIL", .iov_len = 4};
  errno = 0;
  if (write(inherited, "EVIL", 4) != -1 || errno != EBADF) return 4;
  if (writev(inherited, &buffer, 1) != -1 || errno != EBADF) return 5;
  if (pwrite(inherited, "EVIL", 4, 0) != -1 || errno != EBADF) return 6;
  if (ftruncate(inherited, 0) != -1 || errno != EBADF) return 7;
  if (fallocate(inherited, 0, 0, 4096) != -1 || errno != EBADF) return 8;
  if (stdio >= 0 && write(stdio, "EVIL", 4) != 4) return 9;
  int bad = open(path, O_RDWR);
  if (bad < 0) return 10;
  int found = -1;
  if (sandbox_check_fds(&found) == 0 || errno != EACCES || found != bad) return 11;
  scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ERRNO(ENOSYS));
  if (!ctx) return 12;
  const int allowed[] = {SCMP_SYS(read), SCMP_SYS(write), SCMP_SYS(writev), SCMP_SYS(close),
                         SCMP_SYS(fcntl), SCMP_SYS(fstatfs), SCMP_SYS(eventfd2), SCMP_SYS(fstat), SCMP_SYS(newfstatat), SCMP_SYS(stat),
                         SCMP_SYS(getdents64), SCMP_SYS(brk), SCMP_SYS(mmap), SCMP_SYS(munmap),
                         SCMP_SYS(futex), SCMP_SYS(lseek), SCMP_SYS(ioctl), SCMP_SYS(memfd_create),
                         SCMP_SYS(ftruncate), SCMP_SYS(fallocate), SCMP_SYS(exit_group)};
  for (unsigned int i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) {
    if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, allowed[i], 0) < 0) return 12;
  }
  if (!ctx || seccomp_restrict_open(ctx) < 0 || seccomp_restrict_metadata(ctx) < 0 || seccomp_load(ctx) < 0) return 12;
  /* Filtering new opens alone is insufficient: detect the writer that existed
   * before enforcement and abort startup rather than parsing a document. */
  if (sandbox_check_fds(&found) == 0 || errno != EACCES || found != bad) return 13;
  close(bad);
  int read_fd = open(path, O_RDONLY);
  int event = eventfd(0, EFD_CLOEXEC);
  int memory = memfd_create("zatura-display-test", MFD_CLOEXEC);
  if (read_fd < 0 || event < 0 || memory < 0 || sandbox_check_fds(NULL) != 0) return 14;
  if (write(read_fd, "EVIL", 4) != -1 || errno != EBADF) return 15;
  if (writev(read_fd, &buffer, 1) != -1 || errno != EBADF) return 16;
  errno = 0;
  int truncated = ftruncate(read_fd, 0);
  if (truncated != -1 || (errno != EINVAL && errno != EBADF)) return 17;
  if (fallocate(read_fd, 0, 0, 4096) != -1 || errno != EBADF) return 18;
  if (fchmod(read_fd, 0777) != -1 || errno != EACCES) return 19;
  if (syscall(SYS_fchmodat, AT_FDCWD, path, 0777) != -1 || errno != EACCES) return 20;
  if (syscall(SYS_mkdir, "/tmp/zatura-sandbox-must-not-create", 0700) != -1 || errno != EACCES) return 21;
  if (syscall(SYS_mkdirat, AT_FDCWD, "/tmp/zatura-sandbox-must-not-create", 0700) != -1 || errno != EACCES) return 22;
  if (ftruncate(memory, 4096) != 0 || write(memory, "SHM", 3) != 3) return 23;
  close(event);
  close(memory);
  close(read_fd);
  seccomp_release(ctx);
  return 0;
}

int main(void) {
  char path[] = "/tmp/zatura-inherited-fd-XXXXXX";
  int fd = mkstemp(path);
  CHECK(fd > STDERR_FILENO);
  const char content[] = "preserve inherited file";
  CHECK(write(fd, content, sizeof(content)) == sizeof(content));
  for (int stdio = -1; stdio <= STDERR_FILENO; ++stdio) {
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(child_check(fd, stdio, path));
    int status;
    CHECK(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "FD regression stdio=%d failed: status=%d\n", stdio, status);
      return 1;
    }
    char actual[sizeof(content) + 1];
    CHECK(pread(fd, actual, sizeof(actual), 0) == sizeof(content));
    CHECK(memcmp(actual, content, sizeof(content)) == 0);
    struct stat st;
    CHECK(fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0600);
  }
  CHECK(close(fd) == 0);
  CHECK(unlink(path) == 0);
  return 0;
}
