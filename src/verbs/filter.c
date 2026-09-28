/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "filter.h"

#include <lapka/lapka.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    const char     *title;
    const char     *body;
    const char     *placeholder;
    bool            multi;
    LapkaPrimitive *text;
    LapkaPrimitive *list;
    int             exit_code;
} FilterData;

static void filter_draw(void *ud, LapkaFramebuffer *fb) {
    FilterData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char title_buf[512];
    int n = snprintf(title_buf, sizeof(title_buf), "-- %s --", d->title);
    if (n < 0 || n >= (int)sizeof(title_buf)) return;

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, title_buf, theme->title, 0, LAPKA_ATTR_BOLD);

    LapkaRect body_r = { 0, 2, fb->w, fb->h > 6 ? fb->h - 6 : 0 };
    int body_lines = lapka_draw_text(fb, body_r, d->body, 0, 0, 0);

    int text_y = 2 + body_lines + 1;
    if (text_y < fb->h - 3) {
        LapkaCell *sub = fb->cur + (size_t)text_y * fb->w;
        d->text->draw(d->text, sub, fb->w, 1, theme);
    }

    int list_y = text_y + 2;
    if (list_y > fb->h - 1) list_y = fb->h - 1;
    int list_h = fb->h - list_y - 1;
    if (list_h < 0) list_h = 0;
    if (list_h > 0) {
        LapkaCell *sub = fb->cur + (size_t)list_y * fb->w;
        d->list->draw(d->list, sub, fb->w, list_h, theme);
    }

    static const LapkaHintItem HINT_ONE[] = {
        { "\xe2\x86\x91\xe2\x86\x93", "move" },
        { "enter", "select" },
        { "esc",   "cancel" },
    };
    static const LapkaHintItem HINT_MULTI[] = {
        { "\xe2\x86\x91\xe2\x86\x93", "move" },
        { "tab",   "toggle" },
        { "enter", "confirm" },
        { "esc",   "cancel" },
    };

    if (d->multi) {
        lapka_draw_hint(fb, fb->h - 1, HINT_MULTI,
                        (int)(sizeof(HINT_MULTI) / sizeof(HINT_MULTI[0])),
                        theme->muted);
    } else {
        lapka_draw_hint(fb, fb->h - 1, HINT_ONE,
                        (int)(sizeof(HINT_ONE) / sizeof(HINT_ONE[0])),
                        theme->muted);
    }
}

static bool filter_handle(void *ud, const LapkaEvent *ev) {
    FilterData *d = ud;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ESC) {
            d->exit_code = 1;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_CODEPOINT &&
            ev->key.ch == 0x03 && (ev->key.mods & LAPKA_MOD_CTRL)) {
            d->exit_code = 1;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_ENTER) {
            d->exit_code = 0;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_UP ||
            ev->key.code == LAPKA_KEY_DOWN ||
            ev->key.code == LAPKA_KEY_PAGEUP ||
            ev->key.code == LAPKA_KEY_PAGEDOWN) {
            d->list->handle(d->list, ev);
            return false;
        }
        if (ev->key.code == LAPKA_KEY_TAB && d->multi) {
            d->list->handle(d->list, ev);
            return false;
        }
    }

    if (ev->type == LAPKA_EVENT_MOUSE) {
        d->list->handle(d->list, ev);
        return false;
    }

    if (d->text->handle(d->text, ev)) {
        lapka_list_refilter(d->list);
        return false;
    }

    return false;
}

static char **read_items_from_stdin(int *count_out) {
    int cap = 64;
    int count = 0;
    char **items = malloc((size_t)cap * sizeof(char *));
    if (!items) { *count_out = 0; return NULL; }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t n;

    while ((n = getline(&line, &linecap, stdin)) > 0) {
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';

        if (count >= cap) {
            int ncap = cap * 2;
            char **ni = realloc(items, (size_t)ncap * sizeof(char *));
            if (!ni) break;
            items = ni;
            cap = ncap;
        }
        items[count++] = strdup(line);
    }
    free(line);

    *count_out = count;
    return items;
}

int cmd_filter(int argc, char **argv) {
    const char *title = NULL;
    const char *body = NULL;
    const char *placeholder = "";
    bool multi = false;

    int pos = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--multi") == 0 || strcmp(a, "--no-limit") == 0) {
            multi = true;
            continue;
        }
        if (strcmp(a, "--placeholder") == 0 || strcmp(a, "-p") == 0) {
            if (i + 1 >= argc) return 3;
            placeholder = argv[++i];
            continue;
        }
        if (strcmp(a, "--height") == 0) {
            if (i + 1 >= argc) return 3;
            (void)atoi(argv[++i]);
            continue;
        }
        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') return 3;
        if (pos == 0) title = a;
        else if (pos == 1) body = a;
        else return 3;
        pos++;
    }

    if (!title || !body) return 3;

    if (isatty(STDIN_FILENO)) {
        fprintf(stderr, "tui: filter requires items on stdin\n");
        return 3;
    }

    int item_count = 0;
    char **items = read_items_from_stdin(&item_count);

    FilterData d;
    memset(&d, 0, sizeof(d));
    d.title = title;
    d.body = body;
    d.placeholder = placeholder;
    d.multi = multi;

    d.text = lapka_text_new("", false);
    if (!d.text) { 
        for (int i = 0; i < item_count; i++) free(items[i]);
        free(items);
        return 2;
    }
    lapka_text_set_prefix(d.text, "> ");

    d.list = lapka_list_new((const char *const *)items,
                            item_count,
                            multi ? LAPKA_SELECT_MANY : LAPKA_SELECT_ONE);
    if (!d.list) {
        lapka_primitive_free(d.text);
        for (int i = 0; i < item_count; i++) free(items[i]);
        free(items);
        return 2;
    }

    for (int i = 0; i < item_count; i++) free(items[i]);
    free(items);

    lapka_list_attach_filter(d.list, d.text);
    lapka_list_set_matcher(d.list, lapka_filter_fuzzy);
    lapka_list_refilter(d.list);

    LapkaInitOptions opts = { 1, 1, 1, 0 };
    LapkaRunCallbacks cb = { filter_draw, filter_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    int code = (rc != 0) ? rc : d.exit_code;

    if (code == 0) {
        if (multi) {
            int n = lapka_list_result_count(d.list);
            for (int i = 0; i < n; i++) {
                const char *s = lapka_list_result(d.list, i);
                if (s) printf("%s\n", s);
            }
        } else {
            const char *s = lapka_list_cursor_item(d.list);
            if (s) printf("%s\n", s);
        }
        fflush(stdout);
    }

    lapka_primitive_free(d.list);
    lapka_primitive_free(d.text);
    return code;
}