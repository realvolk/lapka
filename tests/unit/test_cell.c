/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

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
    LapkaCell a = lapka_cell_make('A', 0xFF0000, 0x000000, LAPKA_ATTR_BOLD);
    LapkaCell b = lapka_cell_make('A', 0xFF0000, 0x000000, LAPKA_ATTR_BOLD);
    LapkaCell c = lapka_cell_make('B', 0xFF0000, 0x000000, LAPKA_ATTR_BOLD);
    LapkaCell d = lapka_cell_make('A', 0x00FF00, 0x000000, LAPKA_ATTR_BOLD);

    check(lapka_cell_equal(a, b), "equal same");
    check(!lapka_cell_equal(a, c), "not equal cp");
    check(!lapka_cell_equal(a, d), "not equal fg");

    check(!lapka_cell_empty(a), "not empty");
    check(lapka_cell_empty(lapka_cell_blank()), "blank is empty");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}