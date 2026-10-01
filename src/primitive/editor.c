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

#define UNDO_MAX 200

typedef struct {
    char **lines;
    int    count;
    int    row;
    int    col;
} Snapshot;

typedef struct {
    char  **lines;
    int     line_count;
    int     line_cap;

    int     row;
    int     col;
    int     scroll;
    int     visible_h;

    char   *path;
    char   *hint;
    bool    dirty;

    Snapshot undo[UNDO_MAX];
    int      undo_count;
    int      undo_pos;

    bool    pending_undo_capture;
    bool    cursor_visible;
} EditorData;

static int cp_count(const char *s) {
    if (!s) return 0;
    const uint8_t *p = (const uint8_t *)s;
    int n = 0;
    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        n++;
    }
    return n;
}

static int cp_to_byte(const char *s, int cp_idx) {
    if (!s || cp_idx <= 0) return 0;
    const uint8_t *p = (const uint8_t *)s;
    int i = 0;
    while (*p && i < cp_idx) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        i++;
    }
    return (int)((const char *)p - s);
}

static int cp_cols(const char *s, int cp_idx) {
    if (!s || cp_idx <= 0) return 0;
    const uint8_t *p = (const uint8_t *)s;
    int i = 0, w = 0;
    while (*p && i < cp_idx) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        w += lapka_cp_width(cp);
        i++;
    }
    return w;
}

static void ensure_lines(EditorData *d) {
    if (d->line_count == 0) {
        if (d->line_cap < 1) {
            d->lines = malloc(sizeof(char *));
            d->line_cap = 1;
        }
        d->lines[0] = strdup("");
        d->line_count = 1;
    }
}

static void clear_lines(EditorData *d) {
    for (int i = 0; i < d->line_count; i++) free(d->lines[i]);
    free(d->lines);
    d->lines = NULL;
    d->line_count = 0;
    d->line_cap = 0;
}

static void ensure_line_cap(EditorData *d, int need) {
    if (need <= d->line_cap) return;
    int ncap = d->line_cap ? d->line_cap * 2 : 16;
    while (ncap < need) ncap *= 2;
    char **nl = realloc(d->lines, (size_t)ncap * sizeof(char *));
    if (!nl) return;
    d->lines = nl;
    d->line_cap = ncap;
}

static void snapshot_free(Snapshot *s) {
    for (int i = 0; i < s->count; i++) free(s->lines[i]);
    free(s->lines);
    s->lines = NULL;
    s->count = 0;
}

static Snapshot snapshot_make(EditorData *d) {
    Snapshot s;
    s.lines = calloc((size_t)d->line_count, sizeof(char *));
    for (int i = 0; i < d->line_count; i++) {
        s.lines[i] = strdup(d->lines[i]);
    }
    s.count = d->line_count;
    s.row = d->row;
    s.col = d->col;
    return s;
}

static void push_undo(EditorData *d) {
    while (d->undo_count > d->undo_pos) {
        snapshot_free(&d->undo[--d->undo_count]);
    }

    if (d->undo_count == UNDO_MAX) {
        snapshot_free(&d->undo[0]);
        memmove(&d->undo[0], &d->undo[1],
                (size_t)(UNDO_MAX - 1) * sizeof(Snapshot));
        d->undo_count--;
    }

    d->undo[d->undo_count++] = snapshot_make(d);
    d->undo_pos = d->undo_count;
}

static void editor_restore(EditorData *d, Snapshot *s) {
    clear_lines(d);
    d->lines = calloc((size_t)(s->count > 0 ? s->count : 1), sizeof(char *));
    d->line_cap = s->count > 0 ? s->count : 1;
    for (int i = 0; i < s->count; i++) {
        d->lines[i] = strdup(s->lines[i]);
    }
    d->line_count = s->count;
    ensure_lines(d);
    d->row = s->row;
    d->col = s->col;
    if (d->row >= d->line_count) d->row = d->line_count - 1;
    int max_col = cp_count(d->lines[d->row]);
    if (d->col > max_col) d->col = max_col;
}

static void editor_undo(EditorData *d) {
    if (d->undo_pos <= 0) return;
    d->undo_pos--;
    editor_restore(d, &d->undo[d->undo_pos]);
    d->dirty = true;
}

