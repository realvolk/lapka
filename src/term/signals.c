/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "signals.h"
#include "term.h"
#include "ansi.h"

#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t s_restored = 0;
static volatile sig_atomic_t s_resize_pending = 0;
static volatile sig_atomic_t s_in_handler = 0;
static int                    s_tty_fd = -1;
static struct termios         s_orig;
static int                    s_orig_valid = 0;
static int                    s_alt_screen = 0;
static int                    s_installed = 0;

void lapka_term_restore(void) {
    if (s_restored) return;
    s_restored = 1;

    if (s_tty_fd < 0) return;

    if (s_orig_valid) {
        tcsetattr(s_tty_fd, TCSANOW, &s_orig);
    }

    static const char seq[] =
        "\033[0m"
        "\033[?1000l"
        "\033[?1006l"
        "\033[?2004l"
        "\033[?1004l"
        "\033[?2026l"
        "\033[?25h";
    lapka_write_all(s_tty_fd, seq, sizeof(seq) - 1);

    if (s_alt_screen) {
        lapka_ansi_alt_leave(s_tty_fd);
    }

    if (!s_in_handler) {
        tcdrain(s_tty_fd);
    }
}

static void exit_handler(int signo) {
    s_in_handler = 1;
    lapka_term_restore();
    _exit(128 + signo);
}

static void winch_handler(int signo) {
    (void)signo;
    s_resize_pending = 1;
}

void lapka_signals_save(int fd, const struct termios *orig, bool alt_screen) {
    s_tty_fd = fd;
    if (orig) {
        memcpy(&s_orig, orig, sizeof(s_orig));
        s_orig_valid = 1;
    } else {
        s_orig_valid = 0;
    }
    s_alt_screen = alt_screen ? 1 : 0;
    s_restored = 0;
}

void lapka_signals_install(void) {
    if (s_installed) return;
    s_installed = 1;

    int exit_signals[] = { SIGINT, SIGTERM, SIGHUP, SIGSEGV, SIGABRT, SIGPIPE };
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = exit_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    for (size_t i = 0; i < sizeof(exit_signals) / sizeof(exit_signals[0]); i++) {
        sigaction(exit_signals[i], &sa, NULL);
    }

    struct sigaction wa;
    memset(&wa, 0, sizeof(wa));
    wa.sa_handler = winch_handler;
    sigemptyset(&wa.sa_mask);
    wa.sa_flags = 0;
    sigaction(SIGWINCH, &wa, NULL);
}

bool lapka_signals_take_resize(void) {
    if (!s_resize_pending) return false;
    s_resize_pending = 0;
    return true;
}

int lapka_signals_fd(void) {
    return s_tty_fd;
}