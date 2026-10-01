/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "draw.h"
#include "term/ansi.h"
#include "core/cell.h"
#include "core/rect.h"
#include "core/utf8.h"

#include <unistd.h>

int lapka_draw_fill(LapkaFramebuffer *fb, LapkaRect r, LapkaCell c) {
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            lapka_fb_put(fb, x, y, c);
        }
    }
    return 0;
}

int lapka_draw_hline(LapkaFramebuffer *fb, int x, int y, int w, uint32_t cp,
                     uint32_t fg, uint32_t bg) {
    for (int i = 0; i < w; i++) {
        lapka_fb_put(fb, x + i, y, lapka_cell_make(cp, fg, bg, 0));
    }
    return 0;
}

int lapka_draw_text(LapkaFramebuffer *fb, LapkaRect r, const char *text,
                    uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (lapka_rect_empty(r) || !text) return 0;

    const uint8_t *p = (const uint8_t *)text;
    int cx = r.x;
    int cy = r.y;

    while (*p && cy < r.y + r.h) {
        if (*p == '\n') {
            p++;
            cx = r.x;
            cy++;
            continue;
        }

        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        int width = lapka_cp_width(cp);
        p += bytes;

        if (width == 0) continue;

        if (cx + width > r.x + r.w) {
            cx = r.x;
            cy++;
            if (cy >= r.y + r.h) break;
        }

        lapka_fb_put(fb, cx, cy, lapka_cell_make(cp, fg, bg, attrs));
        cx += width;
    }

    return cy - r.y + 1;
}

int lapka_draw_box(LapkaFramebuffer *fb, LapkaRect r, LapkaBorder border,
                   uint32_t fg, uint32_t bg) {
    if (border == LAPKA_BORDER_NONE) return 0;
    if (r.w < 2 || r.h < 2) return 0;

    uint32_t hz, vt, tl, tr, bl, br;
    switch (border) {
        case LAPKA_BORDER_SINGLE:
            hz = 0x2500; vt = 0x2502;
            tl = 0x250C; tr = 0x2510;
            bl = 0x2514; br = 0x2518;
            break;
        case LAPKA_BORDER_ROUNDED:
            hz = 0x2500; vt = 0x2502;
            tl = 0x256D; tr = 0x256E;
            bl = 0x2570; br = 0x256F;
            break;
        case LAPKA_BORDER_DOUBLE:
            hz = 0x2550; vt = 0x2551;
            tl = 0x2554; tr = 0x2557;
            bl = 0x255A; br = 0x255D;
            break;
        default:
            return 0;
    }

    int x0 = r.x, y0 = r.y;
    int x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;

    lapka_fb_put(fb, x0, y0, lapka_cell_make(tl, fg, bg, 0));
    lapka_fb_put(fb, x1, y0, lapka_cell_make(tr, fg, bg, 0));
    lapka_fb_put(fb, x0, y1, lapka_cell_make(bl, fg, bg, 0));
    lapka_fb_put(fb, x1, y1, lapka_cell_make(br, fg, bg, 0));

    for (int x = x0 + 1; x < x1; x++) {
        lapka_fb_put(fb, x, y0, lapka_cell_make(hz, fg, bg, 0));
        lapka_fb_put(fb, x, y1, lapka_cell_make(hz, fg, bg, 0));
    }
    for (int y = y0 + 1; y < y1; y++) {
        lapka_fb_put(fb, x0, y, lapka_cell_make(vt, fg, bg, 0));
        lapka_fb_put(fb, x1, y, lapka_cell_make(vt, fg, bg, 0));
    }
    return 0;
}

int lapka_print_text(int fd, const char *text, int width,
                     uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (!text || width <= 0) return 0;

    char sgr[64];
    size_t sgr_n = lapka_ansi_sgr_build(sgr, sizeof(sgr), fg, bg, attrs);
    if (sgr_n > 0) {
        if (lapka_write_all(fd, sgr, sgr_n) != 0) return -1;
    }

    const uint8_t *p = (const uint8_t *)text;
    int col = 0;

    while (*p) {
        if (*p == '\n') {
            if (lapka_write_all(fd, "\r\n", 2) != 0) return -1;
            col = 0;
            p++;
            continue;
        }

        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        int cw = lapka_cp_width(cp);
        p += bytes;

        if (cw == 0) continue;

        if (col + cw > width) {
            if (lapka_write_all(fd, "\r\n", 2) != 0) return -1;
            col = 0;
        }

        uint8_t enc[4];
        int en = lapka_utf8_encode(cp, enc);
        if (lapka_write_all(fd, (const char *)enc, (size_t)en) != 0) return -1;
        col += cw;
    }

    if (lapka_write_all(fd, "\033[0m", 4) != 0) return -1;
    return 0;
}

