/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "ansi.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int lapka_write_all(int fd, const char *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = write(fd, buf + off, n - off);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;
        off += (size_t)r;
    }
    return 0;
}

#define LAPKA_STATIC_WRITE(fd, s) lapka_write_all((fd), (s), sizeof(s) - 1)

void lapka_ansi_alt_enter(int fd)        { LAPKA_STATIC_WRITE(fd, "\033[?1049h"); }
void lapka_ansi_alt_leave(int fd)        { LAPKA_STATIC_WRITE(fd, "\033[?1049l"); }
void lapka_ansi_cursor_hide(int fd)      { LAPKA_STATIC_WRITE(fd, "\033[?25l"); }
void lapka_ansi_cursor_show(int fd)      { LAPKA_STATIC_WRITE(fd, "\033[?25h"); }
void lapka_ansi_mouse_enable(int fd)     { LAPKA_STATIC_WRITE(fd, "\033[?1000h\033[?1006h"); }
void lapka_ansi_mouse_disable(int fd)    { LAPKA_STATIC_WRITE(fd, "\033[?1000l\033[?1006l"); }
void lapka_ansi_paste_enable(int fd)     { LAPKA_STATIC_WRITE(fd, "\033[?2004h"); }
void lapka_ansi_paste_disable(int fd)    { LAPKA_STATIC_WRITE(fd, "\033[?2004l"); }
void lapka_ansi_focus_enable(int fd)     { LAPKA_STATIC_WRITE(fd, "\033[?1004h"); }
void lapka_ansi_focus_disable(int fd)    { LAPKA_STATIC_WRITE(fd, "\033[?1004l"); }
void lapka_ansi_sync_begin(int fd)       { LAPKA_STATIC_WRITE(fd, "\033[?2026h"); }
void lapka_ansi_sync_end(int fd)         { LAPKA_STATIC_WRITE(fd, "\033[?2026l"); }
void lapka_ansi_sgr_reset(int fd)        { LAPKA_STATIC_WRITE(fd, "\033[0m"); }
void lapka_ansi_clear_screen(int fd)     { LAPKA_STATIC_WRITE(fd, "\033[2J\033[H"); }

void lapka_ansi_cursor_to(int fd, int x, int y) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
    if (n < 0 || (size_t)n >= sizeof(buf)) return;
    lapka_write_all(fd, buf, (size_t)n);
}

static size_t append(char *buf, size_t cap, size_t off, const char *s, size_t n) {
    if (off + n > cap) return off;
    for (size_t i = 0; i < n; i++) buf[off + i] = s[i];
    return off + n;
}

static size_t append_u32(char *buf, size_t cap, size_t off, uint32_t v) {
    char tmp[16];
    int n = snprintf(tmp, sizeof(tmp), "%u", v);
    if (n < 0) return off;
    return append(buf, cap, off, tmp, (size_t)n);
}

size_t lapka_ansi_sgr_build(char *buf, size_t cap,
                            uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (cap < 4) return 0;
    size_t off = 0;
    off = append(buf, cap, off, "\033[0", 3);

    if (attrs & LAPKA_ATTR_BOLD)      off = append(buf, cap, off, ";1", 2);
    if (attrs & LAPKA_ATTR_DIM)       off = append(buf, cap, off, ";2", 2);
    if (attrs & LAPKA_ATTR_ITALIC)    off = append(buf, cap, off, ";3", 2);
    if (attrs & LAPKA_ATTR_UNDERLINE) off = append(buf, cap, off, ";4", 2);
    if (attrs & LAPKA_ATTR_BLINK)     off = append(buf, cap, off, ";5", 2);
    if (attrs & LAPKA_ATTR_REVERSE)   off = append(buf, cap, off, ";7", 2);
    if (attrs & LAPKA_ATTR_STRIKE)    off = append(buf, cap, off, ";9", 2);

    if (fg != 0) {
        off = append(buf, cap, off, ";38;2;", 6);
        off = append_u32(buf, cap, off, (fg >> 16) & 0xFF);
        off = append(buf, cap, off, ";", 1);
        off = append_u32(buf, cap, off, (fg >> 8) & 0xFF);
        off = append(buf, cap, off, ";", 1);
        off = append_u32(buf, cap, off, fg & 0xFF);
    }
    if (bg != 0) {
        off = append(buf, cap, off, ";48;2;", 6);
        off = append_u32(buf, cap, off, (bg >> 16) & 0xFF);
        off = append(buf, cap, off, ";", 1);
        off = append_u32(buf, cap, off, (bg >> 8) & 0xFF);
        off = append(buf, cap, off, ";", 1);
        off = append_u32(buf, cap, off, bg & 0xFF);
    }
    off = append(buf, cap, off, "m", 1);

    if (off < cap) buf[off] = '\0';
    return off;
}