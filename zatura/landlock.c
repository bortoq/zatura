/* SPDX-License-Identifier: Zlib */

#define _GNU_SOURCE
#include "landlock.h"

#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <girara/log.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#ifndef landlock_create_ruleset
static inline int landlock_create_ruleset(const struct landlock_ruleset_attr* const attr, const size_t size,
                                          const __u32 flags) {
  return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}
#endif

#ifndef landlock_restrict_self
static inline int landlock_restrict_self(const int ruleset_fd, const __u32 flags) {
  return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif

#ifndef LANDLOCK_RESTRICT_SELF_TSYNC
#define LANDLOCK_RESTRICT_SELF_TSYNC (1U << 3)
#endif

#ifndef LANDLOCK_ACCESS_FS_IOCTL_DEV
#define LANDLOCK_ACCESS_FS_IOCTL_DEV (1ULL << 15)
#endif

#ifndef LANDLOCK_SCOPE_SIGNAL
#define LANDLOCK_SCOPE_SIGNAL (1ULL << 1)
#endif

#ifndef LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET
#define LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET (1ULL << 0)
#endif

static int landlock_drop(__u64 fs_access, __u64 net_access, __u64 scoped) {
  const struct landlock_ruleset_attr ruleset_attr = {
      .handled_access_fs  = fs_access,
      .handled_access_net = net_access,
      .scoped             = scoped,
  };

  int ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
  if (ruleset_fd < 0) {
    girara_error("landlock_create_ruleset failed: %s", g_strerror(errno));
    return -1;
  }
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    girara_error("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", g_strerror(errno));
    close(ruleset_fd);
    return -1;
  }
  /* GTK/GLib may already have background threads. Rendering threads are
   * created later, while opening a document. ABI 8 TSYNC covers all current
   * threads, which then pass their domain to subsequently created threads. */
  const __u32 flags = LANDLOCK_RESTRICT_SELF_TSYNC;
  if (landlock_restrict_self(ruleset_fd, flags)) {
    girara_error("landlock_restrict_self failed: %s", g_strerror(errno));
    close(ruleset_fd);
    return -1;
  }
  close(ruleset_fd);
  return 0;
}

#define _LANDLOCK_ACCESS_FS_WRITE                                                                                      \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |                    \
   LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |                          \
   LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |                       \
   LANDLOCK_ACCESS_FS_MAKE_SYM)

/* returns landlock ABI version (>=1) if supported, 0 if not, -1 on unexpected probe error */
static int landlock_check_kernel(void) {
  int abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (abi >= 1) {
    return abi;
  }
  if (abi == -1 && (errno == ENOSYS || errno == EOPNOTSUPP)) {
    /*
     * Kernel too old, not compiled with Landlock,
     * or Landlock was not enabled at boot time.
     */
    return 0; /* Graceful fallback: Do nothing. */
  }
  return -1;
}

/* abstract unix sockets carry the X11 transport, so scope them only under Wayland */
static bool session_is_wayland(void) {
#ifdef GDK_WINDOWING_X11
  return !GDK_IS_X11_DISPLAY(gdk_display_get_default());
#else
  return true;
#endif
}

int landlock_drop_write(void) {
  const int abi = landlock_check_kernel();
  if (abi < 0) {
    girara_error("Failed to probe Landlock ABI.");
    return -1;
  }
  if (abi < 8) {
    girara_warning("Landlock ABI 8 with TSYNC is required; strict sandbox cannot start.");
    return 1; /* unsupported; the strict sandbox must refuse to start */
  }

  const __u64 fs = _LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_REFER |
                   LANDLOCK_ACCESS_FS_TRUNCATE | LANDLOCK_ACCESS_FS_IOCTL_DEV;
  const __u64 net = LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;
  __u64 scoped    = LANDLOCK_SCOPE_SIGNAL;
  if (session_is_wayland()) {
    scoped |= LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET;
  }
  return landlock_drop(fs, net, scoped);
}
