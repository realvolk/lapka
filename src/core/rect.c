/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "rect.h"

bool lapka_rect_empty(LapkaRect r) {
    return r.w <= 0 || r.h <= 0;
}

bool lapka_rect_contains(LapkaRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w
        && y >= r.y && y < r.y + r.h;
}

LapkaRect lapka_rect_intersect(LapkaRect a, LapkaRect b) {
    int x1 = a.x > b.x ? a.x : b.x;
    int y1 = a.y > b.y ? a.y : b.y;
    int x2 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y2 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    int w = x2 - x1;
    int h = y2 - y1;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    LapkaRect r = { x1, y1, w, h };
    return r;
}

LapkaRect lapka_rect_shrink(LapkaRect r, int top, int right, int bottom, int left) {
    int w = r.w - left - right;
    int h = r.h - top - bottom;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    LapkaRect out = { r.x + left, r.y + top, w, h };
    return out;
}