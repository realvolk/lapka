/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include <render/draw.h>
#include "core/utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const LapkaListStyle LAPKA_LIST_STYLE_DEFAULT = {
    LAPKA_CURSOR_GLYPH,
    ">",
    0,
    0,
    LAPKA_ATTR_BOLD,
    0,
    0,
    0,
    LAPKA_SCROLL_BOTH,
    "\xe2\x96\xb2",
    "\xe2\x96\xbc",
};

typedef struct {
    char              **items;
    int                 count;
    int                *visible;
    int                 visible_count;
    int                 cursor;
    int                 scroll;
    int                 visible_rows;
    LapkaRect           last_rect;
    int                 rect_known;
    LapkaSelectMode     mode;
    unsigned char      *checked;
    int                *order;
    int                 order_count;
    LapkaListStyle      style;
    LapkaMatcher        matcher;
    LapkaPrimitive     *filter;
} ListData;

static bool default_matcher(const char *item, const char *query) {
    if (!query || query[0] == '\0') return true;
    size_t qn = strlen(query);
    size_t in = strlen(item);
    if (qn > in) return false;
    for (size_t i = 0; i + qn <= in; i++) {
        size_t j = 0;
        while (j < qn) {
            char a = item[i + j];
            char b = query[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
            j++;
        }
        if (j == qn) return true;
    }
    return false;
}

static void clamp_scroll(ListData *d) {
    int rows = d->visible_rows > 0 ? d->visible_rows : 1;
    if (d->cursor < d->scroll) d->scroll = d->cursor;
    if (d->cursor >= d->scroll + rows) d->scroll = d->cursor - rows + 1;
    if (d->scroll < 0) d->scroll = 0;
    if (d->visible_count == 0) { d->scroll = 0; return; }
    if (d->scroll > d->visible_count - 1) d->scroll = d->visible_count - 1;
}

static int put_str(LapkaCell *buf, int w, int h, int x, int y,
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
            lapka_put_cp(buf, w, h, cx + i, y, i == 0 ? cp : 0, fg, bg, attrs);
        }
        cx += cw;
        used += cw;
    }
    return used;
}

static int put_repeat(LapkaCell *buf, int w, int h, int x, int y, uint32_t cp,
                      int n, uint32_t fg, uint32_t bg, uint16_t attrs) {
    for (int i = 0; i < n; i++) lapka_put_cp(buf, w, h, x + i, y, cp, fg, bg, attrs);
    return n;
}

