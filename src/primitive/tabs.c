/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include "core/utf8.h"

#include <stdlib.h>
#include <string.h>

#define TABS_MAX 9

typedef struct {
    char           **labels;
    LapkaPrimitive  *children[TABS_MAX];
    int              count;
    int              active;
} TabsData;

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

static void tabs_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                      const LapkaTheme *theme) {
    TabsData *d = p->data;
    if (w <= 0 || h <= 0) return;

    int x = 0;
    for (int i = 0; i < d->count && x < w; i++) {
        int is_active = (i == d->active);
        uint32_t fg = is_active ? theme->accent : theme->muted;
        uint16_t attrs = is_active ? LAPKA_ATTR_BOLD : 0;

        if (i > 0 && x + 3 < w) {
            put_cp(buf, w, h, x, 0, ' ', 0, 0, 0);
            x++;
            put_cp(buf, w, h, x, 0, 0x2502, theme->muted, 0, 0);
            x++;
            put_cp(buf, w, h, x, 0, ' ', 0, 0, 0);
            x++;
        }
        x += draw_str(buf, w, h, x, 0, d->labels[i], fg, 0, attrs, w - x);
    }

    if (h > 1 && d->active >= 0 && d->active < d->count) {
        LapkaPrimitive *child = d->children[d->active];
        if (child && child->draw) {
            LapkaCell *sub = buf + (size_t)w;
            child->draw(child, sub, w, h - 1, theme);
        }
    }
}

static void tabs_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                         int *out_w, int *out_h) {
    TabsData *d = p->data;
    int total = 0;
    for (int i = 0; i < d->count; i++) {
        if (i > 0) total += 3;
        total += (int)strlen(d->labels[i]);
    }
    if (total > bound_w) total = bound_w;

    int child_w = 0, child_h = 0;
    if (d->active >= 0 && d->active < d->count && d->children[d->active]) {
        d->children[d->active]->measure(d->children[d->active],
                                        bound_w, bound_h - 1,
                                        &child_w, &child_h);
    }

    int w = total > child_w ? total : child_w;
    int h = 1 + child_h;
    if (h > bound_h) h = bound_h;
    *out_w = w;
    *out_h = h;
}

static bool tabs_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    TabsData *d = p->data;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code >= LAPKA_KEY_F1 && ev->key.code <= LAPKA_KEY_F9) {
            int idx = ev->key.code - LAPKA_KEY_F1;
            if (idx < d->count) {
                d->active = idx;
                return true;
            }
        }
    }

    if (d->active >= 0 && d->active < d->count && d->children[d->active]) {
        return d->children[d->active]->handle(d->children[d->active], ev);
    }
    return false;
}

static void tabs_free(LapkaPrimitive *p) {
    TabsData *d = p->data;
    if (!d) { free(p); return; }
    for (int i = 0; i < d->count; i++) free(d->labels[i]);
    free(d->labels);
    for (int i = 0; i < TABS_MAX; i++) {
        if (d->children[i]) lapka_primitive_free(d->children[i]);
    }
    free(d);
    free(p);
}

LapkaPrimitive *lapka_tabs_new(const char **labels, int count) {
    if (count < 1) count = 1;
    if (count > TABS_MAX) count = TABS_MAX;

    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    TabsData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    d->labels = calloc((size_t)count, sizeof(char *));
    if (!d->labels) { free(d); free(p); return NULL; }
    for (int i = 0; i < count; i++) {
        d->labels[i] = strdup(labels[i] ? labels[i] : "");
    }
    d->count = count;
    d->active = 0;

    p->kind = LAPKA_PRIMITIVE_TABS;
    p->data = d;
    p->handle = tabs_handle;
    p->draw = tabs_draw;
    p->measure = tabs_measure;
    p->free = tabs_free;
    return p;
}

void lapka_tabs_set_child(LapkaPrimitive *p, int idx, LapkaPrimitive *child) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TABS) return;
    TabsData *d = p->data;
    if (idx < 0 || idx >= TABS_MAX) return;
    if (d->children[idx]) lapka_primitive_free(d->children[idx]);
    d->children[idx] = child;
}

void lapka_tabs_set_active(LapkaPrimitive *p, int idx) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TABS) return;
    TabsData *d = p->data;
    if (idx < 0 || idx >= d->count) return;
    d->active = idx;
}

int lapka_tabs_active(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TABS) return -1;
    TabsData *d = p->data;
    return d->active;
}

int lapka_tabs_count(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TABS) return 0;
    TabsData *d = p->data;
    return d->count;
}