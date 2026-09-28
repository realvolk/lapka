/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>

bool lapka_filter_fuzzy(const char *item, const char *query) {
    if (!query || query[0] == '\0') return true;
    if (!item) return false;

    const char *ip = item;
    const char *qp = query;

    while (*qp) {
        char qc = *qp;
        if (qc >= 'A' && qc <= 'Z') qc += 32;

        int found = 0;
        while (*ip) {
            char ic = *ip;
            if (ic >= 'A' && ic <= 'Z') ic += 32;
            ip++;
            if (ic == qc) { found = 1; break; }
        }
        if (!found) return false;
        qp++;
    }
    return true;
}