static void list_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                      const LapkaTheme *theme) {
    ListData *d = p->data;

    d->last_rect.x = 0;
    d->last_rect.y = 0;
    d->last_rect.w = w;
    d->last_rect.h = h;
    d->rect_known = 1;

    if (h <= 0 || w <= 0) return;

    uint32_t cursor_fg = d->style.cursor_fg ? d->style.cursor_fg : theme->accent;
    uint32_t item_fg = d->style.item_fg;

    int reserve_bottom = 0;
    if ((d->style.scroll == LAPKA_SCROLL_COUNTER ||
         d->style.scroll == LAPKA_SCROLL_BOTH) && d->visible_count > 0) {
        reserve_bottom = 1;
    }
    int content_h = h - reserve_bottom;
    if (content_h < 0) content_h = 0;

    d->visible_rows = content_h;
    clamp_scroll(d);

    if (d->visible_count == 0) {
        put_str(buf, w, h, 0, 0, "(no matches)", theme->muted, 0, 0, w);
        return;
    }

    for (int row = 0; row < content_h; row++) {
        int vis_idx = d->scroll + row;
        if (vis_idx >= d->visible_count) break;

        int idx = d->visible[vis_idx];
        int is_cursor = (vis_idx == d->cursor);
        uint32_t fg = is_cursor ? cursor_fg : item_fg;
        uint32_t bg = is_cursor ? d->style.cursor_bg : d->style.item_bg;
        uint16_t attrs = is_cursor ? d->style.cursor_attrs : d->style.item_attrs;

        int col = 0;
        if (is_cursor) {
            col += put_str(buf, w, h, col, row, d->style.cursor_glyph, fg, bg, attrs, -1);
            col += put_repeat(buf, w, h, col, row, ' ', 1, fg, bg, attrs);
        } else {
            col += put_repeat(buf, w, h, col, row, ' ', 2, fg, bg, attrs);
        }

        uint16_t item_attrs_local = attrs;
        uint32_t item_fg_local = fg;

        if (d->mode == LAPKA_SELECT_MANY) {
            const char *mark = d->checked[idx] ? "[x] " : "[ ] ";
            uint32_t mark_fg = d->checked[idx] ? theme->accent : item_fg_local;
            uint16_t mark_attrs = d->checked[idx]
                ? (uint16_t)(item_attrs_local | LAPKA_ATTR_BOLD)
                : item_attrs_local;
            col += put_str(buf, w, h, col, row, mark, mark_fg, bg, mark_attrs, -1);
            if (d->checked[idx]) {
                item_fg_local = theme->accent;
                item_attrs_local = (uint16_t)(item_attrs_local | LAPKA_ATTR_BOLD);
            }
        }

        put_str(buf, w, h, col, row, d->items[idx],
                item_fg_local, bg, item_attrs_local, w - col);
    }

    int rows_above = d->scroll;
    int rows_below = d->visible_count - (d->scroll + content_h);
    if (rows_below < 0) rows_below = 0;

    int draw_arrows = (d->style.scroll == LAPKA_SCROLL_ARROWS ||
                       d->style.scroll == LAPKA_SCROLL_BOTH);
    if (draw_arrows && content_h > 0) {
        if (rows_above > 0) {
            put_str(buf, w, h, w - 1, 0, d->style.scroll_up,
                    theme->muted, 0, 0, -1);
        }
        if (rows_below > 0) {
            put_str(buf, w, h, w - 1, content_h - 1, d->style.scroll_down,
                    theme->muted, 0, 0, -1);
        }
    }

    if (reserve_bottom) {
        char counter[32];
        int n = snprintf(counter, sizeof(counter), "(%d/%d)",
                         d->cursor + 1, d->visible_count);
        if (n > 0 && n < (int)sizeof(counter)) {
            int cx = w - n;
            if (cx < 0) cx = 0;
            put_str(buf, w, h, cx, h - 1, counter, theme->muted, 0, 0, w - cx);
        }
    }
}

static void list_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                         int *out_w, int *out_h) {
    ListData *d = p->data;
    int max_w = 0;
    for (int i = 0; i < d->visible_count; i++) {
        const uint8_t *q = (const uint8_t *)d->items[d->visible[i]];
        int col = 2;
        while (*q) {
            uint32_t cp;
            int bytes = lapka_utf8_decode(q, 4, &cp);
            if (bytes <= 0) break;
            q += bytes;
            col += lapka_cp_width(cp);
        }
        if (col > max_w) max_w = col;
    }
    if (max_w > bound_w) max_w = bound_w;
    int h = d->visible_count;
    if (h > bound_h) h = bound_h;
    *out_w = max_w;
    *out_h = h;
}

static void list_move_cursor(ListData *d, int delta) {
    if (d->visible_count == 0) return;
    int next = d->cursor + delta;
    if (next < 0) next = 0;
    if (next >= d->visible_count) next = d->visible_count - 1;
    d->cursor = next;
    clamp_scroll(d);
}

static void list_page(ListData *d, int dir) {
    if (d->visible_count == 0) return;
    int rows = d->visible_rows > 0 ? d->visible_rows : 1;
    int next = d->cursor + dir * rows;
    if (next < 0) next = 0;
    if (next >= d->visible_count) next = d->visible_count - 1;
    d->cursor = next;
    clamp_scroll(d);
}

static void list_home(ListData *d) {
    if (d->visible_count == 0) return;
    d->cursor = 0;
    clamp_scroll(d);
}

