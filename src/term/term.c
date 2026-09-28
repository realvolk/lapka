/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "term.h"
#include "signals.h"
#include "ansi.h"
#include "capabilities.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static int s_inited = 0;
static int s_color_depth = LAPKA_COLOR_16;

static void apply_cooked_defaults(struct termios *t) {
    t->c_iflag = ICRNL | IXON;
    t->c_oflag = OPOST | ONLCR;
    t->c_cflag = CS8 | CREAD;
    t->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}

static void apply_raw(struct termios *t) {
    t->c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    t->c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    t->c_cflag |= CS8;
    t->c_oflag &= ~(tcflag_t)OPOST;
    t->c_cc[VMIN] = 0;
    t->c_cc[VTIME] = 0;
}

static int alt_screen_inherited(void) {
    const char *v = getenv("LAPKA_ALT_SCREEN");
    return v && v[0] == '1';
}

bool lapka_term_init(const LapkaInitOptions *opts) {
    if (s_inited) return true;

    int fd = open("/dev/tty", O_RDWR);
    if (fd < 0) return false;

    struct termios orig;
    if (tcgetattr(fd, &orig) != 0) {
        close(fd);
        return false;
    }

    if (!(orig.c_lflag & ECHO) && !(orig.c_lflag & ICANON)) {
        apply_cooked_defaults(&orig);
        tcsetattr(fd, TCSANOW, &orig);
    }

    bool alt = opts && opts->alt_screen;
    if (alt_screen_inherited()) alt = false;

    lapka_signals_save(fd, &orig, alt);
    lapka_signals_install();

    struct termios raw = orig;
    apply_raw(&raw);
    tcsetattr(fd, TCSAFLUSH, &raw);

    if (alt) {
        lapka_ansi_alt_enter(fd);
    }
    lapka_ansi_cursor_hide(fd);

    if (opts && opts->mouse) lapka_ansi_mouse_enable(fd);
    if (opts && opts->paste) lapka_ansi_paste_enable(fd);
    if (opts && opts->focus) lapka_ansi_focus_enable(fd);

    s_color_depth = lapka_cap_color_depth();
    s_inited = 1;
    atexit(lapka_term_shutdown);
    return true;
}

void lapka_term_shutdown(void) {
    if (!s_inited) return;
    lapka_term_restore();
    s_inited = 0;
}

int lapka_term_fd(void) {
    return lapka_signals_fd();
}

bool lapka_term_get_size(int *w, int *h) {
    struct winsize ws;
    int fd = lapka_signals_fd();
    if (fd < 0) {
        *w = 80;
        *h = 24;
        return false;
    }
    if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *w = ws.ws_col;
        *h = ws.ws_row;
        return true;
    }
    *w = 80;
    *h = 24;
    return false;
}

int lapka_term_color_depth(void) {
    return s_color_depth;
}

void lapka_term_set_color_depth(int depth) {
    s_color_depth = depth;
}