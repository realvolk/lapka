/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>

int  lapka_fb_init(LapkaFramebuffer *fb, int w, int h);
void lapka_fb_free(LapkaFramebuffer *fb);
int  lapka_fb_resize(LapkaFramebuffer *fb, int w, int h);
void lapka_fb_clear(LapkaFramebuffer *fb);
void lapka_fb_mark_full_dirty(LapkaFramebuffer *fb);
int  lapka_fb_put(LapkaFramebuffer *fb, int x, int y, LapkaCell c);
LapkaCell lapka_fb_get(const LapkaFramebuffer *fb, int x, int y);