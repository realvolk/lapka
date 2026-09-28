/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "capabilities.h"

#include <stdlib.h>
#include <string.h>

static bool term_is(const char *term, const char *name) {
    if (!term || !name) return false;
    size_t n = strlen(name);
    if (strncmp(term, name, n) != 0) return false;
    return term[n] == '\0' || term[n] == '-';
}

bool lapka_cap_is_basic_term(void) {
    const char *t = getenv("TERM");
    if (!t || t[0] == '\0') return true;
    return term_is(t, "dumb")
        || term_is(t, "linux")
        || term_is(t, "vt100")
        || term_is(t, "vt102");
}

bool lapka_cap_sync_supported(void) {
    return !lapka_cap_is_basic_term();
}

int lapka_cap_color_depth(void) {
    const char *ct = getenv("COLORTERM");
    if (ct && (strcmp(ct, "truecolor") == 0 || strcmp(ct, "24bit") == 0)) {
        return LAPKA_COLOR_TRUECOLOR;
    }
    const char *t = getenv("TERM");
    if (t && strstr(t, "256color") != NULL) {
        return LAPKA_COLOR_256;
    }
    if (lapka_cap_is_basic_term()) {
        return LAPKA_COLOR_MONO;
    }
    return LAPKA_COLOR_16;
}