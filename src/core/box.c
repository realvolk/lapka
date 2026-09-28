/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "box.h"

static int resolve_size(LapkaSize size, int bound, int natural) {
    switch (size.mode) {
        case LAPKA_SIZE_FIT:
            return natural > bound ? bound : natural;
        case LAPKA_SIZE_FULL:
            return bound;
        case LAPKA_SIZE_FIXED: {
            int v = size.val;
            if (v < 0) v = 0;
            if (v > bound) v = bound;
            return v;
        }
        case LAPKA_SIZE_PERCENT: {
            int v = size.val;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            return bound * v / 100;
        }
    }
    return 0;
}

static int anchor_x(LapkaAnchor a, int inner_x, int inner_w, int box_w) {
    switch (a) {
        case LAPKA_ANCHOR_TR:
        case LAPKA_ANCHOR_RC:
        case LAPKA_ANCHOR_BR:
            return inner_x + inner_w - box_w;
        case LAPKA_ANCHOR_TC:
        case LAPKA_ANCHOR_CC:
        case LAPKA_ANCHOR_BC:
            return inner_x + (inner_w - box_w) / 2;
        default:
            return inner_x;
    }
}

static int anchor_y(LapkaAnchor a, int inner_y, int inner_h, int box_h) {
    switch (a) {
        case LAPKA_ANCHOR_BL:
        case LAPKA_ANCHOR_BC:
        case LAPKA_ANCHOR_BR:
            return inner_y + inner_h - box_h;
        case LAPKA_ANCHOR_LC:
        case LAPKA_ANCHOR_CC:
        case LAPKA_ANCHOR_RC:
            return inner_y + (inner_h - box_h) / 2;
        default:
            return inner_y;
    }
}

LapkaRect lapka_place(LapkaRect parent, LapkaBox box, int natural_w, int natural_h) {
    LapkaRect empty = { 0, 0, 0, 0 };

    int inner_x = parent.x + box.margin[3];
    int inner_y = parent.y + box.margin[0];
    int inner_w = parent.w - box.margin[3] - box.margin[1];
    int inner_h = parent.h - box.margin[0] - box.margin[2];

    if (inner_w <= 0 || inner_h <= 0) {
        return empty;
    }

    int want_border = (box.border != LAPKA_BORDER_NONE) ? 1 : 0;
    int natural_w_full = natural_w + 2 * want_border + box.pad[3] + box.pad[1];
    int natural_h_full = natural_h + 2 * want_border + box.pad[0] + box.pad[2];

    int box_w = resolve_size(box.width, inner_w, natural_w_full);
    int box_h = resolve_size(box.height, inner_h, natural_h_full);

    int bx = anchor_x(box.anchor, inner_x, inner_w, box_w);
    int by = anchor_y(box.anchor, inner_y, inner_h, box_h);

    int bt_x = (want_border && box_w >= 3) ? 1 : 0;
    int bt_y = (want_border && box_h >= 3) ? 1 : 0;

    int cx = bx + bt_x + box.pad[3];
    int cy = by + bt_y + box.pad[0];
    int cw = box_w - 2 * bt_x - box.pad[3] - box.pad[1];
    int ch = box_h - 2 * bt_y - box.pad[0] - box.pad[2];

    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    LapkaRect out = { cx, cy, cw, ch };
    return out;
}