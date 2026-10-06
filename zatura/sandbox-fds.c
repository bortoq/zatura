/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "sandbox-fds.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/magic.h>
#include <sys/vfs.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Record terminal identities before seccomp: its ioctl rules intentionally
 * do not permit arbitrary terminal probes after enforcement. */
static int stdio_tty[3];
static dev_t stdio_tty_device[3];

static int entry_fd(const char* name) {
  char* end;
  long fd = strtol(name, &end, 10);
  return *name && !*end && fd >= 0 && fd <= INT_MAX ? (int)fd : -1;
}

static int safe_writer(int fd, const struct stat* null_stat, int allow_shared_memory) {
  struct stat st;
  if (fstat(fd, &st) < 0) return -1;
  if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) return 1;
  if (S_ISCHR(st.st_mode) && (st.st_rdev == null_stat->st_rdev ||
      (fd <= STDERR_FILENO && stdio_tty[fd] && st.st_rdev == stdio_tty_device[fd]))) return 1;
  /* GTK uses memfd and unlinked shm files for display buffers. Inherited ones
   * are closed first; only internal buffers may survive initialization. */
  if (S_ISREG(st.st_mode) && (!allow_shared_memory || st.st_nlink != 0)) return 0;
  if (!allow_shared_memory) return 0;
  /* eventfd/epoll are writable anon-inode handles, not filesystem files. */
  struct statfs fs;
  if (fstatfs(fd, &fs) < 0) return -1;
  if (S_ISREG(st.st_mode)) {
    return fs.f_type == TMPFS_MAGIC || fs.f_type == HUGETLBFS_MAGIC;
  }
  return fs.f_type == ANON_INODE_FS_MAGIC;
}

int sandbox_prepare_inherited_fds(void) {
  DIR* directory = opendir("/proc/self/fd");
  if (!directory) return -1;
  int error = 0;
  struct dirent* entry;
  for (;;) {
    errno = 0;
    entry = readdir(directory);
    if (!entry) { if (errno) error = errno; break; }
    int fd = entry_fd(entry->d_name);
    if (fd > STDERR_FILENO && fd != dirfd(directory) && close(fd) < 0 && errno != EBADF && errno != EINTR) {
      error = errno; break;
    }
  }
  closedir(directory);
  if (error) { errno = error; return -1; }
  int null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);
  if (null_fd < 0) return -1;
  struct stat null_stat;
  if (fstat(null_fd, &null_stat) < 0) { error = errno; goto out; }
  for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd) {
    struct stat st;
    stdio_tty[fd] = fstat(fd, &st) == 0 && S_ISCHR(st.st_mode) && isatty(fd);
    if (stdio_tty[fd]) stdio_tty_device[fd] = st.st_rdev;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 && errno != EBADF) { error = errno; goto out; }
    if (flags >= 0 && (flags & O_ACCMODE) == O_RDONLY) continue;
    int safe = flags < 0 ? 0 : safe_writer(fd, &null_stat, 0);
    if (safe < 0) { error = errno; goto out; }
    if (!safe && dup2(null_fd, fd) < 0) { error = errno; goto out; }
  }
out:
  if (null_fd > STDERR_FILENO) close(null_fd);
  if (error) { errno = error; return -1; }
  return 0;
}

int sandbox_check_fds(int* bad_fd) {
  if (bad_fd) *bad_fd = -1;
  struct stat null_stat;
  if (stat("/dev/null", &null_stat) < 0) return -1;
  DIR* directory = opendir("/proc/self/fd");
  if (!directory) return -1;
  int error = 0;
  struct dirent* entry;
  for (;;) {
    errno = 0;
    entry = readdir(directory);
    if (!entry) { if (errno) error = errno; break; }
    int fd = entry_fd(entry->d_name);
    if (fd < 0 || fd == dirfd(directory)) continue;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) { if (errno == EBADF) continue; error = errno; break; }
    if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_PATH)) continue;
    int safe = safe_writer(fd, &null_stat, 1);
    if (safe < 0 && errno == EBADF) continue;
    if (safe <= 0) {
      error = safe == 0 ? EACCES : errno;
      if (bad_fd) *bad_fd = fd;
      break;
    }
  }
  closedir(directory);
  if (error) { errno = error; return -1; }
  return 0;
}
