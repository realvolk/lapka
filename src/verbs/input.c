/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "input.h"

#include <lapka/lapka.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *title;
    const char *body;
    LapkaPrimitive *text;
    int   done;
    int   cancelled;
    char *value;
} InputData;

static void input_draw(void *ud, LapkaFramebuffer *fb) {
    InputData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    LapkaRect body_r = { 0, 2, fb->w, fb->h > 4 ? fb->h - 4 : 0 };
    int body_lines = lapka_draw_text(fb, body_r, d->body, 0, 0, 0);

    int y = 2 + body_lines + 1;
    LapkaRect text_r = { 0, y, fb->w, 1 };
    if (text_r.y < fb->h - 1) {
        LapkaCell *sub = fb->cur + (size_t)text_r.y * fb->w + text_r.x;
        d->text->draw(d->text, sub, text_r.w, 1, theme);
    }

    LapkaRect hint_r = { 0, fb->h - 1, fb->w, 1 };
    lapka_draw_text(fb, hint_r, "enter submit  esc cancel", theme->muted, 0, 0);
}

static bool input_handle(void *ud, const LapkaEvent *ev) {
    InputData *d = ud;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER) {
            const char *v = lapka_text_value(d->text);
            d->value = strdup(v ? v : "");
            d->done = 1;
            return true;
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

int cmd_input(int argc, char **argv) {
    const char *title = NULL;
    const char *body = NULL;
    const char *default_value = "";

    int pos = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--default") == 0 || strcmp(a, "-d") == 0) {
            if (i + 1 >= argc) return 3;
            default_value = argv[++i];
            continue;
        }
        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') return 3;
        if (pos == 0) title = a;
        else if (pos == 1) body = a;
        else return 3;
        pos++;
    }
    if (!title || !body) return 3;

    InputData d = { title, body, NULL, 0, 0, NULL };
    d.text = lapka_text_new(default_value, false);
    if (!d.text) return 2;
    lapka_text_set_prefix(d.text, "> ");

    LapkaInitOptions opts = { 1, 0, 1, 0 };
    LapkaRunCallbacks cb = { input_draw, input_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && d.done && d.value) {
        printf("%s\n", d.value);
        fflush(stdout);
    }

    free(d.value);
    lapka_list_free(d.text);
    if (rc != 0) return rc;
    if (d.cancelled) return 1;
    return 0;
}