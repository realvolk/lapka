/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/layer.h>

#include <stdlib.h>
#include <string.h>

static int layer_cmp(const void *a, const void *b) {
    const LapkaLayer *la = *(const LapkaLayer *const *)a;
    const LapkaLayer *lb = *(const LapkaLayer *const *)b;
    return la->z - lb->z;
}

static void blend_layer(LapkaFramebuffer *fb, LapkaLayer *l) {
    int w = l->rect.w;
    int h = l->rect.h;

    for (int y = 0; y < h; y++) {
        int fy = l->rect.y + y;
        if (fy < 0 || fy >= fb->h) continue;

        for (int x = 0; x < w; x++) {
            int fx = l->rect.x + x;
            if (fx < 0 || fx >= fb->w) continue;

            LapkaCell sc = l->buf[(size_t)y * (size_t)w + (size_t)x];

            if (l->blend == LAPKA_BLEND_TRANSPARENT && sc.cp == 0) continue;

            fb->cur[(size_t)fy * (size_t)fb->w + (size_t)fx] = sc;
        }
    }
}

void lapka_composite(LapkaFramebuffer *fb, LapkaLayer **layers, int n) {
    if (!fb || !layers || n <= 0) return;

    LapkaLayer **sorted = malloc((size_t)n * sizeof(LapkaLayer *));
    if (!sorted) return;

    memcpy(sorted, layers, (size_t)n * sizeof(LapkaLayer *));
    qsort(sorted, (size_t)n, sizeof(LapkaLayer *), layer_cmp);

    for (int i = 0; i < n; i++) {
        LapkaLayer *l = sorted[i];
        if (!l || !l->visible) continue;
        blend_layer(fb, l);
    }

    free(sorted);
}