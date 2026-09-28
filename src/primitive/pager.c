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
    char          **lines;
    int             line_count;
    int             cap;
    int             scroll;
    int             last_w;
    int             last_h;
    char           *hint;

    bool            search_enabled;
    bool            searching;
    LapkaPrimitive *search_field;
    char           *committed_query;
    int            *matches;
    int             match_count;
    int             current_match;
} PagerData;

static void put_cp(LapkaCell *buf, int w, int h, int x, int y, uint32_t cp,
                   uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    LapkaCell c = { cp, fg, bg, attrs };
    buf[(size_t)y * (size_t)w + (size_t)x] = c;
}

static int draw_str(LapkaCell *buf, int w, int h, int x, int y,
                    const char *s, size_t n,
                    uint32_t fg, uint32_t bg, uint16_t attrs,
                    int max_cols) {
    if (!s || n == 0) return 0;
    const uint8_t *p = (const uint8_t *)s;
    const uint8_t *end = p + n;
    int cx = x;
    int used = 0;
    while (p < end) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, (size_t)(end - p), &cp);
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

static int line_rows(const char *s, int width) {
    if (width <= 0) return 1;
    if (!s || !s[0]) return 1;

    const uint8_t *p = (const uint8_t *)s;
    int col = 0;
    int rows = 1;

    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        int cw = lapka_cp_width(cp);
        if (cw == 0) continue;
        if (col + cw > width) {
            rows++;
            col = 0;
        }
        col += cw;
    }
    return rows;
}

static int total_rows(PagerData *d, int width) {
    int total = 0;
    for (int i = 0; i < d->line_count; i++) {
        total += line_rows(d->lines[i], width);
    }
    return total;
}

static const char *skip_visual_rows(const char *s, int width, int skip,
                                    int *out_col) {
    if (skip <= 0) { *out_col = 0; return s; }

    const uint8_t *p = (const uint8_t *)s;
    int col = 0;
    int skipped = 0;

    while (*p && skipped < skip) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        int cw = lapka_cp_width(cp);
        if (cw == 0) { p += bytes; continue; }

        if (col + cw > width) {
            skipped++;
            col = 0;
            if (skipped >= skip) break;
        }
        p += bytes;
        col += cw;
    }

    *out_col = col;
    return (const char *)p;
}

static int row_byte_length(const char *s, int width, int start_col) {
    const uint8_t *p = (const uint8_t *)s;
    const uint8_t *start = p;
    int col = start_col;

    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        int cw = lapka_cp_width(cp);
        if (cw == 0) { p += bytes; continue; }
        if (col + cw > width) break;
        p += bytes;
        col += cw;
    }

    return (int)(p - start);
}

static int wrapped_row_of_line(PagerData *d, int line_idx, int width) {
    int row = 0;
    int stop = line_idx < d->line_count ? line_idx : d->line_count;
    for (int i = 0; i < stop; i++) {
        row += line_rows(d->lines[i], width);
    }
    return row;
}

static void clamp_scroll(PagerData *d) {
    int total = total_rows(d, d->last_w);
    int body_h = d->last_h;
    if (body_h < 1) body_h = 1;
    int max_scroll = total - body_h;
    if (max_scroll < 0) max_scroll = 0;
    if (d->scroll < 0) d->scroll = 0;
    if (d->scroll > max_scroll) d->scroll = max_scroll;
}

static void scroll_by(PagerData *d, int delta) {
    d->scroll += delta;
    clamp_scroll(d);
}

static void find_matches(PagerData *d, const char *query) {
    free(d->matches);
    d->matches = NULL;
    d->match_count = 0;

    free(d->committed_query);
    d->committed_query = NULL;

    if (!query || !query[0]) return;

    d->committed_query = strdup(query);

    int cap = 16;
    d->matches = malloc((size_t)cap * sizeof(int));
    if (!d->matches) return;

    for (int i = 0; i < d->line_count; i++) {
        if (strstr(d->lines[i], query)) {
            if (d->match_count >= cap) {
                cap *= 2;
                int *nm = realloc(d->matches, (size_t)cap * sizeof(int));
                if (!nm) break;
                d->matches = nm;
            }
            d->matches[d->match_count++] = i;
        }
    }
}

static void jump_to_current_match(PagerData *d) {
    if (d->match_count == 0) return;
    if (d->current_match < 0) d->current_match = d->match_count - 1;
    if (d->current_match >= d->match_count) d->current_match = 0;

    int line = d->matches[d->current_match];
    int row = wrapped_row_of_line(d, line, d->last_w);
    d->scroll = row;
    clamp_scroll(d);
}

static void next_match(PagerData *d, int dir) {
    if (d->match_count == 0) return;
    d->current_match += dir;
    if (d->current_match < 0) d->current_match = d->match_count - 1;
    if (d->current_match >= d->match_count) d->current_match = 0;
    jump_to_current_match(d);
}

