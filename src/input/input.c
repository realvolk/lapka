/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "input.h"
#include "core/utf8.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static int read_one(int fd, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int rc = poll(&pfd, 1, timeout_ms);
    if (rc < 0) {
        if (errno == EINTR) return -2;
        return -1;
    }
    if (rc == 0) return -2;

    unsigned char c;
    ssize_t n = read(fd, &c, 1);
    if (n == 1) return (int)c;
    if (n == 0) return -1;
    if (errno == EINTR) return -2;
    return -1;
}

static int parse_csi_final(int final, int n1, int n2, LapkaEvent *out) {
    out->type = LAPKA_EVENT_KEY;
    out->key.ch = 0;
    out->key.mods = 0;

    switch (final) {
        case 'A': out->key.code = LAPKA_KEY_UP; return 0;
        case 'B': out->key.code = LAPKA_KEY_DOWN; return 0;
        case 'C': out->key.code = LAPKA_KEY_RIGHT; return 0;
        case 'D': out->key.code = LAPKA_KEY_LEFT; return 0;
        case 'H': out->key.code = LAPKA_KEY_HOME; return 0;
        case 'F': out->key.code = LAPKA_KEY_END; return 0;
        case 'Z': out->key.code = LAPKA_KEY_BACKTAB; return 0;
        case '~':
            switch (n1) {
                case 1: case 7: out->key.code = LAPKA_KEY_HOME; return 0;
                case 2: out->key.code = LAPKA_KEY_INSERT; return 0;
                case 3: out->key.code = LAPKA_KEY_DELETE; return 0;
                case 4: case 8: out->key.code = LAPKA_KEY_END; return 0;
                case 5: out->key.code = LAPKA_KEY_PAGEUP; return 0;
                case 6: out->key.code = LAPKA_KEY_PAGEDOWN; return 0;
                case 11: out->key.code = LAPKA_KEY_F1; return 0;
                case 12: out->key.code = LAPKA_KEY_F2; return 0;
                case 13: out->key.code = LAPKA_KEY_F3; return 0;
                case 14: out->key.code = LAPKA_KEY_F4; return 0;
                case 15: out->key.code = LAPKA_KEY_F5; return 0;
                case 17: out->key.code = LAPKA_KEY_F6; return 0;
                case 18: out->key.code = LAPKA_KEY_F7; return 0;
                case 19: out->key.code = LAPKA_KEY_F8; return 0;
                case 20: out->key.code = LAPKA_KEY_F9; return 0;
                case 21: out->key.code = LAPKA_KEY_F10; return 0;
                case 23: out->key.code = LAPKA_KEY_F11; return 0;
                case 24: out->key.code = LAPKA_KEY_F12; return 0;
                default: out->key.code = LAPKA_KEY_UNKNOWN; return 0;
            }
        default:
            (void)n2;
            out->key.code = LAPKA_KEY_UNKNOWN;
            return 0;
    }
}

static int parse_ss3_final(int final, LapkaEvent *out) {
    out->type = LAPKA_EVENT_KEY;
    out->key.ch = 0;
    out->key.mods = 0;

    switch (final) {
        case 'A': out->key.code = LAPKA_KEY_UP; return 0;
        case 'B': out->key.code = LAPKA_KEY_DOWN; return 0;
        case 'C': out->key.code = LAPKA_KEY_RIGHT; return 0;
        case 'D': out->key.code = LAPKA_KEY_LEFT; return 0;
        case 'H': out->key.code = LAPKA_KEY_HOME; return 0;
        case 'F': out->key.code = LAPKA_KEY_END; return 0;
        case 'P': out->key.code = LAPKA_KEY_F1; return 0;
        case 'Q': out->key.code = LAPKA_KEY_F2; return 0;
        case 'R': out->key.code = LAPKA_KEY_F3; return 0;
        case 'S': out->key.code = LAPKA_KEY_F4; return 0;
        default: out->key.code = LAPKA_KEY_UNKNOWN; return 0;
    }
}

static int parse_sgr_mouse(int fd, LapkaEvent *out) {
    int fields[3] = { 0, 0, 0 };
    int field = 0;
    int c;

    for (;;) {
        c = read_one(fd, 25);
        if (c == -2) return -1;
        if (c == -1) return -1;

        if (c >= '0' && c <= '9') {
            fields[field] = fields[field] * 10 + (c - '0');
            if (fields[field] > 100000) return -1;
        } else if (c == ';') {
            field++;
            if (field > 2) return -1;
        } else if (c == 'M' || c == 'm') {
            break;
        } else {
            return -1;
        }
    }

    int cb = fields[0];
    int mx = fields[1] - 1;
    int my = fields[2] - 1;

    out->type = LAPKA_EVENT_MOUSE;
    out->mouse.x = mx;
    out->mouse.y = my;
    out->mouse.mods = 0;
    if (cb & 0x04) out->mouse.mods |= LAPKA_MOD_SHIFT;
    if (cb & 0x08) out->mouse.mods |= LAPKA_MOD_ALT;
    if (cb & 0x10) out->mouse.mods |= LAPKA_MOD_CTRL;

    if (cb & 0x40) {
        out->mouse.button = 0;
        out->mouse.action = (cb & 0x01) ? LAPKA_MOUSE_WHEEL_DOWN
                                        : LAPKA_MOUSE_WHEEL_UP;
    } else if (cb & 0x20) {
        out->mouse.button = cb & 0x03;
        out->mouse.action = LAPKA_MOUSE_MOVE;
    } else if (c == 'M') {
        out->mouse.button = cb & 0x03;
        out->mouse.action = LAPKA_MOUSE_PRESS;
    } else {
        out->mouse.button = cb & 0x03;
        out->mouse.action = LAPKA_MOUSE_RELEASE;
    }
    return 0;
}

