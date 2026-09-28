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
    char *label;
    int   parent;
    int   depth;
    bool  has_children;
    bool  expanded;
    void *userdata;
} TreeEntry;

typedef struct {
    TreeEntry *entries;
    int        count;
    int        cap;
    int        cursor;
    int        scroll;
    int       *flat;
    int        flat_cap;
} TreeData;

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

static int flatten(TreeData *d, int parent, int depth, int *out, int cap, int n) {
    for (int i = 0; i < d->count && n < cap; i++) {
        if (d->entries[i].parent != parent) continue;
        d->entries[i].depth = depth;
        out[n++] = i;
        if (d->entries[i].expanded && d->entries[i].has_children) {
            n = flatten(d, i, depth + 1, out, cap, n);
        }
    }
    return n;
}

static int flat_count(TreeData *d) {
    if (d->flat_cap < d->count || !d->flat) {
        d->flat_cap = d->count > 0 ? d->count : 16;
        int *nf = realloc(d->flat, (size_t)d->flat_cap * sizeof(int));
        if (!nf) return 0;
        d->flat = nf;
    }
    return flatten(d, -1, 0, d->flat, d->flat_cap, 0);
}

static void clamp(TreeData *d, int h) {
    int n = flat_count(d);
    if (n == 0) { d->cursor = 0; d->scroll = 0; return; }
    if (d->cursor >= n) d->cursor = n - 1;
    if (d->cursor < 0) d->cursor = 0;
    if (d->cursor < d->scroll) d->scroll = d->cursor;
    if (d->cursor >= d->scroll + h) d->scroll = d->cursor - h + 1;
    if (d->scroll < 0) d->scroll = 0;
}

static void tree_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                      const LapkaTheme *theme) {
    TreeData *d = p->data;
    if (w <= 0 || h <= 0) return;

    int n = flat_count(d);
    clamp(d, h);

    for (int row = 0; row < h; row++) {
        int idx = d->scroll + row;
        if (idx >= n) break;
        int node = d->flat[idx];
        TreeEntry *e = &d->entries[node];
        int is_cursor = (idx == d->cursor);

        int x = 0;
        for (int i = 0; i < e->depth * 2 && x < w; i++, x++) {
            put_cp(buf, w, h, x, row, ' ', 0, 0, 0);
        }

        if (x + 1 < w) {
            if (e->has_children) {
                uint32_t mark = e->expanded ? 0x25BC : 0x25B6;
                put_cp(buf, w, h, x, row, mark,
                       is_cursor ? theme->accent : theme->muted, 0, 0);
            } else {
                put_cp(buf, w, h, x, row, ' ', 0, 0, 0);
            }
            x++;
            put_cp(buf, w, h, x, row, ' ', 0, 0, 0);
            x++;
        }

        uint32_t fg = is_cursor ? theme->accent : 0;
        uint16_t attrs = is_cursor ? LAPKA_ATTR_BOLD : 0;
        draw_str(buf, w, h, x, row, e->label, fg, 0, attrs, w - x);
    }
}

static void tree_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                         int *out_w, int *out_h) {
    TreeData *d = p->data;
    int maxw = 0;
    for (int i = 0; i < d->count; i++) {
        const uint8_t *q = (const uint8_t *)d->entries[i].label;
        int col = d->entries[i].depth * 2 + 2;
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
    int h = flat_count(d);
    if (h > bound_h) h = bound_h;
    *out_w = maxw;
    *out_h = h;
}

static bool tree_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    TreeData *d = p->data;
    if (ev->type != LAPKA_EVENT_KEY) return false;

    int n = flat_count(d);

    if (ev->key.code == LAPKA_KEY_UP) {
        if (d->cursor > 0) d->cursor--;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_DOWN) {
        if (d->cursor + 1 < n) d->cursor++;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_HOME) {
        d->cursor = 0;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_END) {
        d->cursor = n > 0 ? n - 1 : 0;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_CODEPOINT && ev->key.ch == ' ') {
        if (d->cursor < n) {
            int node = d->flat[d->cursor];
            if (d->entries[node].has_children) {
                d->entries[node].expanded = !d->entries[node].expanded;
            }
        }
        return true;
    }
    return false;
}

static void tree_free(LapkaPrimitive *p) {
    TreeData *d = p->data;
    if (!d) { free(p); return; }
    for (int i = 0; i < d->count; i++) free(d->entries[i].label);
    free(d->entries);
    free(d->flat);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_tree_new(void) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    TreeData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    p->kind = LAPKA_PRIMITIVE_TREE;
    p->data = d;
    p->handle = tree_handle;
    p->draw = tree_draw;
    p->measure = tree_measure;
    p->free = tree_free;
    return p;
}

int lapka_tree_add(LapkaPrimitive *p, int parent, const char *label) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return -1;
    TreeData *d = p->data;

    if (d->count >= d->cap) {
        int ncap = d->cap ? d->cap * 2 : 16;
        TreeEntry *ne = realloc(d->entries, (size_t)ncap * sizeof(TreeEntry));
        if (!ne) return -1;
        d->entries = ne;
        d->cap = ncap;
    }

    int idx = d->count++;
    TreeEntry *e = &d->entries[idx];
    e->label = strdup(label ? label : "");
    e->parent = parent;
    e->depth = 0;
    e->has_children = false;
    e->expanded = false;
    e->userdata = NULL;

    if (parent >= 0 && parent < idx) {
        d->entries[parent].has_children = true;
    }
    return idx;
}

void lapka_tree_clear(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return;
    TreeData *d = p->data;
    for (int i = 0; i < d->count; i++) free(d->entries[i].label);
    free(d->entries);
    d->entries = NULL;
    d->count = 0;
    d->cap = 0;
    d->cursor = 0;
    d->scroll = 0;
}

void lapka_tree_set_userdata(LapkaPrimitive *p, int idx, void *ud) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return;
    TreeData *d = p->data;
    if (idx < 0 || idx >= d->count) return;
    d->entries[idx].userdata = ud;
}

void *lapka_tree_userdata(LapkaPrimitive *p, int idx) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return NULL;
    TreeData *d = p->data;
    if (idx < 0 || idx >= d->count) return NULL;
    return d->entries[idx].userdata;
}

void lapka_tree_set_expanded(LapkaPrimitive *p, int idx, bool expanded) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return;
    TreeData *d = p->data;
    if (idx < 0 || idx >= d->count) return;
    if (!d->entries[idx].has_children) return;
    d->entries[idx].expanded = expanded;
}

int lapka_tree_cursor(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return -1;
    TreeData *d = p->data;
    return d->cursor;
}

int lapka_tree_cursor_index(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TREE) return -1;
    TreeData *d = p->data;
    int n = flat_count(d);
    if (n == 0 || d->cursor >= n) return -1;
    return d->flat[d->cursor];
}

const char *lapka_tree_cursor_label(LapkaPrimitive *p) {
    int idx = lapka_tree_cursor_index(p);
    if (idx < 0) return NULL;
    TreeData *d = p->data;
    return d->entries[idx].label;
}

void *lapka_tree_cursor_userdata(LapkaPrimitive *p) {
    int idx = lapka_tree_cursor_index(p);
    if (idx < 0) return NULL;
    TreeData *d = p->data;
    return d->entries[idx].userdata;
}