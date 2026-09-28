/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/theme.h>
#include <lapka/types.h>
#include <stdbool.h>

typedef enum {
    LAPKA_BLEND_OPAQUE,
    LAPKA_BLEND_TRANSPARENT
} LapkaBlend;

typedef enum {
    LAPKA_INPUT_NONE,
    LAPKA_INPUT_PASS,
    LAPKA_INPUT_TRAP
} LapkaInput;

typedef struct LapkaLayer LapkaLayer;

struct LapkaLayer {
    LapkaRect   rect;
    int         z;
    LapkaBlend  blend;
    LapkaInput  input;
    bool        visible;
    LapkaCell  *buf;

    void       (*draw)(LapkaLayer *l, const LapkaTheme *theme);
    bool       (*handle)(LapkaLayer *l, const LapkaEvent *ev);
    void       (*free)(LapkaLayer *l);
    void       *userdata;
};

LapkaLayer *lapka_layer_new(LapkaRect rect, int z,
                            LapkaBlend blend, LapkaInput input);
int         lapka_layer_resize(LapkaLayer *l, LapkaRect rect);
void        lapka_layer_free(LapkaLayer *l);

void lapka_composite(LapkaFramebuffer *fb, LapkaLayer **layers, int n);