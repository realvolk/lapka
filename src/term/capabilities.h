/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stdbool.h>

#define LAPKA_COLOR_MONO       1
#define LAPKA_COLOR_16         4
#define LAPKA_COLOR_256        8
#define LAPKA_COLOR_TRUECOLOR  24

bool lapka_cap_sync_supported(void);
bool lapka_cap_is_basic_term(void);
int  lapka_cap_color_depth(void);