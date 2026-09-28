/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <core/utf8.h>

#include <stdint.h>
#include <stdio.h>

static int failures = 0;

static void check_int(int got, int want, const char *what) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d, want %d\n", what, got, want);
        failures++;
    }
}

static void check_u32(uint32_t got, uint32_t want, const char *what) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got U+%04X, want U+%04X\n", what, got, want);
        failures++;
    }
}

static void test_roundtrip(void) {
    uint32_t cps[] = {
        0x41, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF, 0x1F600
    };
    size_t n = sizeof(cps) / sizeof(cps[0]);
    for (size_t i = 0; i < n; i++) {
        uint8_t buf[4];
        int enc = lapka_utf8_encode(cps[i], buf);
        uint32_t back = 0;
        int dec = lapka_utf8_decode(buf, (size_t)enc, &back);
        check_int(dec, enc, "roundtrip consumed");
        check_u32(back, cps[i], "roundtrip value");
    }
}

static void test_decode_ascii(void) {
    uint32_t cp = 0;
    int n = lapka_utf8_decode((const uint8_t *)"A", 1, &cp);
    check_int(n, 1, "ascii bytes");
    check_u32(cp, 0x41, "ascii value");
}

static void test_decode_two_byte(void) {
    uint8_t buf[] = { 0xC3, 0xA9 };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 2, &cp);
    check_int(n, 2, "two-byte bytes");
    check_u32(cp, 0xE9, "two-byte value");
}

static void test_decode_three_byte(void) {
    uint8_t buf[] = { 0xE2, 0x82, 0xAC };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 3, &cp);
    check_int(n, 3, "three-byte bytes");
    check_u32(cp, 0x20AC, "three-byte value");
}

static void test_decode_four_byte(void) {
    uint8_t buf[] = { 0xF0, 0x9F, 0x98, 0x80 };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 4, &cp);
    check_int(n, 4, "four-byte bytes");
    check_u32(cp, 0x1F600, "four-byte value");
}

static void test_decode_invalid_lead(void) {
    uint32_t cp = 0;
    int n = lapka_utf8_decode((const uint8_t *)"\x80", 1, &cp);
    check_int(n, 1, "invalid lead bytes");
    check_u32(cp, 0xFFFD, "invalid lead value");
}

static void test_decode_truncated(void) {
    uint8_t buf[] = { 0xE2, 0x82 };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 2, &cp);
    check_int(n, 1, "truncated bytes");
    check_u32(cp, 0xFFFD, "truncated value");
}

static void test_decode_overlong(void) {
    uint8_t buf[] = { 0xC0, 0xAF };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 2, &cp);
    check_int(n, 1, "overlong bytes");
    check_u32(cp, 0xFFFD, "overlong value");
}

static void test_decode_surrogate(void) {
    uint8_t buf[] = { 0xED, 0xA0, 0x80 };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 3, &cp);
    check_int(n, 1, "surrogate bytes");
    check_u32(cp, 0xFFFD, "surrogate value");
}

static void test_decode_out_of_range(void) {
    uint8_t buf[] = { 0xF5, 0x80, 0x80, 0x80 };
    uint32_t cp = 0;
    int n = lapka_utf8_decode(buf, 4, &cp);
    check_int(n, 1, "out-of-range bytes");
    check_u32(cp, 0xFFFD, "out-of-range value");
}

static void test_decode_empty(void) {
    uint32_t cp = 0;
    int n = lapka_utf8_decode((const uint8_t *)"", 0, &cp);
    check_int(n, 0, "empty bytes");
    check_u32(cp, 0xFFFD, "empty value");
}

static void test_encode_surrogate_replaced(void) {
    uint8_t buf[4];
    int n = lapka_utf8_encode(0xD800, buf);
    check_int(n, 3, "surrogate encode bytes");
    uint32_t cp = 0;
    lapka_utf8_decode(buf, (size_t)n, &cp);
    check_u32(cp, 0xFFFD, "surrogate encode value");
}

static void test_width(void) {
    check_int(lapka_cp_width(0x41),    1, "width ascii");
    check_int(lapka_cp_width(0x00),    0, "width nul");
    check_int(lapka_cp_width(0x09),    1, "width tab");
    check_int(lapka_cp_width(0x1F),    0, "width control");
    check_int(lapka_cp_width(0x7F),    0, "width del");
    check_int(lapka_cp_width(0x0301),  0, "width combining acute");
    check_int(lapka_cp_width(0x200B),  0, "width zwsp");
    check_int(lapka_cp_width(0x4E00),  2, "width cjk");
    check_int(lapka_cp_width(0xFF01),  2, "width fullwidth");
    check_int(lapka_cp_width(0x1F600), 2, "width emoji");
    check_int(lapka_cp_width(0x00E9),  1, "width latin-1");
    check_int(lapka_cp_width(0xAC00),  2, "width hangul");
}

int main(void) {
    test_roundtrip();
    test_decode_ascii();
    test_decode_two_byte();
    test_decode_three_byte();
    test_decode_four_byte();
    test_decode_invalid_lead();
    test_decode_truncated();
    test_decode_overlong();
    test_decode_surrogate();
    test_decode_out_of_range();
    test_decode_empty();
    test_encode_surrogate_replaced();
    test_width();

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}