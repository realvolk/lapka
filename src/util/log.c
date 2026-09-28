/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "log.h"

#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

static int s_fd = -1;

void lapka_log_open(const char *path) {
    if (s_fd >= 0) return;
    if (!path) return;
    s_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
}

void lapka_log_close(void) {
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
}

int lapka_log_fd(void) {
    return s_fd;
}

int lapka_log_write(const char *s, unsigned long n) {
    if (s_fd < 0) return -1;
    unsigned long off = 0;
    while (off < n) {
        ssize_t r = write(s_fd, s + off, (size_t)(n - off));
        if (r < 0) return -1;
        if (r == 0) return -1;
        off += (unsigned long)r;
    }
    return 0;
}