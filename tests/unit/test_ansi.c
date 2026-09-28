/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <term/ansi.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

static void check_str(const char *got, const char *want, const char *what) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s: got \"%s\", want \"%s\"\n", what, got, want);
        failures++;
    }
}

static void test_sgr_build(void) {
    char buf[64];

    lapka_ansi_sgr_build(buf, sizeof(buf), 0, 0, 0);
    check_str(buf, "\033[0m", "sgr reset");

    lapka_ansi_sgr_build(buf, sizeof(buf), 0, 0, LAPKA_ATTR_BOLD);
    check_str(buf, "\033[0;1m", "sgr bold");

    lapka_ansi_sgr_build(buf, sizeof(buf), 0, 0, LAPKA_ATTR_BOLD | LAPKA_ATTR_UNDERLINE);
    check_str(buf, "\033[0;1;4m", "sgr bold underline");

    lapka_ansi_sgr_build(buf, sizeof(buf), 0xFF0000, 0, 0);
    check_str(buf, "\033[0;38;2;255;0;0m", "sgr red fg");

    lapka_ansi_sgr_build(buf, sizeof(buf), 0xFF0000, 0x00FF00, 0);
    check_str(buf, "\033[0;38;2;255;0;0;48;2;0;255;0m", "sgr red on green");

    lapka_ansi_sgr_build(buf, sizeof(buf), 0x123456, 0, LAPKA_ATTR_ITALIC);
    check_str(buf, "\033[0;3;38;2;18;52;86m", "sgr italic custom");
}

static void test_write_all_pipe(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fprintf(stderr, "FAIL pipe\n");
        failures++;
        return;
    }

    const char *msg = "hello world";
    int rc = lapka_write_all(fds[1], msg, 11);
    close(fds[1]);

    char buf[32] = {0};
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);

    if (rc != 0 || n != 11 || memcmp(buf, msg, 11) != 0) {
        fprintf(stderr, "FAIL write_all pipe: rc=%d n=%zd\n", rc, n);
        failures++;
    }
}

static void test_cursor_to_pipe(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        failures++;
        return;
    }

    lapka_ansi_cursor_to(fds[1], 5, 10);
    close(fds[1]);

    char buf[32] = {0};
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);

    if (n <= 0 || strcmp(buf, "\033[11;6H") != 0) {
        fprintf(stderr, "FAIL cursor_to: got \"%s\"\n", buf);
        failures++;
    }
}

int main(void) {
    test_sgr_build();
    test_write_all_pipe();
    test_cursor_to_pipe();

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}