/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include "core/utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int   total;
    int   value;
    char *message;
} ProgressData;

static void put_cp(LapkaCell *buf, int w, int h, int x, int y, uint32_t cp,
                   uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    LapkaCell c = { cp, fg, bg, attrs };
    buf[(size_t)y * (size_t)w + (size_t)x] = c;
}

static int draw_str(LapkaCell *buf, int w, int h, int x, int y,
                    const char *s, uint32_t fg, uint32_t bg, uint16_t attrs,
                    int max_cols) {
    if (!s) return 0;
    const uint8_t *p = (const uint8_t *)s;
    int cx = x;
    int used = 0;
    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        int cw = lapka_cp_width(cp);
        if (cw == 0) continue;
        if (max_cols >= 0 && used + cw > max_cols) break;
        for (int i = 0; i < cw; i++) {
            put_cp(buf, w, h, cx + i, y, i == 0 ? cp : 0, fg, bg, attrs);
        }
        cx += cw;
        used += cw;
    }
    return used;
}

static void progress_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                          const LapkaTheme *theme) {
    ProgressData *d = p->data;
    if (w <= 0 || h <= 0) return;

    int pct = 0;
    if (d->total > 0) {
        pct = (d->value * 100) / d->total;
    } else if (d->value > 0) {
        pct = 100;
    }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%3d%%", pct);

    int bar_w = w - 5;
    if (bar_w < 1) bar_w = 1;

    int filled = 0;
    if (d->total > 0) {
        filled = (d->value * bar_w) / d->total;
    } else if (d->value > 0) {
        filled = bar_w;
    }
    if (filled < 0) filled = 0;
    if (filled > bar_w) filled = bar_w;

    for (int i = 0; i < bar_w; i++) {
        uint32_t cp = (i < filled) ? 0x2588 : 0x2591;
        uint32_t fg = (i < filled) ? theme->accent : theme->muted;
        put_cp(buf, w, h, i, 0, cp, fg, 0, 0);
    }

    draw_str(buf, w, h, bar_w + 1, 0, pct_str, 0, 0, 0, w - bar_w - 1);

    if (h >= 2 && d->message && d->message[0]) {
        draw_str(buf, w, h, 0, 1, d->message, theme->muted, 0, 0, w);
    }
}

static void progress_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                             int *out_w, int *out_h) {
    ProgressData *d = p->data;
    int w = 40;
    if (w > bound_w) w = bound_w;
    int h = 1;
    if (d->message && d->message[0] && bound_h >= 2) h = 2;
    *out_w = w;
    *out_h = h;
}

static bool progress_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    (void)p;
    (void)ev;
    return false;
}

static void progress_free(LapkaPrimitive *p) {
    ProgressData *d = p->data;
    if (!d) { free(p); return; }
    free(d->message);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_progress_new(int total) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    ProgressData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    d->total = total > 0 ? total : 0;
    d->value = 0;
    d->message = NULL;

    p->kind = LAPKA_PRIMITIVE_PROGRESS;
    p->data = d;
    p->handle = progress_handle;
    p->draw = progress_draw;
    p->measure = progress_measure;
    p->free = progress_free;
    return p;
}

void lapka_progress_set(LapkaPrimitive *p, int value) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PROGRESS) return;
    ProgressData *d = p->data;
    if (value < 0) value = 0;
    if (d->total > 0 && value > d->total) value = d->total;
    d->value = value;
}

int lapka_progress_value(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PROGRESS) return 0;
    ProgressData *d = p->data;
    return d->value;
}

int lapka_progress_total(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PROGRESS) return 0;
    ProgressData *d = p->data;
    return d->total;
}

void lapka_progress_set_message(LapkaPrimitive *p, const char *msg) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PROGRESS) return;
    ProgressData *d = p->data;
    free(d->message);
    d->message = msg ? strdup(msg) : NULL;
}

const char *lapka_progress_message(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PROGRESS) return NULL;
    ProgressData *d = p->data;
    return d->message;
}