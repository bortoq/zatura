/* SPDX-License-Identifier: Zlib */
#ifndef ZATURA_SANDBOX_FDS_H
#define ZATURA_SANDBOX_FDS_H
/* Call before UI/plugin initialization: close inherited non-stdio descriptors
 * and replace writable filesystem stdio with /dev/null. */
int sandbox_prepare_inherited_fds(void);
/* Call after TSYNC enforcement, before parsing: reject writable linked files
 * or unexpected devices. Pipes, sockets and unlinked shared memory are allowed.
 * On failure, the caller must abort startup; bad_fd identifies the descriptor. */
int sandbox_check_fds(int* bad_fd);
#endif