static void draw_highlighted(LapkaCell *buf, int w, int h, int y,
                             const char *slice, int nbytes,
                             const char *query, const LapkaTheme *theme) {
    if (!query || !query[0] || nbytes <= 0) {
        draw_str(buf, w, h, 0, y, slice, (size_t)nbytes, 0, 0, 0, w);
        return;
    }

    size_t qn = strlen(query);
    const char *p = slice;
    const char *end = slice + nbytes;
    int cx = 0;

    while (p < end && cx < w) {
        const char *hit = NULL;
        for (const char *q = p; q + qn <= end; q++) {
            if (memcmp(q, query, qn) == 0) { hit = q; break; }
        }

        if (!hit) {
            int used = draw_str(buf, w, h, cx, y, p, (size_t)(end - p),
                                0, 0, 0, w - cx);
            cx += used;
            break;
        }

        if (hit > p) {
            int used = draw_str(buf, w, h, cx, y, p, (size_t)(hit - p),
                                0, 0, 0, w - cx);
            cx += used;
            if (cx >= w) break;
        }

        int used = draw_str(buf, w, h, cx, y, hit, qn,
                            0, theme->accent, LAPKA_ATTR_BOLD, w - cx);
        cx += used;
        if (cx >= w) break;

        p = hit + qn;
    }
}

static void pager_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                       const LapkaTheme *theme) {
    PagerData *d = p->data;
    if (w <= 0 || h <= 0) return;

    int content_h = h - 1;
    if (content_h < 0) content_h = 0;

    d->last_w = w;
    d->last_h = content_h;
    clamp_scroll(d);

    int rows_before = 0;
    int drawn = 0;
    int i = 0;

    while (i < d->line_count && drawn < content_h) {
        const char *line = d->lines[i];
        int wc = line_rows(line, w);

        if (rows_before + wc <= d->scroll) {
            rows_before += wc;
            i++;
            continue;
        }

        int skip_in_line = 0;
        if (rows_before < d->scroll) skip_in_line = d->scroll - rows_before;

        const char *slice = line;
        int start_col = 0;
        if (skip_in_line > 0) {
            slice = skip_visual_rows(slice, w, skip_in_line, &start_col);
        }

        int rows_this_line = wc - skip_in_line;
        for (int r = 0; r < rows_this_line && drawn < content_h; r++) {
            int nbytes = row_byte_length(slice, w, start_col);
            draw_highlighted(buf, w, h, drawn, slice, nbytes,
                             d->committed_query, theme);
            drawn++;
            if (nbytes == 0) break;
            slice += nbytes;
            start_col = 0;
        }

        rows_before += wc;
        i++;
    }

    if (d->searching) {
        LapkaCell *sub = buf + (size_t)content_h * (size_t)w;
        d->search_field->draw(d->search_field, sub, w, 1, theme);
    } else {
        const char *hint = d->hint ? d->hint : "";
        draw_str(buf, w, h, 0, content_h, hint, strlen(hint),
                 theme->muted, 0, 0, w);
    }
}

static void pager_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                          int *out_w, int *out_h) {
    PagerData *d = p->data;
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
    *out_w = maxw;

    int h = total_rows(d, bound_w) + 1;
    if (h > bound_h) h = bound_h;
    *out_h = h;
}

static bool pager_handle_searching(PagerData *d, const LapkaEvent *ev) {
    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER) {
            const char *q = lapka_text_value(d->search_field);
            find_matches(d, q);
            d->current_match = 0;
            jump_to_current_match(d);
            d->searching = false;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_ESC) {
            d->searching = false;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_CODEPOINT &&
            ev->key.ch == 0x03 && (ev->key.mods & LAPKA_MOD_CTRL)) {
            d->searching = false;
            return true;
        }
    }
    d->search_field->handle(d->search_field, ev);
    return true;
}

