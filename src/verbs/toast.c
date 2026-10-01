/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "toast.h"

#include <lapka/lapka.h>
#include <lapka/layer.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    LapkaPrimitive *notif;
    LapkaLayer     *layer;
    int             done;
} ToastData;

static void toast_place(ToastData *d, LapkaFramebuffer *fb,
                        const LapkaTheme *theme) {
    (void)theme;

    int pw = 0, ph = 0;
    d->notif->measure(d->notif, fb->w - 4, fb->h - 4, &pw, &ph);
    if (pw < 20) pw = 20;
    if (pw > fb->w - 4) pw = fb->w - 4;

    int x = fb->w - pw - 2;
    int y = fb->h - ph - 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    LapkaRect want = { x, y, pw, ph };
    if (!d->layer) {
        d->layer = lapka_layer_new(want, 100,
                                   LAPKA_BLEND_TRANSPARENT,
                                   LAPKA_INPUT_NONE);
    } else if (want.w != d->layer->rect.w || want.h != d->layer->rect.h ||
               want.x != d->layer->rect.x || want.y != d->layer->rect.y) {
        lapka_layer_resize(d->layer, want);
    }
}

static void toast_draw(void *ud, LapkaFramebuffer *fb) {
    ToastData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    toast_place(d, fb, theme);
    if (!d->layer) return;

    int w = d->layer->rect.w;
    int h = d->layer->rect.h;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            d->layer->buf[(size_t)y * (size_t)w + (size_t)x] = (LapkaCell){ 0, 0, 0, 0 };
        }
    }

    d->notif->draw(d->notif, d->layer->buf, w, h, theme);

    LapkaLayer *layers[1] = { d->layer };
    lapka_composite(fb, layers, 1);
}

static bool toast_tick(void *ud) {
    ToastData *d = ud;
    if (lapka_notification_expired(d->notif)) {
        d->done = 1;
        return true;
    }
    return false;
}

static bool toast_handle(void *ud, const LapkaEvent *ev) {
    ToastData *d = ud;
    if (ev->type == LAPKA_EVENT_KEY) {
        d->done = 1;
        return true;
    }
    return d->done;
}

int cmd_toast(int argc, char **argv) {
    if (argc < 1) return 3;

    const char *message = argv[0];
    int duration_ms = 3000;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strncmp(a, "--duration=", 11) == 0) {
            duration_ms = atoi(a + 11);
            continue;
        }
        if (strcmp(a, "--duration") == 0 && i + 1 < argc) {
            duration_ms = atoi(argv[++i]);
            continue;
        }
        fprintf(stderr, "tui: toast: unknown argument: %s\n", a);
        return 3;
    }

    ToastData d;
    memset(&d, 0, sizeof(d));
    d.notif = lapka_notification_new(message, duration_ms);
    if (!d.notif) return 2;

    LapkaInitOptions opts = { 0, 0, 0, 0 };
    LapkaRunCallbacks cb = { toast_draw, toast_handle, toast_tick, &d };

    int rc = lapka_run(&opts, &cb, -1);

    lapka_primitive_free(d.notif);
    if (d.layer) lapka_layer_free(d.layer);

    return rc == 0 ? 0 : 2;
}