/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <util/buf.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

int main(void) {
    LapkaBuf b;
    lapka_buf_init(&b);

    check(b.data == NULL && b.len == 0 && b.cap == 0, "init empty");

    check(lapka_buf_append_str(&b, "hello") == 0, "append hello");
    check(b.len == 5, "len 5");
    check(memcmp(b.data, "hello", 5) == 0, "content hello");

    check(lapka_buf_append_char(&b, ' ') == 0, "append space");
    check(lapka_buf_append_str(&b, "world") == 0, "append world");
    check(b.len == 11, "len 11");
    check(memcmp(b.data, "hello world", 11) == 0, "content hello world");

    lapka_buf_clear(&b);
    check(b.len == 0, "clear len");

    for (int i = 0; i < 10000; i++) {
        check(lapka_buf_append_char(&b, 'x') == 0, "grow");
    }
    check(b.len == 10000, "len 10000");

    lapka_buf_free(&b);
    check(b.data == NULL && b.len == 0 && b.cap == 0, "free resets");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}