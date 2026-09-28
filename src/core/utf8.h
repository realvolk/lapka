/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

int lapka_cp_width(uint32_t cp);
int lapka_utf8_decode(const uint8_t *buf, size_t len, uint32_t *cp_out);
int lapka_utf8_encode(uint32_t cp, uint8_t *out);