/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>
#include <stdbool.h>

typedef struct {
    uint32_t bg;
    uint32_t fg;
    uint32_t accent;
    uint32_t title;
    uint32_t muted;
    uint32_t error;
    uint32_t warning;
    uint32_t success;
    uint32_t modal_bg;
    uint32_t modal_border;
    uint32_t modal_title;
    LapkaBorder border;
    const char *cursor;
    const char *check;
    const char *radio_on;
    const char *radio_off;
    const char *bullet;
    const char *arrow;
    bool owns_strings;
} LapkaTheme;

extern const LapkaTheme LAPKA_THEME_DEFAULT;

uint32_t lapka_color_darken(uint32_t rgb, int percent);

int                lapka_theme_load(const char *path, LapkaTheme *out);
const LapkaTheme *lapka_theme_active(void);
int                lapka_theme_set_file(const char *path);
void               lapka_theme_reset(void);
void               lapka_theme_free(LapkaTheme *t);