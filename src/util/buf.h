/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} LapkaBuf;

void  lapka_buf_init(LapkaBuf *b);
void  lapka_buf_free(LapkaBuf *b);
void  lapka_buf_clear(LapkaBuf *b);
int   lapka_buf_reserve(LapkaBuf *b, size_t extra);
int   lapka_buf_append(LapkaBuf *b, const char *s, size_t n);
int   lapka_buf_append_str(LapkaBuf *b, const char *s);
int   lapka_buf_append_char(LapkaBuf *b, char c);