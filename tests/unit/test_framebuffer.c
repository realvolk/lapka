/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <render/framebuffer.h>
#include <core/cell.h>

#include <stdio.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

int main(void) {
    LapkaFramebuffer fb;
    check(lapka_fb_init(&fb, 80, 24) == 0, "init");

    check(fb.w == 80 && fb.h == 24, "dimensions");
    check(fb.full_dirty, "full dirty after init");
    check(lapka_cell_empty(lapka_fb_get(&fb, 0, 0)), "cell empty at origin");
    check(lapka_cell_empty(lapka_fb_get(&fb, 79, 23)), "cell empty at corner");

    LapkaCell c = lapka_cell_make('A', 0xFF0000, 0, LAPKA_ATTR_BOLD);
    check(lapka_fb_put(&fb, 5, 5, c) == 0, "put");
    LapkaCell got = lapka_fb_get(&fb, 5, 5);
    check(lapka_cell_equal(got, c), "get matches put");

    check(lapka_fb_put(&fb, -1, 0, c) == -1, "put off-screen left");
    check(lapka_fb_put(&fb, 0, -1, c) == -1, "put off-screen top");
    check(lapka_fb_put(&fb, 80, 0, c) == -1, "put off-screen right");
    check(lapka_fb_put(&fb, 0, 24, c) == -1, "put off-screen bottom");

    check(lapka_cell_empty(lapka_fb_get(&fb, -5, -5)), "get off-screen returns blank");

    check(lapka_fb_resize(&fb, 100, 30) == 0, "resize");
    check(fb.w == 100 && fb.h == 30, "resize dimensions");
    check(fb.full_dirty, "full dirty after resize");
    check(lapka_cell_empty(lapka_fb_get(&fb, 5, 5)), "resize clears content");

    lapka_fb_clear(&fb);
    check(lapka_cell_empty(lapka_fb_get(&fb, 0, 0)), "clear");

    lapka_fb_free(&fb);
    check(fb.cur == NULL && fb.prev == NULL, "free");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}