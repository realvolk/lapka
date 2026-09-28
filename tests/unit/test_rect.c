/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <core/rect.h>

#include <stdio.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

static void check_rect(LapkaRect got, LapkaRect want, const char *what) {
    if (got.x != want.x || got.y != want.y || got.w != want.w || got.h != want.h) {
        fprintf(stderr, "FAIL %s: got {%d,%d,%d,%d}, want {%d,%d,%d,%d}\n",
                what, got.x, got.y, got.w, got.h, want.x, want.y, want.w, want.h);
        failures++;
    }
}

int main(void) {
    LapkaRect a = { 0, 0, 10, 10 };
    LapkaRect b = { 5, 5, 10, 10 };
    LapkaRect c = { 20, 20, 5, 5 };

    check(!lapka_rect_empty(a), "not empty");
    check(lapka_rect_empty((LapkaRect){0, 0, 0, 0}), "zero empty");
    check(lapka_rect_empty((LapkaRect){0, 0, 10, -1}), "negative h empty");

    check(lapka_rect_contains(a, 0, 0), "contains corner");
    check(lapka_rect_contains(a, 9, 9), "contains inner edge");
    check(!lapka_rect_contains(a, 10, 10), "excludes outer edge");

    check_rect(lapka_rect_intersect(a, b), (LapkaRect){5, 5, 5, 5}, "intersect overlap");
    check_rect(lapka_rect_intersect(a, c), (LapkaRect){20, 20, 0, 0}, "intersect disjoint");
    check_rect(lapka_rect_intersect(a, a), a, "intersect self");

    check_rect(lapka_rect_shrink(a, 1, 2, 3, 4), (LapkaRect){4, 1, 4, 6}, "shrink");
    check_rect(lapka_rect_shrink(a, 20, 20, 20, 20), (LapkaRect){20, 20, 0, 0}, "shrink to zero");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}