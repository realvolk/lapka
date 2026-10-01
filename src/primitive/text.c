/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include <render/draw.h>
#include "core/utf8.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char *value;
    char *prefix;
    int   cursor_cp;
    bool  masked;
    void (*on_change)(void *);
    void *on_change_ud;
} TextData;

static size_t cp_offset(const char *s, int cp_index) {
    const uint8_t *p = (const uint8_t *)s;
    int i = 0;
    while (*p && i < cp_index) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        i++;
    }
    return (size_t)((const char *)p - s);
}

static int cp_count(const char *s) {
    const uint8_t *p = (const uint8_t *)s;
    int i = 0;
    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        i++;
    }
    return i;
}

static int cp_display_width(const char *s, int cp_index) {
    const uint8_t *p = (const uint8_t *)s;
    int i = 0, w = 0;
    while (*p && i < cp_index) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        w += lapka_cp_width(cp);
        p += bytes;
        i++;
    }
    return w;
}

static void text_apply_change(TextData *d) {
    if (d->on_change) d->on_change(d->on_change_ud);
}

static void cp_delete_left(char *s, int *cursor_cp) {
    if (*cursor_cp <= 0) return;
    size_t end = cp_offset(s, *cursor_cp);
    size_t start = cp_offset(s, *cursor_cp - 1);
    size_t old_len = strlen(s);
    memmove(s + start, s + end, old_len - end + 1);
    (*cursor_cp)--;
}

static void cp_delete_right(char *s, int *cursor_cp) {
    size_t old_len = strlen(s);
    size_t start = cp_offset(s, *cursor_cp);
    if (start >= old_len) return;
    size_t end = cp_offset(s, *cursor_cp + 1);
    memmove(s + start, s + end, old_len - end + 1);
}

static char *cp_insert(char *s, int *cursor_cp, uint32_t cp) {
    size_t off = cp_offset(s, *cursor_cp);
    uint8_t enc[4];
    int n = lapka_utf8_encode(cp, enc);
    size_t old_len = strlen(s);
    char *ns = realloc(s, old_len + (size_t)n + 1);
    if (!ns) return NULL;
    memmove(ns + off + (size_t)n, ns + off, old_len - off + 1);
    memcpy(ns + off, enc, (size_t)n);
    (*cursor_cp)++;
    return ns;
}

static void text_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                      const LapkaTheme *theme) {
    TextData *d = p->data;
    (void)theme;
    if (w <= 0 || h <= 0) return;

    int x = 0;
    if (d->prefix && d->prefix[0]) {
        const uint8_t *pre = (const uint8_t *)d->prefix;
        while (*pre && x < w) {
            uint32_t cp;
            int b = lapka_utf8_decode(pre, 4, &cp);
            int cw = lapka_cp_width(cp);
            pre += b;
            if (cw == 0) continue;
            for (int i = 0; i < cw && x + i < w; i++) {
                LapkaCell c = { i == 0 ? cp : 0, 0, 0, 0 };
                buf[x + i] = c;
            }
            x += cw;
        }
    }

    const uint8_t *s = (const uint8_t *)d->value;
    int cp_idx = 0;

    while (*s && x < w) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(s, 4, &cp);
        int cw = lapka_cp_width(cp);
        s += bytes;
        if (cw == 0) { cp_idx++; continue; }

        int is_cursor = (cp_idx == d->cursor_cp);
        uint32_t draw_cp = d->masked ? '*' : cp;
        if (d->masked && is_cursor) draw_cp = ' ';
        uint16_t attrs = is_cursor ? LAPKA_ATTR_REVERSE : 0;

        for (int i = 0; i < cw && x + i < w; i++) {
            LapkaCell c = { i == 0 ? draw_cp : 0, 0, 0, attrs };
            buf[x + i] = c;
        }
        x += cw;
        cp_idx++;
    }

    if (d->cursor_cp == cp_idx && x < w) {
        LapkaCell c = { ' ', 0, 0, LAPKA_ATTR_REVERSE };
        buf[x] = c;
    }
}

static void text_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                         int *out_w, int *out_h) {
    TextData *d = p->data;
    (void)bound_h;
    int pre_w = 0;
    if (d->prefix) pre_w = (int)strlen(d->prefix);
    int val_w = d->masked ? cp_count(d->value) : cp_display_width(d->value, cp_count(d->value));
    int total = pre_w + val_w;
    if (total > bound_w) total = bound_w;
    *out_w = total;
    *out_h = 1;
}

static bool text_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    TextData *d = p->data;
    if (ev->type != LAPKA_EVENT_KEY) return false;

    switch (ev->key.code) {
        case LAPKA_KEY_BACKSPACE:
            cp_delete_left(d->value, &d->cursor_cp);
            text_apply_change(d);
            return true;
        case LAPKA_KEY_DELETE:
            cp_delete_right(d->value, &d->cursor_cp);
            text_apply_change(d);
            return true;
        case LAPKA_KEY_LEFT:
            if (d->cursor_cp > 0) d->cursor_cp--;
            return true;
        case LAPKA_KEY_RIGHT: {
            size_t len = strlen(d->value);
            size_t off = cp_offset(d->value, d->cursor_cp);
            if (off < len) d->cursor_cp++;
            return true;
        }
        case LAPKA_KEY_HOME:
            d->cursor_cp = 0;
            return true;
        case LAPKA_KEY_END:
            d->cursor_cp = cp_count(d->value);
            return true;
        case LAPKA_KEY_CODEPOINT:
            if (ev->key.ch == '\n' || ev->key.ch == '\r') return false;
            if (ev->key.ch == 0x03) return false;
            if (ev->key.ch < 0x20) return false;
            {
                char *nv = cp_insert(d->value, &d->cursor_cp, ev->key.ch);
                if (nv) d->value = nv;
                text_apply_change(d);
            }
            return true;
        default:
            return false;
    }
}

static void text_free(LapkaPrimitive *p) {
    TextData *d = p->data;
    if (!d) { free(p); return; }
    free(d->value);
    free(d->prefix);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_text_new(const char *initial, bool masked) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    TextData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    d->value = strdup(initial ? initial : "");
    d->prefix = strdup("");
    d->masked = masked;
    d->cursor_cp = cp_count(d->value);

    p->kind = LAPKA_PRIMITIVE_TEXT;
    p->data = d;
    p->handle = text_handle;
    p->draw = text_draw;
    p->measure = text_measure;
    p->free = text_free;
    return p;
}

void lapka_text_set(LapkaPrimitive *p, const char *value) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TEXT) return;
    TextData *d = p->data;
    free(d->value);
    d->value = strdup(value ? value : "");
    d->cursor_cp = cp_count(d->value);
}

const char *lapka_text_value(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TEXT) return NULL;
    TextData *d = p->data;
    return d->value;
}

void lapka_text_on_change(LapkaPrimitive *p, void (*fn)(void *), void *ud) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TEXT) return;
    TextData *d = p->data;
    d->on_change = fn;
    d->on_change_ud = ud;
}

void lapka_text_set_prefix(LapkaPrimitive *p, const char *prefix) {
    if (!p || p->kind != LAPKA_PRIMITIVE_TEXT) return;
    TextData *d = p->data;
    free(d->prefix);
    d->prefix = strdup(prefix ? prefix : "");
}