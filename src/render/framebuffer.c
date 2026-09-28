/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "framebuffer.h"
#include "../core/cell.h"

#include <stdlib.h>
#include <string.h>

static size_t cell_count(int w, int h) {
    return (size_t)w * (size_t)h;
}

int lapka_fb_init(LapkaFramebuffer *fb, int w, int h) {
    if (w <= 0 || h <= 0) return -1;

    fb->w = w;
    fb->h = h;
    fb->full_dirty = true;

    size_t n = cell_count(w, h);
    fb->cur = calloc(n, sizeof(LapkaCell));
    fb->prev = calloc(n, sizeof(LapkaCell));
    if (!fb->cur || !fb->prev) {
        free(fb->cur);
        free(fb->prev);
        fb->cur = NULL;
        fb->prev = NULL;
        return -1;
    }
    return 0;
}

void lapka_fb_free(LapkaFramebuffer *fb) {
    free(fb->cur);
    free(fb->prev);
    fb->cur = NULL;
    fb->prev = NULL;
    fb->w = 0;
    fb->h = 0;
    fb->full_dirty = true;
}

int lapka_fb_resize(LapkaFramebuffer *fb, int w, int h) {
    if (w <= 0 || h <= 0) return -1;
    if (w == fb->w && h == fb->h) return 0;

    size_t n = cell_count(w, h);
    LapkaCell *cur = calloc(n, sizeof(LapkaCell));
    LapkaCell *prev = calloc(n, sizeof(LapkaCell));
    if (!cur || !prev) {
        free(cur);
        free(prev);
        return -1;
    }
    free(fb->cur);
    free(fb->prev);
    fb->cur = cur;
    fb->prev = prev;
    fb->w = w;
    fb->h = h;
    fb->full_dirty = true;
    return 0;
}

void lapka_fb_clear(LapkaFramebuffer *fb) {
    if (!fb->cur) return;
    size_t n = cell_count(fb->w, fb->h);
    memset(fb->cur, 0, n * sizeof(LapkaCell));
}

void lapka_fb_mark_full_dirty(LapkaFramebuffer *fb) {
    fb->full_dirty = true;
}

int lapka_fb_put(LapkaFramebuffer *fb, int x, int y, LapkaCell c) {
    if (x < 0 || y < 0 || x >= fb->w || y >= fb->h) return -1;
    fb->cur[(size_t)y * (size_t)fb->w + (size_t)x] = c;
    return 0;
}

LapkaCell lapka_fb_get(const LapkaFramebuffer *fb, int x, int y) {
    if (x < 0 || y < 0 || x >= fb->w || y >= fb->h) {
        return lapka_cell_blank();
    }
    return fb->cur[(size_t)y * (size_t)fb->w + (size_t)x];
}