/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>
#include <stdbool.h>

bool     lapka_cell_equal(LapkaCell a, LapkaCell b);
bool     lapka_cell_empty(LapkaCell c);
LapkaCell lapka_cell_blank(void);
LapkaCell lapka_cell_make(uint32_t cp, uint32_t fg, uint32_t bg, uint16_t attrs);