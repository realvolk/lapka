/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/verbs.h>
#include <stdbool.h>

bool lapka_term_init(const LapkaInitOptions *opts);
void lapka_term_shutdown(void);
void lapka_term_restore(void);
int  lapka_term_fd(void);
bool lapka_term_get_size(int *w, int *h);
int  lapka_term_color_depth(void);
void lapka_term_set_color_depth(int depth);