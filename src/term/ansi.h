/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>
#include <stddef.h>
#include <stdint.h>

int  lapka_write_all(int fd, const char *buf, size_t n);

void lapka_ansi_alt_enter(int fd);
void lapka_ansi_alt_leave(int fd);
void lapka_ansi_cursor_hide(int fd);
void lapka_ansi_cursor_show(int fd);
void lapka_ansi_mouse_enable(int fd);
void lapka_ansi_mouse_disable(int fd);
void lapka_ansi_paste_enable(int fd);
void lapka_ansi_paste_disable(int fd);
void lapka_ansi_focus_enable(int fd);
void lapka_ansi_focus_disable(int fd);
void lapka_ansi_sync_begin(int fd);
void lapka_ansi_sync_end(int fd);
void lapka_ansi_sgr_reset(int fd);
void lapka_ansi_cursor_to(int fd, int x, int y);
void lapka_ansi_clear_screen(int fd);

size_t lapka_ansi_sgr_build(char *buf, size_t cap,
                            uint32_t fg, uint32_t bg, uint16_t attrs);