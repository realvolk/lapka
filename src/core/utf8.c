/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "utf8.h"

typedef struct {
    uint32_t start;
    uint32_t end;
} Range;

static const Range zero_width_ranges[] = {
    { 0x0300,  0x036F  },
    { 0x1AB0,  0x1AFF  },
    { 0x1DC0,  0x1DFF  },
    { 0x20D0,  0x20FF  },
    { 0xFE20,  0xFE2F  },
    { 0x200B,  0x200B  },
    { 0x200C,  0x200C  },
    { 0x200D,  0x200D  },
    { 0x2060,  0x2060  },
    { 0xFEFF,  0xFEFF  }
};

static const Range wide_ranges[] = {
    { 0x1100,  0x115F  },
    { 0x2329,  0x232A  },
    { 0x2E80,  0x2E99  },
    { 0x2E9B,  0x2EF3  },
    { 0x2F00,  0x2FD5  },
    { 0x2FF0,  0x2FFB  },
    { 0x3000,  0x303E  },
    { 0x3041,  0x3096  },
    { 0x3099,  0x30FF  },
    { 0x3105,  0x312F  },
    { 0x3131,  0x318E  },
    { 0x3190,  0x31E3  },
    { 0x31F0,  0x321E  },
    { 0x3220,  0x3247  },
    { 0x3250,  0x4DBF  },
    { 0x4E00,  0xA48C  },
    { 0xA490,  0xA4C6  },
    { 0xA960,  0xA97C  },
    { 0xAC00,  0xD7A3  },
    { 0xF900,  0xFAFF  },
    { 0xFE10,  0xFE19  },
    { 0xFE30,  0xFE52  },
    { 0xFE54,  0xFE66  },
    { 0xFE68,  0xFE6B  },
    { 0xFF01,  0xFF60  },
    { 0xFFE0,  0xFFE6  },
    { 0x16FE0, 0x16FE4 },
    { 0x16FF0, 0x16FF1 },
    { 0x17000, 0x187F7 },
    { 0x18800, 0x18CD5 },
    { 0x18D00, 0x18D08 },
    { 0x1B000, 0x1B11E },
    { 0x1B150, 0x1B152 },
    { 0x1B164, 0x1B167 },
    { 0x1B170, 0x1B2FB },
    { 0x1F004, 0x1F004 },
    { 0x1F0CF, 0x1F0CF },
    { 0x1F18E, 0x1F18E },
    { 0x1F191, 0x1F19A },
    { 0x1F200, 0x1F202 },
    { 0x1F210, 0x1F23B },
    { 0x1F240, 0x1F248 },
    { 0x1F250, 0x1F251 },
    { 0x1F260, 0x1F265 },
    { 0x1F300, 0x1F320 },
    { 0x1F32D, 0x1F335 },
    { 0x1F337, 0x1F37C },
    { 0x1F37E, 0x1F393 },
    { 0x1F3A0, 0x1F3CA },
    { 0x1F3CF, 0x1F3D3 },
    { 0x1F3E0, 0x1F3F0 },
    { 0x1F3F4, 0x1F3F4 },
    { 0x1F3F8, 0x1F43E },
    { 0x1F440, 0x1F440 },
    { 0x1F442, 0x1F4FC },
    { 0x1F4FF, 0x1F53D },
    { 0x1F54B, 0x1F54E },
    { 0x1F550, 0x1F567 },
    { 0x1F57A, 0x1F57A },
    { 0x1F595, 0x1F596 },
    { 0x1F5A4, 0x1F5A4 },
    { 0x1F5FB, 0x1F64F },
    { 0x1F680, 0x1F6C5 },
    { 0x1F6CC, 0x1F6CC },
    { 0x1F6D0, 0x1F6D2 },
    { 0x1F6D5, 0x1F6D7 },
    { 0x1F6EB, 0x1F6EC },
    { 0x1F6F4, 0x1F6FC },
    { 0x1F7E0, 0x1F7EB },
    { 0x1F90C, 0x1F93A },
    { 0x1F93C, 0x1F945 },
    { 0x1F947, 0x1F978 },
    { 0x1F97A, 0x1F9CB },
    { 0x1F9CD, 0x1F9FF },
    { 0x1FA70, 0x1FA74 },
    { 0x1FA78, 0x1FA7A },
    { 0x1FA80, 0x1FA86 },
    { 0x1FA90, 0x1FAA8 },
    { 0x1FAB0, 0x1FAB6 },
    { 0x1FAC0, 0x1FAC2 },
    { 0x1FAD0, 0x1FAD6 },
    { 0x20000, 0x2FFFD },
    { 0x30000, 0x3FFFD }
};

