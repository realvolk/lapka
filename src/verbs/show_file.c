/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "show_file.h"

#include <lapka/lapka.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char     *title;
    LapkaPrimitive *pager;
} ShowFileData;

static void show_draw(void *ud, LapkaFramebuffer *fb) {
    ShowFileData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    if (fb->h > 1) {
        LapkaCell *sub = fb->cur + (size_t)fb->w;
        d->pager->draw(d->pager, sub, fb->w, fb->h - 1, theme);
    }
}

static bool show_handle(void *ud, const LapkaEvent *ev) {
    ShowFileData *d = ud;

    if (d->pager->handle(d->pager, ev)) return false;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ESC) return true;
        if (ev->key.code == LAPKA_KEY_CODEPOINT) {
            if (ev->key.ch == 'q' || ev->key.ch == 'Q') return true;
            if (ev->key.ch == 0x03) return true;
        }
    }
    return false;
}

int cmd_show_file(int argc, char **argv) {
    if (argc < 2) return 3;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0 && argv[i][2] != '\0') return 3;
    }

    const char *title = argv[0];
    const char *path = argv[1];

    ShowFileData d;
    memset(&d, 0, sizeof(d));
    d.title = title;

    d.pager = lapka_pager_new();
    if (!d.pager) return 2;

    if (lapka_pager_set_file(d.pager, path) != 0) {
        fprintf(stderr, "tui: show-file: cannot read %s\n", path);
        lapka_primitive_free(d.pager);
        return 2;
    }

    lapka_pager_set_hint(d.pager,
        "/ find  n/N next/prev  \xe2\x86\x91\xe2\x86\x93 scroll  "
        "pgup/pgdn page  home/end  q quit");

    LapkaInitOptions opts = { 1, 1, 0, 0 };
    LapkaRunCallbacks cb = { show_draw, show_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    lapka_primitive_free(d.pager);
    return rc != 0 ? rc : 0;
}