/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "password_confirm.h"

#include <lapka/lapka.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char     *title;
    const char     *body;
    const char     *error;
    LapkaPrimitive *text;
    int             done;
    int             cancelled;
    char           *value;
} PwData;

static void pw_draw(void *ud, LapkaFramebuffer *fb) {
    PwData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    int y = 2;

    if (d->error && d->error[0]) {
        LapkaRect err_r = { 0, y, fb->w, 1 };
        lapka_draw_text(fb, err_r, d->error, theme->error, 0, LAPKA_ATTR_BOLD);
        y += 2;
    }

    LapkaRect body_r = { 0, y, fb->w, fb->h > y + 2 ? fb->h - y - 2 : 0 };
    int body_lines = lapka_draw_text(fb, body_r, d->body, 0, 0, 0);

    int text_y = y + body_lines + 1;
    if (text_y < fb->h - 1) {
        LapkaCell *sub = fb->cur + (size_t)text_y * fb->w;
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

static char *prompt_once(const char *title, const char *body, const char *error,
                         int *cancelled) {
    PwData d = { title, body, error, NULL, 0, 0, NULL };
    d.text = lapka_text_new("", true);
    if (!d.text) { *cancelled = 1; return NULL; }
    lapka_text_set_prefix(d.text, "> ");

    LapkaInitOptions opts = { 1, 0, 1, 0 };
    LapkaRunCallbacks cb = { pw_draw, pw_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    char *value = d.value;
    lapka_primitive_free(d.text);

    if (rc != 0 || d.cancelled) {
        free(value);
        *cancelled = 1;
        return NULL;
    }
    *cancelled = 0;
    return value;
}

int cmd_password_confirm(int argc, char **argv) {
    const char *title = NULL;
    const char *first_body = NULL;
    const char *confirm_prompt = NULL;
    int tries = 3;

    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--tries") == 0) {
            if (i + 1 >= argc) return 3;
            tries = atoi(argv[++i]);
            if (tries < 0) tries = 0;
            continue;
        }
        if (strcmp(a, "--confirm-prompt") == 0) {
            if (i + 1 >= argc) return 3;
            confirm_prompt = argv[++i];
            continue;
        }
        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') return 3;
        if (!title) title = a;
        else if (!first_body) first_body = a;
        else return 3;
    }

    if (!title || !first_body) return 3;
    if (!confirm_prompt) confirm_prompt = first_body;

    if (isatty(STDOUT_FILENO)) {
        fprintf(stderr, "tui: password-confirm requires stdout to be redirected\n");
        return 2;
    }

    int attempt = 0;
    const char *error = NULL;

    for (;;) {
        int cancelled = 0;

        char *pass = prompt_once(title, first_body, error, &cancelled);
        if (cancelled) return 1;

        char *confirm = prompt_once(title, confirm_prompt, NULL, &cancelled);
        if (cancelled) { free(pass); return 1; }

        if (strcmp(pass, confirm) == 0) {
            printf("%s\n", pass);
            fflush(stdout);
            free(pass);
            free(confirm);
            return 0;
        }

        free(pass);
        free(confirm);

        attempt++;
        if (tries != 0 && attempt >= tries) {
            return 1;
        }

        error = "passwords do not match";
    }
}