static void editor_redo(EditorData *d) {
    if (d->undo_pos >= d->undo_count) return;
    editor_restore(d, &d->undo[d->undo_pos]);
    d->undo_pos++;
    d->dirty = true;
}

static void clamp_cursor(EditorData *d, int width) {
    ensure_lines(d);
    if (d->row < 0) d->row = 0;
    if (d->row >= d->line_count) d->row = d->line_count - 1;
    int max_col = cp_count(d->lines[d->row]);
    if (d->col < 0) d->col = 0;
    if (d->col > max_col) d->col = max_col;

    if (d->row < d->scroll) d->scroll = d->row;
    if (d->row >= d->scroll + d->visible_h) d->scroll = d->row - d->visible_h + 1;
    if (d->scroll < 0) d->scroll = 0;

    (void)width;
}

void lapka_editor_set_cursor_visible(LapkaPrimitive *p, bool visible) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return;
    EditorData *d = p->data;
    d->cursor_visible = visible;
}

static void editor_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                        const LapkaTheme *theme) {
    EditorData *d = p->data;
    if (w <= 0 || h <= 0) return;

    d->visible_h = h - 1;
    if (d->visible_h < 1) d->visible_h = 1;
    clamp_cursor(d, w);

    int content_h = d->visible_h;
    for (int i = 0; i < content_h; i++) {
        int idx = d->scroll + i;
        if (idx >= d->line_count) break;
        lapka_draw_str(buf, w, h, 0, i, d->lines[idx], 0, 0, 0, w);
    }

    if (d->cursor_visible) {
        int scr_row = d->row - d->scroll;
        if (scr_row >= 0 && scr_row < content_h) {
            int scr_col = cp_cols(d->lines[d->row], d->col);
            if (scr_col < w) {
                LapkaCell c = buf[(size_t)scr_row * (size_t)w + (size_t)scr_col];
                c.attrs |= LAPKA_ATTR_REVERSE;
                if (c.cp == 0) c.cp = ' ';
                buf[(size_t)scr_row * (size_t)w + (size_t)scr_col] = c;
            }
        }
    }

    const char *hint = d->hint ? d->hint : "";
    lapka_draw_str(buf, w, h, 0, h - 1, hint, theme->muted, 0, 0, w);
}

static void editor_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                           int *out_w, int *out_h) {
    EditorData *d = p->data;
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
    int h = d->line_count;
    if (h > bound_h) h = bound_h;
    *out_h = h;
}

static void editor_insert_cp(EditorData *d, uint32_t cp) {
    char *line = d->lines[d->row];
    int byte_off = cp_to_byte(line, d->col);
    size_t len = strlen(line);

    uint8_t enc[4];
    int n = lapka_utf8_encode(cp, enc);

    char *nl = realloc(line, len + (size_t)n + 1);
    if (!nl) return;
    memmove(nl + byte_off + n, nl + byte_off, len - (size_t)byte_off + 1);
    memcpy(nl + byte_off, enc, (size_t)n);
    d->lines[d->row] = nl;
    d->col++;
}

static void editor_newline(EditorData *d) {
    char *line = d->lines[d->row];
    int byte_off = cp_to_byte(line, d->col);
    char *rest = strdup(line + byte_off);
    line[byte_off] = '\0';

    ensure_line_cap(d, d->line_count + 1);
    memmove(&d->lines[d->row + 2], &d->lines[d->row + 1],
            (size_t)(d->line_count - d->row - 1) * sizeof(char *));
    d->lines[d->row + 1] = rest;
    d->line_count++;
    d->row++;
    d->col = 0;
}

static void editor_backspace(EditorData *d) {
    if (d->col > 0) {
        char *line = d->lines[d->row];
        int byte_end = cp_to_byte(line, d->col);
        int byte_start = cp_to_byte(line, d->col - 1);
        memmove(line + byte_start, line + byte_end,
                strlen(line + byte_end) + 1);
        d->col--;
    } else if (d->row > 0) {
        char *prev = d->lines[d->row - 1];
        char *cur = d->lines[d->row];
        d->col = cp_count(prev);
        size_t new_len = strlen(prev) + strlen(cur);
        char *joined = realloc(prev, new_len + 1);
        if (!joined) return;
        strcat(joined, cur);
        d->lines[d->row - 1] = joined;
        free(cur);
        memmove(&d->lines[d->row], &d->lines[d->row + 1],
                (size_t)(d->line_count - d->row - 1) * sizeof(char *));
        d->line_count--;
        d->row--;
    }
}