static bool pager_handle_nav(PagerData *d, const LapkaEvent *ev) {
    if (ev->type == LAPKA_EVENT_MOUSE) {
        if (ev->mouse.action == LAPKA_MOUSE_WHEEL_UP) {
            scroll_by(d, -3);
            return true;
        }
        if (ev->mouse.action == LAPKA_MOUSE_WHEEL_DOWN) {
            scroll_by(d, 3);
            return true;
        }
        return false;
    }

    if (ev->type != LAPKA_EVENT_KEY) return false;

    int page = d->last_h;
    if (page < 1) page = 1;

    switch (ev->key.code) {
        case LAPKA_KEY_UP:       scroll_by(d, -1); return true;
        case LAPKA_KEY_DOWN:     scroll_by(d, 1); return true;
        case LAPKA_KEY_PAGEUP:   scroll_by(d, -page); return true;
        case LAPKA_KEY_PAGEDOWN: scroll_by(d, page); return true;
        case LAPKA_KEY_HOME:     d->scroll = 0; return true;
        case LAPKA_KEY_END:
            d->scroll = total_rows(d, d->last_w);
            clamp_scroll(d);
            return true;
        case LAPKA_KEY_CODEPOINT:
            if (ev->key.ch == 'j') { scroll_by(d, 1); return true; }
            if (ev->key.ch == 'k') { scroll_by(d, -1); return true; }
            if (ev->key.ch == 'g') { d->scroll = 0; return true; }
            if (ev->key.ch == 'G') {
                d->scroll = total_rows(d, d->last_w);
                clamp_scroll(d);
                return true;
            }
            if (ev->key.ch == 'b') { scroll_by(d, -page); return true; }
            if (ev->key.ch == ' ') { scroll_by(d, page); return true; }
            if (ev->key.ch == 'n') { next_match(d, 1); return true; }
            if (ev->key.ch == 'N') { next_match(d, -1); return true; }
            if (ev->key.ch == '/' && d->search_enabled) {
                lapka_text_set(d->search_field, "");
                lapka_text_set_prefix(d->search_field, "/ ");
                d->searching = true;
                return true;
            }
            return false;
        default:
            return false;
    }
}

static bool pager_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    PagerData *d = p->data;
    if (d->searching) return pager_handle_searching(d, ev);
    return pager_handle_nav(d, ev);
}

static void pager_free(LapkaPrimitive *p) {
    PagerData *d = p->data;
    if (!d) { free(p); return; }
    for (int i = 0; i < d->line_count; i++) free(d->lines[i]);
    free(d->lines);
    free(d->hint);
    free(d->committed_query);
    free(d->matches);
    if (d->search_field) lapka_primitive_free(d->search_field);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_pager_new(void) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    PagerData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    d->search_field = lapka_text_new("", false);
    if (!d->search_field) { free(d); free(p); return NULL; }
    d->search_enabled = true;
    d->current_match = 0;

    p->kind = LAPKA_PRIMITIVE_PAGER;
    p->data = d;
    p->handle = pager_handle;
    p->draw = pager_draw;
    p->measure = pager_measure;
    p->free = pager_free;
    return p;
}

void lapka_pager_set_lines(LapkaPrimitive *p, const char *const *lines, int count) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return;
    PagerData *d = p->data;

    for (int i = 0; i < d->line_count; i++) free(d->lines[i]);
    free(d->lines);
    free(d->matches);
    free(d->committed_query);
    d->matches = NULL;
    d->match_count = 0;
    d->committed_query = NULL;
    d->current_match = 0;
    d->scroll = 0;

    d->cap = count > 0 ? count : 1;
    d->lines = calloc((size_t)d->cap, sizeof(char *));
    if (!d->lines) { d->line_count = 0; d->cap = 0; return; }
    for (int i = 0; i < count; i++) {
        d->lines[i] = strdup(lines[i] ? lines[i] : "");
    }
    d->line_count = count;
}

int lapka_pager_set_file(LapkaPrimitive *p, const char *path) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int cap = 64, n = 0;
    char **lines = malloc((size_t)cap * sizeof(char *));
    if (!lines) { fclose(f); return -1; }

    char *line = NULL;
    size_t lcap = 0;
    ssize_t got;

    while ((got = getline(&line, &lcap, f)) >= 0) {
        while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r')) {
            line[--got] = '\0';
        }
        if (n >= cap) {
            cap *= 2;
            char **nl = realloc(lines, (size_t)cap * sizeof(char *));
            if (!nl) break;
            lines = nl;
        }
        lines[n++] = strdup(line);
    }
    free(line);
    fclose(f);

    lapka_pager_set_lines(p, (const char *const *)lines, n);

    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);

    return 0;
}

void lapka_pager_set_hint(LapkaPrimitive *p, const char *hint) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return;
    PagerData *d = p->data;
    free(d->hint);
    d->hint = hint ? strdup(hint) : NULL;
}

void lapka_pager_set_search_enabled(LapkaPrimitive *p, bool enabled) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return;
    PagerData *d = p->data;
    d->search_enabled = enabled;
}

int lapka_pager_line_count(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return 0;
    PagerData *d = p->data;
    return d->line_count;
}

const char *lapka_pager_line(LapkaPrimitive *p, int idx) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return NULL;
    PagerData *d = p->data;
    if (idx < 0 || idx >= d->line_count) return NULL;
    return d->lines[idx];
}

int lapka_pager_scroll(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return 0;
    PagerData *d = p->data;
    return d->scroll;
}

void lapka_pager_reset_scroll(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return;
    PagerData *d = p->data;
    d->scroll = 0;
}

bool lapka_pager_is_searching(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return false;
    PagerData *d = p->data;
    return d->searching;
}

int lapka_pager_match_count(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_PAGER) return 0;
    PagerData *d = p->data;
    return d->match_count;
}