static void list_end(ListData *d) {
    if (d->visible_count == 0) return;
    d->cursor = d->visible_count - 1;
    clamp_scroll(d);
}

static void list_toggle(ListData *d) {
    if (d->mode != LAPKA_SELECT_MANY) return;
    if (d->visible_count == 0) return;
    if (d->cursor < 0 || d->cursor >= d->visible_count) return;
    int idx = d->visible[d->cursor];
    if (d->checked[idx]) {
        d->checked[idx] = 0;
        for (int i = 0; i < d->order_count; i++) {
            if (d->order[i] == idx) {
                for (int j = i; j + 1 < d->order_count; j++) {
                    d->order[j] = d->order[j + 1];
                }
                d->order_count--;
                break;
            }
        }
    } else {
        d->checked[idx] = 1;
        d->order[d->order_count++] = idx;
    }
}

static int list_handle_mouse(ListData *d, const LapkaEvent *ev) {
    if (!d->rect_known) return 0;

    int rel_y = ev->mouse.y - d->last_rect.y;
    int rel_x = ev->mouse.x - d->last_rect.x;

    if (ev->mouse.action == LAPKA_MOUSE_WHEEL_UP) {
        list_move_cursor(d, -1);
        return 1;
    }
    if (ev->mouse.action == LAPKA_MOUSE_WHEEL_DOWN) {
        list_move_cursor(d, 1);
        return 1;
    }
    if (ev->mouse.action != LAPKA_MOUSE_PRESS) return 0;
    if (ev->mouse.button != 0) return 0;
    if (rel_x < 0 || rel_x >= d->last_rect.w) return 0;
    if (rel_y < 0 || rel_y >= d->last_rect.h) return 0;

    int reserve_bottom = 0;
    if ((d->style.scroll == LAPKA_SCROLL_COUNTER ||
         d->style.scroll == LAPKA_SCROLL_BOTH) && d->visible_count > 0) {
        reserve_bottom = 1;
    }
    int content_h = d->last_rect.h - reserve_bottom;
    if (rel_y >= content_h) return 0;

    int vis_idx = d->scroll + rel_y;
    if (vis_idx < 0 || vis_idx >= d->visible_count) return 0;
    d->cursor = vis_idx;
    clamp_scroll(d);
    return 1;
}

static bool list_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    ListData *d = p->data;

    if (ev->type == LAPKA_EVENT_MOUSE) {
        return list_handle_mouse(d, ev) != 0;
    }

    if (ev->type != LAPKA_EVENT_KEY) return false;

    switch (ev->key.code) {
        case LAPKA_KEY_UP:       list_move_cursor(d, -1); return true;
        case LAPKA_KEY_DOWN:     list_move_cursor(d, 1); return true;
        case LAPKA_KEY_PAGEUP:   list_page(d, -1); return true;
        case LAPKA_KEY_PAGEDOWN: list_page(d, 1); return true;
        case LAPKA_KEY_HOME:     list_home(d); return true;
        case LAPKA_KEY_END:      list_end(d); return true;
        case LAPKA_KEY_TAB:
            if (d->mode == LAPKA_SELECT_MANY) {
                list_toggle(d);
                return true;
            }
            return false;
        case LAPKA_KEY_CODEPOINT:
            if (ev->key.ch == ' ') {
                if (d->mode == LAPKA_SELECT_MANY && !d->filter) {
                    list_toggle(d);
                    return true;
                }
                return false;
            }
            return false;
        default:
            return false;
    }
}

static void list_free_impl(LapkaPrimitive *p) {
    ListData *d = p->data;
    if (!d) { free(p); return; }
    for (int i = 0; i < d->count; i++) free(d->items[i]);
    free(d->items);
    free(d->checked);
    free(d->order);
    free(d->visible);
    free(d);
    free(p);
}

