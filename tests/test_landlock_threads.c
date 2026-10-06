/* SPDX-License-Identifier: Zlib */
#define _GNU_SOURCE
#include "zatura/landlock.h"
#include "zatura/sandbox-fds.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int waiting, enforced, worker_ok;
static char file_path[256], new_path[256];
static void* worker(void* data) {
  (void)data;
  pthread_mutex_lock(&lock);
  waiting = 1;
  pthread_cond_broadcast(&ready);
  while (!enforced) pthread_cond_wait(&ready, &lock);
  pthread_mutex_unlock(&lock);
  errno = 0;
  int fd = open(file_path, O_WRONLY | O_TRUNC);
  int ok = fd == -1 && errno == EACCES;
  if (fd >= 0) close(fd);
  errno = 0;
  fd = open(new_path, O_WRONLY | O_CREAT, 0600);
  ok &= fd == -1 && errno == EACCES;
  if (fd >= 0) close(fd);
  errno = 0;
  ok &= mkdir(new_path, 0700) == -1 && errno == EACCES;
  char contents[8] = {0};
  fd = open(file_path, O_RDONLY);
  ok &= fd >= 0 && read(fd, contents, sizeof(contents)) == 4 && memcmp(contents, "SAFE", 4) == 0;
  if (fd >= 0) close(fd);
  worker_ok = ok;
  return NULL;
}
int main(void) {
  long abi = syscall(SYS_landlock_create_ruleset, NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (abi < 8) {
    fprintf(stderr, "Landlock ABI %ld: positive TSYNC runtime test requires ABI 8+\n", abi);
    return 77;
  }
  char directory[] = "/tmp/zatura-landlock-thread-XXXXXX";
  if (!mkdtemp(directory)) return 1;
  snprintf(file_path, sizeof(file_path), "%s/existing", directory);
  snprintf(new_path, sizeof(new_path), "%s/new", directory);
  int fd = open(file_path, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0 || write(fd, "SAFE", 4) != 4) return 1;
  close(fd);
  pid_t child = fork();
  if (child < 0) return 1;
  if (child == 0) {
    if (sandbox_prepare_inherited_fds() != 0) _exit(2);
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, NULL) != 0) _exit(3);
    pthread_mutex_lock(&lock);
    while (!waiting) pthread_cond_wait(&ready, &lock);
    pthread_mutex_unlock(&lock);
    /* The worker is already alive: this exercises TSYNC, not inheritance by
     * a new child. No seccomp is active, so only Landlock can deny these ops. */
    if (landlock_drop_write() != 0 || sandbox_check_fds(NULL) != 0) _exit(4);
    pthread_mutex_lock(&lock);
    enforced = 1;
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
    if (pthread_join(thread, NULL) != 0 || !worker_ok) _exit(5);
    /* Later rendering workers must inherit the same domain as well. */
    worker_ok = 0;
    if (pthread_create(&thread, NULL, worker, NULL) != 0 ||
        pthread_join(thread, NULL) != 0 || !worker_ok) _exit(6);
    _exit(0);
  }
  int status;
  int ok = waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  char actual[8] = {0};
  fd = open(file_path, O_RDONLY);
  ok &= fd >= 0 && read(fd, actual, sizeof(actual)) == 4 && memcmp(actual, "SAFE", 4) == 0;
  if (fd >= 0) close(fd);
  ok &= access(new_path, F_OK) == -1 && errno == ENOENT;
  unlink(new_path);
  rmdir(new_path);
  unlink(file_path);
  rmdir(directory);
  if (!ok) fprintf(stderr, "Landlock TSYNC worker regression failed (status=%d)\n", status);
  return ok ? 0 : 1;
}
