/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/layer.h>

#include <stdlib.h>

LapkaLayer *lapka_layer_new(LapkaRect rect, int z,
                            LapkaBlend blend, LapkaInput input) {
    if (rect.w <= 0 || rect.h <= 0) return NULL;

    LapkaLayer *l = calloc(1, sizeof(*l));
    if (!l) return NULL;

    l->rect = rect;
    l->z = z;
    l->blend = blend;
    l->input = input;
    l->visible = true;

    size_t n = (size_t)rect.w * (size_t)rect.h;
    l->buf = calloc(n, sizeof(LapkaCell));
    if (!l->buf) { free(l); return NULL; }

    return l;
}

int lapka_layer_resize(LapkaLayer *l, LapkaRect rect) {
    if (!l || rect.w <= 0 || rect.h <= 0) return -1;

    size_t n = (size_t)rect.w * (size_t)rect.h;
    LapkaCell *nb = calloc(n, sizeof(LapkaCell));
    if (!nb) return -1;

    free(l->buf);
    l->buf = nb;
    l->rect = rect;
    return 0;
}

void lapka_layer_free(LapkaLayer *l) {
    if (!l) return;
    if (l->free) l->free(l);
    free(l->buf);
    free(l);
}