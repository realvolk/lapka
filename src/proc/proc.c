/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "proc.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

int lapka_proc_start(const char *const *argv, LapkaProc *p) {
    int fds[2];
    if (pipe(fds) != 0) return -1;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    sigset_t def;
    sigemptyset(&def);
    sigaddset(&def, SIGINT);
    sigaddset(&def, SIGTERM);
    sigaddset(&def, SIGPIPE);
    posix_spawnattr_setsigdefault(&attr, &def);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF);

    int rc = posix_spawnp(&p->pid, argv[0], &actions, &attr,
                          (char *const *)argv, environ);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    close(fds[1]);

    if (rc != 0) {
        close(fds[0]);
        return -1;
    }

    int flags = fcntl(fds[0], F_GETFL, 0);
    fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);

    p->fd = fds[0];
    p->status = 0;
    p->done = false;
    return 0;
}

int lapka_proc_read(LapkaProc *p, char *buf, size_t cap) {
    if (p->fd < 0) return 0;
    ssize_t n = read(p->fd, buf, cap);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    return (int)n;
}

int lapka_proc_reap(LapkaProc *p) {
    if (p->done) return 1;
    int status;
    pid_t r = waitpid(p->pid, &status, WNOHANG);
    if (r == 0) return 0;
    if (r < 0) return -1;
    p->status = status;
    p->done = true;
    return 1;
}

void lapka_proc_terminate(LapkaProc *p) {
    if (p->done) return;
    kill(p->pid, SIGTERM);
    for (int i = 0; i < 50; i++) {
        int status;
        pid_t r = waitpid(p->pid, &status, WNOHANG);
        if (r > 0) { p->done = true; p->status = status; return; }
        struct timespec ts = { 0, 10000000 };
        nanosleep(&ts, NULL);
    }
    kill(p->pid, SIGKILL);
    int status;
    waitpid(p->pid, &status, 0);
    p->status = status;
    p->done = true;
}

void lapka_proc_close(LapkaProc *p) {
    if (p->fd >= 0) close(p->fd);
    p->fd = -1;
}