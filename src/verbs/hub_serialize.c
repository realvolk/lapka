/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "hub.h"

#include <stdio.h>
#include <string.h>

static int needs_quote(const char *s) {
    if (!s || !*s) return 1;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '_' || c == '-'
              || c == '.' || c == '/' || c == ':' || c == '@'
              || c == '+' || c == '=')) {
            return 1;
        }
    }
    return 0;
}

static void write_value(FILE *out, const char *s) {
    if (!s) s = "";
    if (!needs_quote(s)) {
        fputs(s, out);
        return;
    }
    fputc('\'', out);
    for (const char *p = s; *p; p++) {
        if (*p == '\'') fputs("'\\''", out);
        else fputc(*p, out);
    }
    fputc('\'', out);
}

int lapka_hub_serialize(LapkaHubDef *def, FILE *out) {
    for (int c = 0; c < def->category_count; c++) {
        LapkaHubCategory *cat = &def->categories[c];
        for (int i = 0; i < cat->item_count; i++) {
            LapkaHubItem *it = &cat->items[i];
            fputs(it->key, out);
            fputc('=', out);
            write_value(out, it->value);
            fputc('\n', out);
        }
    }
    return 0;
}