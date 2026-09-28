/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <core/box.h>

#include <stdio.h>

static int failures = 0;

static void check_rect(LapkaRect got, LapkaRect want, const char *what) {
    if (got.x != want.x || got.y != want.y || got.w != want.w || got.h != want.h) {
        fprintf(stderr, "FAIL %s: got {%d,%d,%d,%d}, want {%d,%d,%d,%d}\n",
                what, got.x, got.y, got.w, got.h, want.x, want.y, want.w, want.h);
        failures++;
    }
}

static LapkaBox box_of(LapkaAnchor anchor, LapkaSizeMode m, int w, int h) {
    LapkaBox b;
    b.anchor = anchor;
    b.width.mode = m;
    b.width.val = w;
    b.height.mode = m;
    b.height.val = h;
    b.border = LAPKA_BORDER_NONE;
    for (int i = 0; i < 4; i++) { b.pad[i] = 0; b.margin[i] = 0; }
    return b;
}

int main(void) {
    LapkaRect screen = { 0, 0, 80, 24 };

    LapkaBox full = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FULL, 0, 0);
    check_rect(lapka_place(screen, full, 0, 0), screen, "full");

    LapkaBox tl = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FIXED, 10, 5);
    check_rect(lapka_place(screen, tl, 0, 0), (LapkaRect){0, 0, 10, 5}, "top-left fixed");

    LapkaBox tc = box_of(LAPKA_ANCHOR_TC, LAPKA_SIZE_FIXED, 10, 5);
    check_rect(lapka_place(screen, tc, 0, 0), (LapkaRect){35, 0, 10, 5}, "top-center fixed");

    LapkaBox mc = box_of(LAPKA_ANCHOR_CC, LAPKA_SIZE_FIXED, 10, 5);
    check_rect(lapka_place(screen, mc, 0, 0), (LapkaRect){35, 9, 10, 5}, "mid-center fixed");

    LapkaBox br = box_of(LAPKA_ANCHOR_BR, LAPKA_SIZE_FIXED, 10, 5);
    check_rect(lapka_place(screen, br, 0, 0), (LapkaRect){70, 19, 10, 5}, "bot-right fixed");

    LapkaBox pct = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_PERCENT, 50, 50);
    check_rect(lapka_place(screen, pct, 0, 0), (LapkaRect){0, 0, 40, 12}, "percent");

    LapkaBox fit = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FIT, 0, 0);
    check_rect(lapka_place(screen, fit, 20, 3), (LapkaRect){0, 0, 20, 3}, "fit");

    LapkaBox bd = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FIXED, 10, 5);
    bd.border = LAPKA_BORDER_SINGLE;
    check_rect(lapka_place(screen, bd, 0, 0), (LapkaRect){1, 1, 8, 3}, "border");

    LapkaBox pad = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FIXED, 10, 5);
    pad.pad[0] = 1; pad.pad[1] = 2; pad.pad[2] = 1; pad.pad[3] = 2;
    check_rect(lapka_place(screen, pad, 0, 0), (LapkaRect){2, 1, 6, 3}, "padding");

    LapkaBox mar = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FULL, 0, 0);
    mar.margin[0] = 1; mar.margin[1] = 2; mar.margin[2] = 1; mar.margin[3] = 2;
    check_rect(lapka_place(screen, mar, 0, 0), (LapkaRect){2, 1, 76, 22}, "margin");

    LapkaBox tiny = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FIXED, 2, 2);
    tiny.border = LAPKA_BORDER_SINGLE;
    check_rect(lapka_place(screen, tiny, 0, 0), (LapkaRect){0, 0, 2, 2}, "tiny box drops border");

    LapkaBox overflowing = box_of(LAPKA_ANCHOR_TL, LAPKA_SIZE_FIXED, 1000, 1000);
    check_rect(lapka_place(screen, overflowing, 0, 0), screen, "clamp to parent");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}