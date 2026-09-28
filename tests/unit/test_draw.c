/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <render/framebuffer.h>
#include <render/draw.h>
#include <core/cell.h>

#include <stdio.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

static void check_cp(LapkaFramebuffer *fb, int x, int y, uint32_t want, const char *what) {
    LapkaCell c = lapka_fb_get(fb, x, y);
    if (c.cp != want) {
        fprintf(stderr, "FAIL %s: got U+%04X, want U+%04X\n", what, c.cp, want);
        failures++;
    }
}

int main(void) {
    LapkaFramebuffer fb;
    check(lapka_fb_init(&fb, 20, 5) == 0, "init");

    lapka_draw_text(&fb, (LapkaRect){0, 0, 20, 1}, "hello", 0, 0, 0);
    check_cp(&fb, 0, 0, 'h', "text h");
    check_cp(&fb, 4, 0, 'o', "text o");

    lapka_draw_text(&fb, (LapkaRect){0, 1, 20, 2}, "a\nb", 0, 0, 0);
    check_cp(&fb, 0, 1, 'a', "nl a");
    check_cp(&fb, 0, 2, 'b', "nl b");

    lapka_fb_clear(&fb);
    lapka_draw_text(&fb, (LapkaRect){0, 0, 3, 3}, "abcdef", 0, 0, 0);
    check_cp(&fb, 0, 0, 'a', "wrap a");
    check_cp(&fb, 2, 0, 'c', "wrap c");
    check_cp(&fb, 0, 1, 'd', "wrap d");
    check_cp(&fb, 2, 1, 'f', "wrap f");

    lapka_fb_clear(&fb);
    lapka_draw_fill(&fb, (LapkaRect){2, 1, 3, 2}, lapka_cell_make('X', 0, 0, 0));
    check_cp(&fb, 2, 1, 'X', "fill origin");
    check_cp(&fb, 4, 2, 'X', "fill corner");

    lapka_fb_clear(&fb);
    lapka_draw_box(&fb, (LapkaRect){0, 0, 5, 4}, LAPKA_BORDER_SINGLE, 0, 0);
    check_cp(&fb, 0, 0, 0x250C, "box tl");
    check_cp(&fb, 4, 0, 0x2510, "box tr");
    check_cp(&fb, 0, 3, 0x2514, "box bl");
    check_cp(&fb, 4, 3, 0x2518, "box br");
    check_cp(&fb, 2, 0, 0x2500, "box top hz");
    check_cp(&fb, 0, 1, 0x2502, "box left vt");

    lapka_fb_free(&fb);

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}