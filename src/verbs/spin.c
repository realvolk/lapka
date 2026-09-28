/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "spin.h"

#include <lapka/lapka.h>
#include <render/draw.h>
#include <proc/proc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define SPIN_RING 10
#define SPIN_LINE 256

static const char *SPIN_FRAMES[] = {
    "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
    "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
    "\xe2\xa0\x87", "\xe2\xa0\x8f",
};

typedef enum {
    SPIN_HOLD_AUTO,
    SPIN_HOLD_ENTER,
    SPIN_HOLD_NONE,
    SPIN_HOLD_MSEC,
} SpinHoldMode;

typedef struct {
    const char     *title;
    const char     *body;
    LapkaAnchor     anchor;
    SpinHoldMode    hold_mode;
    long long       hold_ms;

    LapkaProc   proc;
    int         proc_started;

    char        lines[SPIN_RING][SPIN_LINE];
    int         line_count;
    int         line_head;
    char        partial[SPIN_LINE];
    int         partial_len;

    int         spinner_frame;
    int         spinner_tick;

    int             exit_code;
    int             hold_wait;
    int             finished;
    long long       finish_ms;
    SpinHoldMode    effective_hold;
} SpinData;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void push_line(SpinData *d, const char *s, size_t n) {
    if (n >= SPIN_LINE) n = SPIN_LINE - 1;
    memcpy(d->lines[d->line_head], s, n);
    d->lines[d->line_head][n] = '\0';
    d->line_head = (d->line_head + 1) % SPIN_RING;
    if (d->line_count < SPIN_RING) d->line_count++;
}

static void feed_bytes(SpinData *d, const char *buf, int n) {
    for (int i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '\n') {
            push_line(d, d->partial, (size_t)d->partial_len);
            d->partial_len = 0;
        } else if (c == '\r') {
        } else {
            if (d->partial_len < SPIN_LINE - 1) {
                d->partial[d->partial_len++] = c;
            }
        }
    }
}

static bool spin_tick(void *ud) {
    SpinData *d = ud;

    if (d->proc_started && d->proc.fd >= 0) {
        char buf[4096];
        for (;;) {
            int n = lapka_proc_read(&d->proc, buf, sizeof(buf));
            if (n <= 0) break;
            feed_bytes(d, buf, n);
        }
    }

    if (d->proc_started && !d->proc.done) {
        int rc = lapka_proc_reap(&d->proc);
        if (rc == 1) {
            if (WIFEXITED(d->proc.status)) {
                d->exit_code = WEXITSTATUS(d->proc.status);
            } else if (WIFSIGNALED(d->proc.status)) {
                d->exit_code = 128 + WTERMSIG(d->proc.status);
            } else {
                d->exit_code = 1;
            }
            if (d->partial_len > 0) {
                push_line(d, d->partial, (size_t)d->partial_len);
                d->partial_len = 0;
            }
            d->finished = 1;
            d->finish_ms = now_ms();

            SpinHoldMode mode = d->hold_mode;
            if (mode == SPIN_HOLD_AUTO) {
                mode = SPIN_HOLD_MSEC;
                d->hold_ms = 3000;
            }
            d->effective_hold = mode;

            if (mode == SPIN_HOLD_ENTER) {
                d->hold_wait = 1;
            } else if (mode == SPIN_HOLD_NONE) {
                return true;
            }
        }
    }

    if (d->finished) {
        if (d->effective_hold == SPIN_HOLD_MSEC) {
            if (now_ms() - d->finish_ms >= d->hold_ms) return true;
        }
    } else {
        d->spinner_tick++;
        if (d->spinner_tick >= 1) {
            d->spinner_tick = 0;
            d->spinner_frame = (d->spinner_frame + 1) % 10;
        }
    }

    return false;
}

static bool spin_handle(void *ud, const LapkaEvent *ev) {
    SpinData *d = ud;

    if (d->hold_wait) {
        if (ev->type == LAPKA_EVENT_KEY) return true;
        return false;
    }

    if (ev->type == LAPKA_EVENT_KEY &&
        ev->key.code == LAPKA_KEY_CODEPOINT &&
        ev->key.ch == 0x03 && (ev->key.mods & LAPKA_MOD_CTRL)) {
        if (d->proc_started) lapka_proc_terminate(&d->proc);
        d->exit_code = 130;
        return true;
    }

    return false;
}

