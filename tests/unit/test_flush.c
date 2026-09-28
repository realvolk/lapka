/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <render/framebuffer.h>
#include <render/flush.h>
#include <term/capabilities.h>
#include <core/cell.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

static size_t drain_pipe(int fd, char *buf, size_t cap) {
    size_t off = 0;
    while (off + 1 < cap) {
        ssize_t r = read(fd, buf + off, cap - off - 1);
        if (r <= 0) break;
        off += (size_t)r;
    }
    buf[off] = '\0';
    return off;
}

int main(void) {
    unsetenv("TERM");

    int fds[2];
    check(pipe(fds) == 0, "pipe");

    LapkaFramebuffer fb;
    check(lapka_fb_init(&fb, 4, 1) == 0, "fb init");

    lapka_fb_put(&fb, 0, 0, lapka_cell_make('A', 0, 0, 0));
    lapka_fb_put(&fb, 1, 0, lapka_cell_make('B', 0, 0, 0));

    check(lapka_flush(&fb, fds[1], LAPKA_COLOR_TRUECOLOR) == 0, "flush");
    close(fds[1]);

    char got[256];
    size_t n = drain_pipe(fds[0], got, sizeof(got));
    close(fds[0]);

    check(n > 0, "flush produced output");
    check(strstr(got, "A") != NULL, "flush contains A");
    check(strstr(got, "B") != NULL, "flush contains B");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}