static int parse_escape(int fd, LapkaEvent *out) {
    int c2 = read_one(fd, 25);

    if (c2 == -2) {
        out->type = LAPKA_EVENT_KEY;
        out->key.code = LAPKA_KEY_ESC;
        out->key.ch = 0;
        out->key.mods = 0;
        return 0;
    }
    if (c2 == -1) return -1;

    if (c2 == '[') {
        int c3 = read_one(fd, 25);
        if (c3 == -2) {
            out->type = LAPKA_EVENT_KEY;
            out->key.code = LAPKA_KEY_ESC;
            out->key.ch = 0;
            out->key.mods = 0;
            return 0;
        }
        if (c3 == -1) return -1;

        if (c3 == '<') {
            return parse_sgr_mouse(fd, out);
        }

        if (c3 >= '0' && c3 <= '9') {
            int n1 = c3 - '0';
            int c4 = read_one(fd, 25);
            while (c4 >= '0' && c4 <= '9') {
                n1 = n1 * 10 + (c4 - '0');
                c4 = read_one(fd, 25);
            }
            int n2 = 0;
            if (c4 == ';') {
                int c5 = read_one(fd, 25);
                while (c5 >= '0' && c5 <= '9') {
                    n2 = n2 * 10 + (c5 - '0');
                    c5 = read_one(fd, 25);
                }
                c4 = c5;
            }
            while (c4 != -2 && c4 != -1 && (c4 < 0x40 || c4 > 0x7E)) {
                c4 = read_one(fd, 25);
            }
            if (c4 == -2 || c4 == -1) {
                out->type = LAPKA_EVENT_KEY;
                out->key.code = LAPKA_KEY_UNKNOWN;
                out->key.ch = 0;
                out->key.mods = 0;
                return 0;
            }
            return parse_csi_final(c4, n1, n2, out);
        }

        return parse_csi_final(c3, 0, 0, out);
    }

    if (c2 == 'O') {
        int c3 = read_one(fd, 25);
        if (c3 == -2 || c3 == -1) {
            out->type = LAPKA_EVENT_KEY;
            out->key.code = LAPKA_KEY_ESC;
            out->key.ch = 0;
            out->key.mods = 0;
            return 0;
        }
        return parse_ss3_final(c3, out);
    }

    out->type = LAPKA_EVENT_KEY;
    out->key.code = LAPKA_KEY_ESC;
    out->key.ch = 0;
    out->key.mods = 0;
    return 0;
}

static int decode_utf8(int fd, unsigned char lead, LapkaEvent *out) {
    int expected;
    if (lead < 0xC2) return -1;
    else if (lead < 0xE0) expected = 2;
    else if (lead < 0xF0) expected = 3;
    else if (lead < 0xF5) expected = 4;
    else return -1;

    uint8_t buf[4];
    buf[0] = lead;
    for (int i = 1; i < expected; i++) {
        int c = read_one(fd, 100);
        if (c < 0) return -1;
        buf[i] = (uint8_t)c;
    }

    uint32_t cp;
    int n = lapka_utf8_decode(buf, (size_t)expected, &cp);
    if (n != expected || cp == 0xFFFD) return -1;

    out->type = LAPKA_EVENT_KEY;
    out->key.code = LAPKA_KEY_CODEPOINT;
    out->key.ch = cp;
    out->key.mods = 0;
    return 0;
}

int lapka_event_read_timeout(int fd, LapkaEvent *out, int timeout_ms) {
    memset(out, 0, sizeof(*out));

    int c = read_one(fd, timeout_ms);
    if (c == -1) return -1;
    if (c == -2) {
        out->type = LAPKA_EVENT_NONE;
        return 1;
    }

    if (c == 0x1B) {
        return parse_escape(fd, out);
    }

    if (c == 0x03) {
        out->type = LAPKA_EVENT_KEY;
        out->key.code = LAPKA_KEY_CODEPOINT;
        out->key.ch = 0x03;
        out->key.mods = LAPKA_MOD_CTRL;
        return 0;
    }

    if (c == '\r' || c == '\n') {
        out->type = LAPKA_EVENT_KEY;
        out->key.code = LAPKA_KEY_ENTER;
        return 0;
    }

    if (c == '\t') {
        out->type = LAPKA_EVENT_KEY;
        out->key.code = LAPKA_KEY_TAB;
        return 0;
    }

    if (c == 0x7F || c == 0x08) {
        out->type = LAPKA_EVENT_KEY;
        out->key.code = LAPKA_KEY_BACKSPACE;
        return 0;
    }

    if (c < 0x80) {
        out->type = LAPKA_EVENT_KEY;
        out->key.code = LAPKA_KEY_CODEPOINT;
        out->key.ch = (uint32_t)c;
        out->key.mods = 0;
        return 0;
    }

    return decode_utf8(fd, (unsigned char)c, out);
}

int lapka_event_read(int fd, LapkaEvent *out) {
    return lapka_event_read_timeout(fd, out, -1);
}