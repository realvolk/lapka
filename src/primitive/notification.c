/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include <render/draw.h>
#include "core/utf8.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char      *message;
    long long  duration_ms;
    long long  start_ms;
} NotificationData;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void notif_draw(LapkaPrimitive *p, LapkaCell *buf, int w, int h,
                       const LapkaTheme *theme) {
    NotificationData *d = p->data;
    if (w <= 0 || h <= 0) return;
    if (lapka_notification_expired(p)) return;

    uint32_t bg = theme->modal_bg;
    uint32_t border = theme->modal_border;
    uint32_t fg = theme->title;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            lapka_put_cp(buf, w, h, x, y, ' ', 0, bg, 0);
        }
    }

    for (int x = 0; x < w; x++) {
        lapka_put_cp(buf, w, h, x, 0, 0x2500, border, bg, 0);
        lapka_put_cp(buf, w, h, x, h - 1, 0x2500, border, bg, 0);
    }
    for (int y = 0; y < h; y++) {
        lapka_put_cp(buf, w, h, 0, y, 0x2502, border, bg, 0);
        lapka_put_cp(buf, w, h, w - 1, y, 0x2502, border, bg, 0);
    }
    lapka_put_cp(buf, w, h, 0, 0, 0x250C, border, bg, 0);
    lapka_put_cp(buf, w, h, w - 1, 0, 0x2510, border, bg, 0);
    lapka_put_cp(buf, w, h, 0, h - 1, 0x2514, border, bg, 0);
    lapka_put_cp(buf, w, h, w - 1, h - 1, 0x2518, border, bg, 0);

    if (h > 2 && w > 4) {
        lapka_draw_str(buf, w, h, 2, 1, d->message, fg, bg, 0, w - 4);
    }
}

static void notif_measure(LapkaPrimitive *p, int bound_w, int bound_h,
                          int *out_w, int *out_h) {
    NotificationData *d = p->data;
    (void)bound_h;

    const uint8_t *q = (const uint8_t *)d->message;
    int len = 0;
    while (*q) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(q, 4, &cp);
        if (bytes <= 0) break;
        q += bytes;
        len += lapka_cp_width(cp);
    }

    int w = len + 6;
    if (w < 20) w = 20;
    if (w > bound_w) w = bound_w;
    *out_w = w;
    *out_h = 3;
}

static bool notif_handle(LapkaPrimitive *p, const LapkaEvent *ev) {
    (void)p;
    (void)ev;
    return false;
}

static void notif_free(LapkaPrimitive *p) {
    NotificationData *d = p->data;
    if (!d) { free(p); return; }
    free(d->message);
    free(d);
    free(p);
}

LapkaPrimitive *lapka_notification_new(const char *message, int duration_ms) {
    LapkaPrimitive *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    NotificationData *d = calloc(1, sizeof(*d));
    if (!d) { free(p); return NULL; }

    d->message = strdup(message ? message : "");
    d->duration_ms = duration_ms > 0 ? duration_ms : 3000;
    d->start_ms = now_ms();

    p->kind = LAPKA_PRIMITIVE_NOTIFICATION;
    p->data = d;
    p->handle = notif_handle;
    p->draw = notif_draw;
    p->measure = notif_measure;
    p->free = notif_free;
    return p;
}

void lapka_notification_set(LapkaPrimitive *p, const char *message) {
    if (!p || p->kind != LAPKA_PRIMITIVE_NOTIFICATION) return;
    NotificationData *d = p->data;
    free(d->message);
    d->message = strdup(message ? message : "");
    d->start_ms = now_ms();
}

bool lapka_notification_expired(LapkaPrimitive *p) {
    if (!p || p->kind != LAPKA_PRIMITIVE_NOTIFICATION) return true;
    NotificationData *d = p->data;
    return (now_ms() - d->start_ms) >= d->duration_ms;
}