static int anchor_row(LapkaAnchor a, int top, int bottom) {
    switch (a) {
        case LAPKA_ANCHOR_TL:
        case LAPKA_ANCHOR_TC:
        case LAPKA_ANCHOR_TR:
            return top;
        case LAPKA_ANCHOR_LC:
        case LAPKA_ANCHOR_CC:
        case LAPKA_ANCHOR_RC:
            return (top + bottom) / 2;
        default:
            return bottom;
    }
}

static int anchor_col(LapkaAnchor a, int width, int len) {
    switch (a) {
        case LAPKA_ANCHOR_TR:
        case LAPKA_ANCHOR_RC:
        case LAPKA_ANCHOR_BR:
            return width - len;
        case LAPKA_ANCHOR_TC:
        case LAPKA_ANCHOR_CC:
        case LAPKA_ANCHOR_BC:
            return (width - len) / 2;
        default:
            return 0;
    }
}

static int anchor_vertical_top(LapkaAnchor a) {
    switch (a) {
        case LAPKA_ANCHOR_TL:
        case LAPKA_ANCHOR_TC:
        case LAPKA_ANCHOR_TR:
            return 1;
        default:
            return 0;
    }
}

static void spin_draw(void *ud, LapkaFramebuffer *fb) {
    SpinData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    int y = 2;
    if (d->body && d->body[0]) {
        LapkaRect body_r = { 0, y, fb->w, 1 };
        lapka_draw_text(fb, body_r, d->body, 0, 0, 0);
        y += 2;
    }

    int sp_row = anchor_row(d->anchor, y, fb->h - 2);
    if (sp_row < y) sp_row = y;
    if (sp_row > fb->h - 2) sp_row = fb->h - 2;

    int out_top, out_bottom;
    if (anchor_vertical_top(d->anchor)) {
        out_top = sp_row + 2;
        out_bottom = fb->h - 1;
    } else {
        out_top = y;
        out_bottom = sp_row - 1;
    }
    int out_h = out_bottom - out_top;
    if (out_h < 0) out_h = 0;

    int visible = d->line_count < out_h ? d->line_count : out_h;
    for (int i = 0; i < visible; i++) {
        int src = (d->line_head - visible + i + SPIN_RING) % SPIN_RING;
        LapkaRect r = { 0, out_top + i, fb->w, 1 };
        lapka_draw_text(fb, r, d->lines[src], theme->muted, 0, 0);
    }

    char sp_buf[512];
    if (d->finished) {
        const char *mark = (d->exit_code == 0) ? "*" : "!";
        snprintf(sp_buf, sizeof(sp_buf), "%s %s", mark, d->title);
    } else {
        const char *frame = SPIN_FRAMES[d->spinner_frame % 10];
        snprintf(sp_buf, sizeof(sp_buf), "%s %s", frame, d->title);
    }
    int sp_len = (int)strlen(sp_buf);
    int sp_col = anchor_col(d->anchor, fb->w, sp_len);
    if (sp_col < 0) sp_col = 0;
    LapkaRect sp_r = { sp_col, sp_row, fb->w - sp_col, 1 };
    uint32_t sp_fg = (d->finished && d->exit_code != 0)
        ? theme->error
        : theme->title;
    lapka_draw_text(fb, sp_r, sp_buf, sp_fg, 0, LAPKA_ATTR_BOLD);

    if (d->hold_wait) {
        LapkaRect hint_r = { 0, fb->h - 1, fb->w, 1 };
        lapka_draw_text(fb, hint_r, "enter acknowledge", theme->muted, 0, 0);
    }
}