static int in_ranges(uint32_t cp, const Range *ranges, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (cp >= ranges[i].start && cp <= ranges[i].end) {
            return 1;
        }
    }
    return 0;
}

int lapka_cp_width(uint32_t cp) {
    if (cp == 0x09) {
        return 1;
    }
    if (cp < 0x20) {
        return 0;
    }
    if (cp == 0x7F) {
        return 0;
    }
    if (cp >= 0x80 && cp <= 0x9F) {
        return 0;
    }
    if (in_ranges(cp, zero_width_ranges,
                  sizeof(zero_width_ranges) / sizeof(zero_width_ranges[0]))) {
        return 0;
    }
    if (in_ranges(cp, wide_ranges,
                  sizeof(wide_ranges) / sizeof(wide_ranges[0]))) {
        return 2;
    }
    return 1;
}

int lapka_utf8_decode(const uint8_t *buf, size_t len, uint32_t *cp_out) {
    if (len == 0) {
        *cp_out = 0xFFFD;
        return 0;
    }

    uint8_t b0 = buf[0];

    if (b0 < 0x80) {
        *cp_out = b0;
        return 1;
    }

    if (b0 < 0xC2) {
        *cp_out = 0xFFFD;
        return 1;
    }

    if (b0 < 0xE0) {
        if (len < 2 || (buf[1] & 0xC0) != 0x80) {
            *cp_out = 0xFFFD;
            return 1;
        }
        *cp_out = ((uint32_t)(b0 & 0x1F) << 6) | (uint32_t)(buf[1] & 0x3F);
        return 2;
    }

    if (b0 < 0xF0) {
        if (len < 3 || (buf[1] & 0xC0) != 0x80 || (buf[2] & 0xC0) != 0x80) {
            *cp_out = 0xFFFD;
            return 1;
        }
        uint32_t cp = ((uint32_t)(b0 & 0x0F) << 12)
                    | ((uint32_t)(buf[1] & 0x3F) << 6)
                    | (uint32_t)(buf[2] & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) {
            *cp_out = 0xFFFD;
            return 1;
        }
        *cp_out = cp;
        return 3;
    }

    if (b0 < 0xF5) {
        if (len < 4 || (buf[1] & 0xC0) != 0x80 || (buf[2] & 0xC0) != 0x80 || (buf[3] & 0xC0) != 0x80) {
            *cp_out = 0xFFFD;
            return 1;
        }
        uint32_t cp = ((uint32_t)(b0 & 0x07) << 18)
                    | ((uint32_t)(buf[1] & 0x3F) << 12)
                    | ((uint32_t)(buf[2] & 0x3F) << 6)
                    | (uint32_t)(buf[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) {
            *cp_out = 0xFFFD;
            return 1;
        }
        *cp_out = cp;
        return 4;
    }

    *cp_out = 0xFFFD;
    return 1;
}

int lapka_utf8_encode(uint32_t cp, uint8_t *out) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        cp = 0xFFFD;
    }

    if (cp < 0x80) {
        out[0] = (uint8_t)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (uint8_t)(0xC0 | (cp >> 6));
        out[1] = (uint8_t)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (uint8_t)(0xE0 | (cp >> 12));
        out[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (uint8_t)(0xF0 | (cp >> 18));
    out[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (uint8_t)(0x80 | (cp & 0x3F));
    return 4;
}