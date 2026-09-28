/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "cell.h"

bool lapka_cell_equal(LapkaCell a, LapkaCell b) {
    return a.cp == b.cp
        && a.fg == b.fg
        && a.bg == b.bg
        && a.attrs == b.attrs;
}

bool lapka_cell_empty(LapkaCell c) {
    return c.cp == 0;
}

LapkaCell lapka_cell_blank(void) {
    LapkaCell c = { 0, 0, 0, 0 };
    return c;
}

LapkaCell lapka_cell_make(uint32_t cp, uint32_t fg, uint32_t bg, uint16_t attrs) {
    LapkaCell c = { cp, fg, bg, attrs };
    return c;
}