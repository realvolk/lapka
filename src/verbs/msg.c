/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "msg.h"

#include <lapka/lapka.h>
#include <render/draw.h>

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *title;
    const char *body;
    int         done;
} MsgData;

static void msg_draw(void *ud, LapkaFramebuffer *fb) {
    MsgData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    LapkaRect body_r = { 0, 2, fb->w, fb->h > 2 ? fb->h - 2 : 0 };
    lapka_draw_text(fb, body_r, d->body, 0, 0, 0);

    LapkaRect hint_r = { 0, fb->h - 1, fb->w, 1 };
    lapka_draw_text(fb, hint_r, "enter continue", theme->muted, 0, 0);
}

static bool msg_handle(void *ud, const LapkaEvent *ev) {
    MsgData *d = ud;
    if (ev->type != LAPKA_EVENT_KEY) return false;
    if (ev->key.code == LAPKA_KEY_ENTER) { d->done = 1; return true; }
    if (ev->key.code == LAPKA_KEY_ESC)   { d->done = 1; return true; }
    if (ev->key.code == LAPKA_KEY_CODEPOINT && ev->key.ch == 0x03) {
        d->done = 1;
        return true;
    }
    return false;
}

int cmd_msg(int argc, char **argv) {
    if (argc < 2) return 3;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0 && argv[i][2] != '\0') return 3;
    }

    MsgData d = { argv[0], argv[1], 0 };

    LapkaInitOptions opts = { 1, 0, 0, 0 };
    LapkaRunCallbacks cb = { msg_draw, msg_handle, NULL, &d };

    return lapka_run(&opts, &cb, -1);
}