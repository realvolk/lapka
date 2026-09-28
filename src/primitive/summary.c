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

typedef struct {
    char **lines;
    int    line_count;
    int    cap;
    int    scroll;
} SummaryData;

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

static void append_line(SummaryData *d, const char *line, size_t len) {
    if (d->line_count >= d->cap) {
        int ncap = d->cap ? d->cap * 2 : 32;
        char **nl = realloc(d->lines, (size_t)ncap * sizeof(char *));
        if (!nl) return;
        d->lines = nl;
        d->cap = ncap;
    }
    char *s = malloc(len + 1);
    if (!s) return;
    memcpy(s, line, len);
    s[len] = '\0';
    d->lines[d->line_count++] = s;
}

static void summary_clear(SummaryData *d) {
    for (int i = 0; i < d->line_count; i++) free(d->lines[i]);
    free(d->lines);
    d->lines = NULL;
    d->line_count = 0;
    d->cap = 0;
    d->scroll = 0;
}

static void summary_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                         const LapkaTheme *theme) {
    SummaryData *d = p->data;
    (void)theme;
    if (w <= 0 || h <= 0) return;

    int total = d->line_count;
    int max_scroll = total - h;
    if (max_scroll < 0) max_scroll = 0;
    if (d->scroll < 0) d->scroll = 0;
    if (d->scroll > max_scroll) d->scroll = max_scroll;

    int first = d->scroll;
    int last = first + h;
    if (last > total) last = total;

    for (int i = first; i < last; i++) {
        int y = i - first;
        draw_str(buf, w, h, 0, y, d->lines[i], 0, 0, 0, w);
    }
}

static void summary_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                            int *out_w, int *out_h) {
    SummaryData *d = p->data;
    int maxw = 0;
    for (int i = 0; i < d->line_count; i++) {
        const uint8_t *q = (const uint8_t *)d->lines[i];
        int col = 0;
        while (*q) {
            uint32_t cp;
            int bytes = lapka_utf8_decode(q, 4, &cp);
            if (bytes <= 0) break;
            q += bytes;
            col += lapka_cp_width(cp);
        }
        if (col > maxw) maxw = col;
    }
    if (maxw > bound_w) maxw = bound_w;
    int h = d->line_count;
    if (h > bound_h) h = bound_h;
    *out_w = maxw;
    *out_h = h;
}

static bool summary_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    SummaryData *d = p->data;
    if (ev->type != LAPKA_EVENT_KEY) return false;

    int total = d->line_count;

    if (ev->key.code == LAPKA_KEY_UP || ev->key.code == LAPKA_KEY_CODEPOINT) {
        if (ev->key.code == LAPKA_KEY_UP || ev->key.ch == 'k') {
            if (d->scroll > 0) d->scroll--;
            return true;
        }
        if (ev->key.ch == 'j') {
            if (d->scroll + 1 < total) d->scroll++;
            return true;
        }
    }
    if (ev->key.code == LAPKA_KEY_DOWN) {
        if (d->scroll + 1 < total) d->scroll++;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_PAGEUP) {
        d->scroll -= 10;
        if (d->scroll < 0) d->scroll = 0;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_PAGEDOWN) {
        d->scroll += 10;
        if (d->scroll >= total) d->scroll = total > 0 ? total - 1 : 0;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_HOME) {
        d->scroll = 0;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_END) {
        d->scroll = total > 0 ? total - 1 : 0;
        return true;
    }
    return false;
}

static void summary_free(LapkaPrimitive *p) {
    SummaryData *d = p->data;
    if (!d) { free(p); return; }
    summary_clear(d);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_summary_new(void) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    SummaryData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    p->kind = LAPKA_PRIMITIVE_SUMMARY;
    p->data = d;
    p->handle = summary_handle;
    p->draw = summary_draw;
    p->measure = summary_measure;
    p->free = summary_free;
    return p;
}

void lapka_summary_set(LapkaPrimitive *p, const char *text) {
    if (!p || p->kind != LAPKA_PRIMITIVE_SUMMARY) return;
    SummaryData *d = p->data;
    summary_clear(d);

    if (!text) return;

    const char *s = text;
    while (*s) {
        const char *nl = strchr(s, '\n');
        size_t len = nl ? (size_t)(nl - s) : strlen(s);
        append_line(d, s, len);
        if (!nl) break;
        s = nl + 1;
    }
}

void lapka_summary_append(LapkaPrimitive *p, const char *line) {
    if (!p || p->kind != LAPKA_PRIMITIVE_SUMMARY) return;
    SummaryData *d = p->data;
    if (!line) return;
    append_line(d, line, strlen(line));
}

int lapka_summary_line_count(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_SUMMARY) return 0;
    SummaryData *d = p->data;
    return d->line_count;
}

int lapka_summary_scroll(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_SUMMARY) return 0;
    SummaryData *d = p->data;
    return d->scroll;
}

void lapka_summary_reset_scroll(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_SUMMARY) return;
    SummaryData *d = p->data;
    d->scroll = 0;
}