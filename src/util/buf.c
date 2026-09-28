/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "buf.h"

#include <stdlib.h>
#include <string.h>

void lapka_buf_init(LapkaBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void lapka_buf_free(LapkaBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void lapka_buf_clear(LapkaBuf *b) {
    b->len = 0;
}

int lapka_buf_reserve(LapkaBuf *b, size_t extra) {
    if (b->len + extra <= b->cap) return 0;

    size_t need = b->len + extra;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < need) {
        if (cap > (size_t)-1 / 2) return -1;
        cap *= 2;
    }

    char *p = realloc(b->data, cap);
    if (!p) return -1;
    b->data = p;
    b->cap = cap;
    return 0;
}

int lapka_buf_append(LapkaBuf *b, const char *s, size_t n) {
    if (n == 0) return 0;
    if (lapka_buf_reserve(b, n) != 0) return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return 0;
}

int lapka_buf_append_str(LapkaBuf *b, const char *s) {
    return lapka_buf_append(b, s, strlen(s));
}

int lapka_buf_append_char(LapkaBuf *b, char c) {
    return lapka_buf_append(b, &c, 1);
}