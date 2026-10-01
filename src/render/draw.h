/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include "framebuffer.h"

#include <lapka/types.h>

int lapka_draw_fill(LapkaFramebuffer *fb, LapkaRect r, LapkaCell c);
int lapka_draw_text(LapkaFramebuffer *fb, LapkaRect r, const char *text,
                    uint32_t fg, uint32_t bg, uint16_t attrs);
int lapka_draw_box(LapkaFramebuffer *fb, LapkaRect r, LapkaBorder border,
                   uint32_t fg, uint32_t bg);
int lapka_draw_hline(LapkaFramebuffer *fb, int x, int y, int w, uint32_t cp,
                     uint32_t fg, uint32_t bg);

int lapka_print_text(int fd, const char *text, int width,
                     uint32_t fg, uint32_t bg, uint16_t attrs);

typedef struct {
    const char *key;
    const char *label;
} LapkaHintItem;

int lapka_draw_hint(LapkaFramebuffer *fb, int y, const LapkaHintItem *items, int n,
                    uint32_t fg);

int lapka_put_cp(LapkaCell *buf, int w, int h, int x, int y, uint32_t cp,
                 uint32_t fg, uint32_t bg, uint16_t attrs);

int lapka_draw_str(LapkaCell *buf, int w, int h, int x, int y,
                   const char *s, uint32_t fg, uint32_t bg, uint16_t attrs,
                   int max_cols);