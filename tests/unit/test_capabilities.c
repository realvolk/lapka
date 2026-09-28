/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <term/capabilities.h>

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

int main(void) {
    unsetenv("COLORTERM");
    unsetenv("TERM");
    check(lapka_cap_color_depth() == LAPKA_COLOR_MONO, "no env -> mono");
    check(lapka_cap_is_basic_term(), "no env is basic");
    check(!lapka_cap_sync_supported(), "no env no sync");

    setenv("TERM", "xterm-256color", 1);
    unsetenv("COLORTERM");
    check(lapka_cap_color_depth() == LAPKA_COLOR_256, "xterm-256color");
    check(!lapka_cap_is_basic_term(), "xterm-256color not basic");
    check(lapka_cap_sync_supported(), "xterm-256color sync");

    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);
    check(lapka_cap_color_depth() == LAPKA_COLOR_TRUECOLOR, "truecolor wins");

    setenv("TERM", "xterm", 1);
    unsetenv("COLORTERM");
    check(lapka_cap_color_depth() == LAPKA_COLOR_16, "xterm -> 16");

    setenv("TERM", "linux", 1);
    unsetenv("COLORTERM");
    check(lapka_cap_is_basic_term(), "linux basic");
    check(!lapka_cap_sync_supported(), "linux no sync");

    setenv("TERM", "vt100", 1);
    check(lapka_cap_is_basic_term(), "vt100 basic");

    setenv("TERM", "screen-256color", 1);
    unsetenv("COLORTERM");
    check(lapka_cap_color_depth() == LAPKA_COLOR_256, "screen-256color");
    check(lapka_cap_sync_supported(), "screen-256color sync");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}