static void recompute_visible(ListData *d) {
    const char *query = NULL;
    if (d->filter && d->filter->kind == LAPKA_PRIMITIVE_TEXT) {
        query = lapka_text_value(d->filter);
    }

    free(d->visible);
    d->visible = d->count > 0 ? calloc((size_t)d->count, sizeof(int)) : NULL;

    if (!query || !query[0]) {
        for (int i = 0; i < d->count; i++) d->visible[i] = i;
        d->visible_count = d->count;
    } else {
        int n = 0;
        for (int i = 0; i < d->count; i++) {
            if (d->matcher(d->items[i], query)) {
                d->visible[n++] = i;
            }
        }
        d->visible_count = n;
    }

    if (d->cursor >= d->visible_count) {
        d->cursor = d->visible_count > 0 ? d->visible_count - 1 : 0;
    }
    if (d->cursor < 0) d->cursor = 0;
    d->scroll = 0;
}

static void set_items(ListData *d, const char *const *items, int n) {
    for (int i = 0; i < d->count; i++) free(d->items[i]);
    free(d->items);
    free(d->checked);
    free(d->order);
    free(d->visible);

    d->count = n;
    d->items = n > 0 ? calloc((size_t)n, sizeof(char *)) : NULL;
    for (int i = 0; i < n; i++) d->items[i] = strdup(items[i]);

    d->checked = n > 0 ? calloc((size_t)n, 1) : NULL;
    d->order = n > 0 ? calloc((size_t)n, sizeof(int)) : NULL;
    d->visible = n > 0 ? calloc((size_t)n, sizeof(int)) : NULL;
    for (int i = 0; i < n; i++) d->visible[i] = i;

    d->order_count = 0;
    d->visible_count = n;
    d->cursor = 0;
    d->scroll = 0;
}

LapkaPrimitive *lapka_list_new(const char *const *items, int n,
                               LapkaSelectMode mode) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    ListData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    d->mode = mode;
    d->style = LAPKA_LIST_STYLE_DEFAULT;
    d->matcher = default_matcher;
    set_items(d, items, n);

    p->kind = LAPKA_PRIMITIVE_LIST;
    p->data = d;
    p->handle = list_handle;
    p->draw = list_draw;
    p->measure = list_measure;
    p->free = list_free_impl;
    return p;
}

void lapka_list_free(LapkaPrimitive *p) {
    if (!p) return;
    if (p->free) p->free(p);
    else free(p);
}

void lapka_list_set_items(LapkaPrimitive *p, const char *const *items, int n) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return;
    set_items(p->data, items, n);
}

void lapka_list_set_style(LapkaPrimitive *p, const LapkaListStyle *style) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST || !style) return;
    ListData *d = p->data;
    d->style = *style;
}

void lapka_list_attach_filter(LapkaPrimitive *p, LapkaPrimitive *text) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return;
    ListData *d = p->data;
    d->filter = text;
}

void lapka_list_set_matcher(LapkaPrimitive *p, LapkaMatcher m) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return;
    ListData *d = p->data;
    d->matcher = m ? m : default_matcher;
}

void lapka_list_refilter(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return;
    recompute_visible(p->data);
}

const char *lapka_list_result(LapkaPrimitive *p, int index) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return NULL;
    ListData *d = p->data;
    if (d->mode == LAPKA_SELECT_ONE) {
        if (index != 0) return NULL;
        if (d->visible_count == 0) return NULL;
        if (d->cursor < 0 || d->cursor >= d->visible_count) return NULL;
        return d->items[d->visible[d->cursor]];
    }
    if (index < 0 || index >= d->order_count) return NULL;
    return d->items[d->order[index]];
}

int lapka_list_result_count(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return 0;
    ListData *d = p->data;
    if (d->mode == LAPKA_SELECT_ONE) return d->visible_count > 0 ? 1 : 0;
    return d->order_count;
}

int lapka_list_cursor(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return -1;
    ListData *d = p->data;
    return d->cursor;
}

const char *lapka_list_cursor_item(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_LIST) return NULL;
    ListData *d = p->data;
    if (d->visible_count == 0) return NULL;
    if (d->cursor < 0 || d->cursor >= d->visible_count) return NULL;
    return d->items[d->visible[d->cursor]];
}
