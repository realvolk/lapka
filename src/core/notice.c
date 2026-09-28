/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

static const char lapka_notice[]
    __attribute__((section(".notice")))
    __attribute__((used)) =
    "lapka - CLEAR License v1 - not GPL-compatible";

const char *lapka_notice_string(void) {
    return lapka_notice;
}