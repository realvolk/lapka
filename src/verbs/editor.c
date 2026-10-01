/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "editor.h"

#include <lapka/lapka.h>
#include <lapka/layer.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const char     *title;
    LapkaPrimitive *editor;
    int             exit_code;
    int             cursor_visible;
    long long       last_blink_ms;
    int             readonly;
    int             esc_pending;

    LapkaPrimitive *toast;
    LapkaLayer     *toast_layer;
    long long       toast_start_ms;
    int             toast_duration_ms;
} EditorVerbData;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int process_is_root(void) {
    if (geteuid() == 0) return 1;
    if (getuid() == 0) return 1;
    return 0;
}

static int file_is_writable(const char *path) {
    if (!path) return 1;
    if (process_is_root()) return 1;
    if (access(path, F_OK) != 0) return 1;
    return access(path, W_OK) == 0;
}

static void show_toast(EditorVerbData *d, const char *msg, int ms) {
    if (d->toast) {
        lapka_primitive_free(d->toast);
        d->toast = NULL;
    }
    d->toast = lapka_notification_new(msg, ms);
    d->toast_start_ms = now_ms();
    d->toast_duration_ms = ms;
}

static bool toast_expired(EditorVerbData *d) {
    if (!d->toast) return false;
    return (now_ms() - d->toast_start_ms) >= d->toast_duration_ms;
}

static bool tick(void *ud) {
    EditorVerbData *d = ud;

    long long now = now_ms();
    if (now - d->last_blink_ms >= 500) {
        d->last_blink_ms = now;
        d->cursor_visible = !d->cursor_visible;
        lapka_editor_set_cursor_visible(d->editor, d->cursor_visible);
    }

    if (d->toast && toast_expired(d)) {
        lapka_primitive_free(d->toast);
        d->toast = NULL;
        if (d->toast_layer) {
            lapka_layer_free(d->toast_layer);
            d->toast_layer = NULL;
        }
    }

    return false;
}

static void draw(void *ud, LapkaFramebuffer *fb) {
    EditorVerbData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    char buf[512];
    const char *path = lapka_editor_path(d->editor);
    int n = snprintf(buf, sizeof(buf), "-- %s -- %s%s%s",
                     d->title,
                     path ? path : "(no file)",
                     lapka_editor_is_dirty(d->editor) ? " *" : "",
                     d->readonly ? "  [read-only]" : "");
    if (n < 0 || n >= (int)sizeof(buf)) return;

    LapkaRect tr = { 0, 0, fb->w, 1 };
    uint32_t title_fg = d->readonly ? theme->error : theme->title;
    lapka_draw_text(fb, tr, buf, title_fg, 0, LAPKA_ATTR_BOLD);

    if (fb->h > 1) {
        LapkaCell *sub = fb->cur + (size_t)fb->w;
        d->editor->draw(d->editor, sub, fb->w, fb->h - 1, theme);
    }

    if (d->toast) {
        int pw = 0, ph = 0;
        d->toast->measure(d->toast, fb->w - 4, fb->h - 4, &pw, &ph);
        if (pw < 20) pw = 20;
        if (pw > fb->w - 4) pw = fb->w - 4;

        int x = fb->w - pw - 2;
        int y = fb->h - ph - 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;

        LapkaRect want = { x, y, pw, ph };
        if (!d->toast_layer) {
            d->toast_layer = lapka_layer_new(want, 100,
                                             LAPKA_BLEND_TRANSPARENT,
                                             LAPKA_INPUT_NONE);
        } else if (want.w != d->toast_layer->rect.w ||
                   want.h != d->toast_layer->rect.h ||
                   want.x != d->toast_layer->rect.x ||
                   want.y != d->toast_layer->rect.y) {
            lapka_layer_resize(d->toast_layer, want);
        }

        if (d->toast_layer) {
            int tw = d->toast_layer->rect.w;
            int th = d->toast_layer->rect.h;
            for (int yy = 0; yy < th; yy++) {
                for (int xx = 0; xx < tw; xx++) {
                    d->toast_layer->buf[(size_t)yy * (size_t)tw + (size_t)xx] =
                        (LapkaCell){ 0, 0, 0, 0 };
                }
            }
            d->toast->draw(d->toast, d->toast_layer->buf, tw, th, theme);
            LapkaLayer *layers[1] = { d->toast_layer };
            lapka_composite(fb, layers, 1);
        }
    }
}

static bool handle(void *ud, const LapkaEvent *ev) {
    EditorVerbData *d = ud;

    if (d->toast) {
        if (ev->type == LAPKA_EVENT_KEY) {
            lapka_primitive_free(d->toast);
            d->toast = NULL;
            if (d->toast_layer) {
                lapka_layer_free(d->toast_layer);
                d->toast_layer = NULL;
            }
        }
    }

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ESC) {
            if (lapka_editor_is_dirty(d->editor) && !d->readonly && !d->esc_pending) {
                d->esc_pending = 1;
                show_toast(d, "Unsaved changes. Ctrl-S to save, Esc again to discard.", 3000);
                return false;
            }
            d->exit_code = 0;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_CODEPOINT && ev->key.ch == 0x03) {
            d->exit_code = 1;
            return true;
        }
        if (d->readonly && ev->key.code == LAPKA_KEY_CODEPOINT &&
            ev->key.ch == 0x13) {
            show_toast(d, "Read-only: cannot save.", 2000);
            return false;
        }
        if (!d->readonly && ev->key.code == LAPKA_KEY_CODEPOINT &&
            ev->key.ch == 0x13) {
            int rc = lapka_editor_save(d->editor);
            d->esc_pending = 0;
            if (rc != 0) show_toast(d, "Save failed.", 2000);
            else show_toast(d, "Saved.", 1500);
            return false;
        }
        if (ev->key.code != LAPKA_KEY_ESC) {
            d->esc_pending = 0;
        }
    }

    d->editor->handle(d->editor, ev);
    return false;
}

int cmd_editor(int argc, char **argv) {
    const char *title = "Editor";
    const char *path = NULL;
    bool new_file = false;

    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--new") == 0) { new_file = true; continue; }
        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') {
            fprintf(stderr, "tui: editor: unknown flag: %s\n", a);
            return 3;
        }
        if (!path) path = a;
        else title = a;
    }

    if (!path && !new_file) {
        fprintf(stderr, "tui: editor: FILE required (or --new)\n");
        return 3;
    }

    EditorVerbData d;
    memset(&d, 0, sizeof(d));
    d.title = title;
    d.editor = lapka_editor_new();
    if (!d.editor) return 2;

    d.cursor_visible = 1;
    d.last_blink_ms = now_ms();
    lapka_editor_set_cursor_visible(d.editor, true);

    lapka_editor_set_hint(d.editor,
        "^S save  ^Z undo  ^Y redo  ^D delete-line  esc done");

    if (path) {
        lapka_editor_open(d.editor, path);
    }

    if (path && !file_is_writable(path)) {
        d.readonly = 1;
    }

    d.esc_pending = 0;

    LapkaInitOptions opts = { 1, 0, 1, 0 };
    LapkaRunCallbacks cb = { draw, handle, tick, &d };

    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && d.exit_code == 0 && !path) {
        char *text = lapka_editor_text(d.editor);
        if (text) {
            fputs(text, stdout);
            fflush(stdout);
            free(text);
        }
    }

    if (d.toast) lapka_primitive_free(d.toast);
    if (d.toast_layer) lapka_layer_free(d.toast_layer);
    lapka_primitive_free(d.editor);
    return rc != 0 ? rc : d.exit_code;
}