static void editor_delete(EditorData *d) {
    char *line = d->lines[d->row];
    int max = cp_count(line);
    if (d->col < max) {
        int byte_start = cp_to_byte(line, d->col);
        int byte_end = cp_to_byte(line, d->col + 1);
        memmove(line + byte_start, line + byte_end,
                strlen(line + byte_end) + 1);
    } else if (d->row + 1 < d->line_count) {
        char *next = d->lines[d->row + 1];
        size_t new_len = strlen(line) + strlen(next);
        char *joined = realloc(line, new_len + 1);
        if (!joined) return;
        strcat(joined, next);
        d->lines[d->row] = joined;
        free(next);
        memmove(&d->lines[d->row + 1], &d->lines[d->row + 2],
                (size_t)(d->line_count - d->row - 2) * sizeof(char *));
        d->line_count--;
    }
}

static void editor_delete_line(EditorData *d) {
    if (d->line_count <= 1) {
        free(d->lines[0]);
        d->lines[0] = strdup("");
        d->row = 0;
        d->col = 0;
        return;
    }
    free(d->lines[d->row]);
    memmove(&d->lines[d->row], &d->lines[d->row + 1],
            (size_t)(d->line_count - d->row - 1) * sizeof(char *));
    d->line_count--;
    if (d->row >= d->line_count) d->row = d->line_count - 1;
    d->col = 0;
}

static bool editor_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    EditorData *d = p->data;
    if (ev->type != LAPKA_EVENT_KEY) return false;

    bool mutated = false;

    switch (ev->key.code) {
        case LAPKA_KEY_UP:
            if (d->row > 0) { d->row--; clamp_cursor(d, 0); }
            return true;
        case LAPKA_KEY_DOWN:
            if (d->row + 1 < d->line_count) { d->row++; clamp_cursor(d, 0); }
            return true;
        case LAPKA_KEY_LEFT:
            if (d->col > 0) d->col--;
            else if (d->row > 0) {
                d->row--;
                d->col = cp_count(d->lines[d->row]);
            }
            clamp_cursor(d, 0);
            return true;
        case LAPKA_KEY_RIGHT:
            if (d->col < cp_count(d->lines[d->row])) d->col++;
            else if (d->row + 1 < d->line_count) {
                d->row++;
                d->col = 0;
            }
            clamp_cursor(d, 0);
            return true;
        case LAPKA_KEY_HOME: d->col = 0; return true;
        case LAPKA_KEY_END:  d->col = cp_count(d->lines[d->row]); return true;
        case LAPKA_KEY_PAGEUP:
            d->row -= d->visible_h;
            if (d->row < 0) d->row = 0;
            clamp_cursor(d, 0);
            return true;
        case LAPKA_KEY_PAGEDOWN:
            d->row += d->visible_h;
            if (d->row >= d->line_count) d->row = d->line_count - 1;
            clamp_cursor(d, 0);
            return true;
        case LAPKA_KEY_ENTER:
            push_undo(d);
            editor_newline(d);
            mutated = true;
            break;
        case LAPKA_KEY_BACKSPACE:
            push_undo(d);
            editor_backspace(d);
            mutated = true;
            break;
        case LAPKA_KEY_DELETE:
            push_undo(d);
            editor_delete(d);
            mutated = true;
            break;
        case LAPKA_KEY_CODEPOINT:
            if (ev->key.ch == 0x13) {
                return false;
            }
            if (ev->key.ch == 0x1A) {
                editor_undo(d);
                return true;
            }
            if (ev->key.ch == 0x19) {
                editor_redo(d);
                return true;
            }
            if (ev->key.ch == 0x04) {
                push_undo(d);
                editor_delete_line(d);
                mutated = true;
                break;
            }
            if (ev->key.ch >= 0x20 || ev->key.ch == '\t') {
                push_undo(d);
                editor_insert_cp(d, ev->key.ch);
                mutated = true;
                break;
            }
            return false;
        default:
            return false;
    }

    if (mutated) {
        d->dirty = true;
        clamp_cursor(d, 0);
    }
    return true;
}

