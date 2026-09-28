/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>
#include <stdbool.h>

bool      lapka_rect_empty(LapkaRect r);
bool      lapka_rect_contains(LapkaRect r, int x, int y);
LapkaRect lapka_rect_intersect(LapkaRect a, LapkaRect b);
LapkaRect lapka_rect_shrink(LapkaRect r, int top, int right, int bottom, int left);