int lapka_draw_hint(LapkaFramebuffer *fb, int y, const LapkaHintItem *items, int n,
                    uint32_t fg) {
    if (!fb || !items || n <= 0) return 0;
    if (y < 0 || y >= fb->h) return 0;

    int x = 0;
    for (int i = 0; i < n; i++) {
        if (x >= fb->w) break;
        const char *key = items[i].key ? items[i].key : "";
        const char *label = items[i].label ? items[i].label : "";

        int key_w = 0;
        {
            const uint8_t *p = (const uint8_t *)key;
            while (*p) {
                uint32_t cp;
                int b = lapka_utf8_decode(p, 4, &cp);
                if (b <= 0) break;
                p += b;
                key_w += lapka_cp_width(cp);
            }
        }
        int label_w = 0;
        {
            const uint8_t *p = (const uint8_t *)label;
            while (*p) {
                uint32_t cp;
                int b = lapka_utf8_decode(p, 4, &cp);
                if (b <= 0) break;
                p += b;
                label_w += lapka_cp_width(cp);
            }
        }

        int needed = key_w + 1 + label_w;
        if (i > 0) needed += 2;
        if (x + needed > fb->w) {
            needed = fb->w - x;
            if (needed <= 0) break;
        }

        if (i > 0) {
            lapka_fb_put(fb, x, y, lapka_cell_make(' ', 0, 0, 0));
            x++;
            lapka_fb_put(fb, x, y, lapka_cell_make(' ', 0, 0, 0));
            x++;
        }

        {
            const uint8_t *p = (const uint8_t *)key;
            while (*p) {
                uint32_t cp;
                int b = lapka_utf8_decode(p, 4, &cp);
                if (b <= 0) break;
                p += b;
                int cw = lapka_cp_width(cp);
                for (int k = 0; k < cw; k++) {
                    lapka_fb_put(fb, x + k, y, lapka_cell_make(k == 0 ? cp : 0, fg, 0, LAPKA_ATTR_BOLD));
                }
                x += cw;
            }
        }
        if (x < fb->w) {
            lapka_fb_put(fb, x, y, lapka_cell_make(' ', 0, 0, 0));
            x++;
        }
        {
            const uint8_t *p = (const uint8_t *)label;
            while (*p) {
                uint32_t cp;
                int b = lapka_utf8_decode(p, 4, &cp);
                if (b <= 0) break;
                p += b;
                int cw = lapka_cp_width(cp);
                for (int k = 0; k < cw; k++) {
                    lapka_fb_put(fb, x + k, y, lapka_cell_make(k == 0 ? cp : 0, fg, 0, 0));
                }
                x += cw;
            }
        }
    }
    return 0;
}

int lapka_put_cp(LapkaCell *buf, int w, int h, int x, int y, uint32_t cp,
                 uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    LapkaCell c = { cp, fg, bg, attrs };
    buf[(size_t)y * (size_t)w + (size_t)x] = c;
    return 0;
}

int lapka_draw_str(LapkaCell *buf, int w, int h, int x, int y,
                   const char *s, uint32_t fg, uint32_t bg, uint16_t attrs,
                   int max_cols) {
    if (!s) return 0;
    const uint8_t *p = (const uint8_t *)s;
    int cx = x;
    int used = 0;
    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        int cw = lapka_cp_width(cp);
        if (cw == 0) continue;
        if (max_cols >= 0 && used + cw > max_cols) break;
        for (int i = 0; i < cw; i++) {
            lapka_put_cp(buf, w, h, cx + i, y, i == 0 ? cp : 0, fg, bg, attrs);
        }
        cx += cw;
        used += cw;
    }
    return used;
}