static void editor_free(LapkaPrimitive *p) {
    EditorData *d = p->data;
    if (!d) { free(p); return; }
    clear_lines(d);
    free(d->path);
    free(d->hint);
    for (int i = 0; i < d->undo_count; i++) snapshot_free(&d->undo[i]);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_editor_new(void) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    EditorData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    ensure_lines(d);
    d->undo_pos = 0;

    p->kind = LAPKA_PRIMITIVE_EDITOR;
    p->data = d;
    p->handle = editor_handle;
    p->draw = editor_draw;
    p->measure = editor_measure;
    p->free = editor_free;
    return p;
}

static void load_from_file(EditorData *d, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char *line = NULL;
    size_t cap = 0;
    ssize_t got;

    clear_lines(d);

    while ((got = getline(&line, &cap, f)) >= 0) {
        while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r')) {
            line[--got] = '\0';
        }
        ensure_line_cap(d, d->line_count + 1);
        d->lines[d->line_count++] = strdup(line);
    }
    free(line);
    fclose(f);

    ensure_lines(d);
    d->row = 0;
    d->col = 0;
    d->scroll = 0;
    d->dirty = false;
}

int lapka_editor_open(LapkaPrimitive *p, const char *path) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return -1;
    EditorData *d = p->data;

    free(d->path);
    d->path = path ? strdup(path) : NULL;

    for (int i = 0; i < d->undo_count; i++) snapshot_free(&d->undo[i]);
    d->undo_count = 0;
    d->undo_pos = 0;

    if (path) {
        load_from_file(d, path);
    } else {
        clear_lines(d);
        ensure_lines(d);
        d->dirty = false;
    }
    return 0;
}

void lapka_editor_set_text(LapkaPrimitive *p, const char *text) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return;
    EditorData *d = p->data;

    clear_lines(d);
    if (!text) { ensure_lines(d); return; }

    const char *s = text;
    while (*s) {
        const char *nl = strchr(s, '\n');
        size_t len = nl ? (size_t)(nl - s) : strlen(s);
        ensure_line_cap(d, d->line_count + 1);
        char *line = malloc(len + 1);
        if (!line) break;
        memcpy(line, s, len);
        line[len] = '\0';
        d->lines[d->line_count++] = line;
        if (!nl) break;
        s = nl + 1;
    }
    ensure_lines(d);
    d->row = 0;
    d->col = 0;
    d->scroll = 0;
    d->dirty = false;
}

char *lapka_editor_text(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return NULL;
    EditorData *d = p->data;

    size_t total = 0;
    for (int i = 0; i < d->line_count; i++) {
        total += strlen(d->lines[i]) + 1;
    }

    char *out = malloc(total + 1);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < d->line_count; i++) {
        strcat(out, d->lines[i]);
        strcat(out, "\n");
    }

    return out;
}

void lapka_editor_set_path(LapkaPrimitive *p, const char *path) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return;
    EditorData *d = p->data;
    free(d->path);
    d->path = path ? strdup(path) : NULL;
}

const char *lapka_editor_path(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return NULL;
    EditorData *d = p->data;
    return d->path;
}

bool lapka_editor_is_dirty(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return false;
    EditorData *d = p->data;
    return d->dirty;
}

int lapka_editor_save(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return -1;
    EditorData *d = p->data;
    if (!d->path) return -1;

    FILE *f = fopen(d->path, "w");
    if (!f) return -1;
    for (int i = 0; i < d->line_count; i++) {
        fprintf(f, "%s\n", d->lines[i]);
    }
    fclose(f);
    d->dirty = false;
    return 0;
}

void lapka_editor_set_hint(LapkaPrimitive *p, const char *hint) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return;
    EditorData *d = p->data;
    free(d->hint);
    d->hint = hint ? strdup(hint) : NULL;
}

int lapka_editor_cursor_row(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return 0;
    EditorData *d = p->data;
    return d->row;
}

int lapka_editor_cursor_col(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_EDITOR) return 0;
    EditorData *d = p->data;
    return d->col;
}
