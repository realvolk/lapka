/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "list_verb.h"

#include <lapka/lapka.h>
#include <lapka/theme.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const ListVerbArgs *args;
    LapkaPrimitive     *list;
    ListResult          on_result;
    void               *result_ud;
    int                 done;
    int                 cancelled;
} ListVerbData;

static void lv_draw(void *ud, LapkaFramebuffer *fb) {
    ListVerbData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->args->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    LapkaRect body_r = { 0, 2, fb->w, fb->h > 2 ? fb->h - 2 : 0 };
    int body_lines = lapka_draw_text(fb, body_r, d->args->body, 0, 0, 0);

    int hint_h = (d->args->hint && d->args->hint_count > 0) ? 1 : 0;
    int list_y = 2 + body_lines + 1;
    if (list_y > fb->h - 1 - hint_h) list_y = fb->h - 1 - hint_h;
    if (list_y < 0) list_y = 0;
    int list_h = fb->h - list_y - hint_h;
    if (list_h < 0) list_h = 0;

    if (list_h > 0) {
        LapkaCell *sub = fb->cur + (size_t)list_y * fb->w;
        d->list->draw(d->list, sub, fb->w, list_h, theme);
    }

    if (hint_h) {
        lapka_draw_hint(fb, fb->h - 1, d->args->hint, d->args->hint_count,
                        theme->muted);
    }
}

static bool lv_handle(void *ud, const LapkaEvent *ev) {
    ListVerbData *d = ud;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER) { d->done = 1; return true; }
        if (ev->key.code == LAPKA_KEY_ESC)   { d->cancelled = 1; return true; }
        if (ev->key.code == LAPKA_KEY_CODEPOINT && ev->key.ch == 0x03) {
            d->cancelled = 1;
            return true;
        }
    }

    d->list->handle(d->list, ev);
    return false;
}

int lapka_parse_list_args(int argc, char **argv, LapkaSelectMode mode,
                          ListVerbArgs *out) {
    (void)mode;
    out->title = NULL;
    out->body = NULL;
    out->items = NULL;
    out->item_count = 0;
    out->hint = NULL;
    out->hint_count = 0;

    int pos = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--") == 0) {
            for (int j = i + 1; j < argc; j++) {
                if (pos == 0) out->title = argv[j];
                else if (pos == 1) out->body = argv[j];
                else {
                    if (!out->items) out->items = (const char *const *)&argv[j];
                    out->item_count++;
                }
                pos++;
            }
            break;
        }
        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') return 3;
        if (pos == 0) out->title = a;
        else if (pos == 1) out->body = a;
        else {
            if (!out->items) out->items = (const char *const *)&argv[i];
            out->item_count++;
        }
        pos++;
    }

    if (!out->title || !out->body || out->item_count == 0) return 3;
    return 0;
}

int lapka_run_list_verb(const ListVerbArgs *args,
                        ListResult on_result, void *ud) {
    ListVerbData d = { args, NULL, on_result, ud, 0, 0 };
    d.list = lapka_list_new(args->items, args->item_count, args->mode);
    if (!d.list) return 2;

    LapkaInitOptions opts = { 1, 1, 1, 0 };
    LapkaRunCallbacks cb = { lv_draw, lv_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && d.done && d.on_result) {
        d.on_result(d.list, d.result_ud);
    }

    lapka_primitive_free(d.list);

    if (rc != 0) return rc;
    if (d.cancelled) return 1;
    return 0;
}