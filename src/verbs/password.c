/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "password.h"

#include <lapka/lapka.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *title;
    const char *body;
    LapkaPrimitive *text;
    int   done;
    int   cancelled;
    char *value;
} PwData;

static void pw_draw(void *ud, LapkaFramebuffer *fb) {
    PwData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    LapkaRect body_r = { 0, 2, fb->w, fb->h > 4 ? fb->h - 4 : 0 };
    int body_lines = lapka_draw_text(fb, body_r, d->body, 0, 0, 0);

    int y = 2 + body_lines + 1;
    if (y < fb->h - 1) {
        LapkaCell *sub = fb->cur + (size_t)y * fb->w;
        d->text->draw(d->text, sub, fb->w, 1, theme);
    }

    LapkaRect hint_r = { 0, fb->h - 1, fb->w, 1 };
    lapka_draw_text(fb, hint_r, "enter submit  esc cancel", theme->muted, 0, 0);
}

static bool pw_handle(void *ud, const LapkaEvent *ev) {
    PwData *d = ud;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER) {
            const char *v = lapka_text_value(d->text);
            if (v && v[0]) { d->value = strdup(v); d->done = 1; return true; }
            return false;
        }
        if (ev->key.code == LAPKA_KEY_ESC) { d->cancelled = 1; return true; }
        if (ev->key.code == LAPKA_KEY_CODEPOINT && ev->key.ch == 0x03) {
            d->cancelled = 1;
            return true;
        }
    }

    d->text->handle(d->text, ev);
    return false;
}

int cmd_password(int argc, char **argv) {
    if (argc < 2) return 3;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0 && argv[i][2] != '\0') return 3;
    }

    PwData d = { argv[0], argv[1], NULL, 0, 0, NULL };
    d.text = lapka_text_new("", true);
    if (!d.text) return 2;
    lapka_text_set_prefix(d.text, "> ");

    LapkaInitOptions opts = { 1, 0, 1, 0 };
    LapkaRunCallbacks cb = { pw_draw, pw_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && d.done) {
        if (isatty(STDOUT_FILENO)) {
            fputc('\n', stderr);
            fprintf(stderr, "tui: password requires stdout to be redirected\n");
            free(d.value);
            lapka_primitive_free(d.text);
            return 2;
        }
        printf("%s\n", d.value);
        fflush(stdout);
    }

    free(d.value);
    lapka_primitive_free(d.text);
    if (rc != 0) return rc;
    if (d.cancelled) return 1;
    return 0;
}