static int parse_anchor(const char *s, LapkaAnchor *out) {
    if (strcmp(s, "TL") == 0) { *out = LAPKA_ANCHOR_TL; return 0; }
    if (strcmp(s, "TC") == 0) { *out = LAPKA_ANCHOR_TC; return 0; }
    if (strcmp(s, "TR") == 0) { *out = LAPKA_ANCHOR_TR; return 0; }
    if (strcmp(s, "LC") == 0) { *out = LAPKA_ANCHOR_LC; return 0; }
    if (strcmp(s, "CC") == 0) { *out = LAPKA_ANCHOR_CC; return 0; }
    if (strcmp(s, "RC") == 0) { *out = LAPKA_ANCHOR_RC; return 0; }
    if (strcmp(s, "BL") == 0) { *out = LAPKA_ANCHOR_BL; return 0; }
    if (strcmp(s, "BC") == 0) { *out = LAPKA_ANCHOR_BC; return 0; }
    if (strcmp(s, "BR") == 0) { *out = LAPKA_ANCHOR_BR; return 0; }
    return -1;
}

int cmd_spin(int argc, char **argv) {
    const char *title = NULL;
    const char *body = NULL;
    LapkaAnchor anchor = LAPKA_ANCHOR_BL;
    SpinHoldMode hold_mode = SPIN_HOLD_AUTO;
    long long hold_ms = 0;
    int split = -1;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) { split = i; break; }
    }
    if (split < 1) return 3;
    if (argv[split + 1] == NULL) return 3;

    int pos = 0;
    for (int i = 0; i < split; i++) {
        const char *a = argv[i];

        if (strncmp(a, "--where=", 8) == 0) {
            if (parse_anchor(a + 8, &anchor) != 0) {
                fprintf(stderr, "tui: spin: --where must be one of "
                                "TL TC TR LC CC RC BL BC BR\n");
                return 3;
            }
            continue;
        }
        if (strcmp(a, "--where") == 0) {
            if (i + 1 >= split) return 3;
            if (parse_anchor(argv[++i], &anchor) != 0) {
                fprintf(stderr, "tui: spin: --where must be one of "
                                "TL TC TR LC CC RC BL BC BR\n");
                return 3;
            }
            continue;
        }

        if (strncmp(a, "--hold=", 7) == 0) {
            const char *v = a + 7;
            if (strcmp(v, "enter") == 0) {
                hold_mode = SPIN_HOLD_ENTER;
            } else if (strcmp(v, "none") == 0) {
                hold_mode = SPIN_HOLD_NONE;
            } else {
                char *end = NULL;
                long secs = strtol(v, &end, 10);
                if (!end || *end != '\0' || secs < 0) return 3;
                hold_mode = SPIN_HOLD_MSEC;
                hold_ms = (long long)secs * 1000;
            }
            continue;
        }
        if (strcmp(a, "--hold") == 0) {
            if (i + 1 >= split) return 3;
            const char *v = argv[++i];
            if (strcmp(v, "enter") == 0) {
                hold_mode = SPIN_HOLD_ENTER;
            } else if (strcmp(v, "none") == 0) {
                hold_mode = SPIN_HOLD_NONE;
            } else {
                char *end = NULL;
                long secs = strtol(v, &end, 10);
                if (!end || *end != '\0' || secs < 0) return 3;
                hold_mode = SPIN_HOLD_MSEC;
                hold_ms = (long long)secs * 1000;
            }
            continue;
        }

        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') {
            fprintf(stderr, "tui: spin: unknown flag: %s\n", a);
            return 3;
        }
        if (pos == 0) title = a;
        else if (pos == 1) body = a;
        else return 3;
        pos++;
    }

    if (!title) return 3;

    const char *const *child_argv = (const char *const *)&argv[split + 1];

    SpinData d;
    memset(&d, 0, sizeof(d));
    d.title = title;
    d.body = (body && body[0]) ? body : NULL;
    d.anchor = anchor;
    d.hold_mode = hold_mode;
    d.hold_ms = hold_ms;
    d.proc.fd = -1;

    if (lapka_proc_start(child_argv, &d.proc) != 0) {
        fprintf(stderr, "tui: spin: failed to spawn command\n");
        return 2;
    }
    d.proc_started = 1;

    LapkaInitOptions opts = { 1, 0, 0, 0 };
    LapkaRunCallbacks cb = { spin_draw, spin_handle, spin_tick, &d };

    int rc = lapka_run(&opts, &cb, d.proc.fd);

    if (d.proc_started) lapka_proc_close(&d.proc);

    if (rc != 0) return rc;
    return d.exit_code;
}