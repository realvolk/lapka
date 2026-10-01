/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "flush.h"
#include "term/capabilities.h"
#include "term/ansi.h"
#include "core/cell.h"
#include "core/utf8.h"
#include "util/buf.h"

#include <string.h>

static int emit_cursor(LapkaBuf *out, int x, int y) {
    char tmp[32];
    int n = 0;
    tmp[n++] = 0x1B;
    tmp[n++] = '[';
    int ny = y + 1;
    int nx = x + 1;
    char digits[12];
    int dn = 0;
    if (ny == 0) digits[dn++] = '0';
    while (ny > 0) { digits[dn++] = (char)('0' + ny % 10); ny /= 10; }
    while (dn > 0) tmp[n++] = digits[--dn];
    tmp[n++] = ';';
    dn = 0;
    if (nx == 0) digits[dn++] = '0';
    while (nx > 0) { digits[dn++] = (char)('0' + nx % 10); nx /= 10; }
    while (dn > 0) tmp[n++] = digits[--dn];
    tmp[n++] = 'H';
    return lapka_buf_append(out, tmp, (size_t)n);
}

static int emit_codepoint(LapkaBuf *out, uint32_t cp) {
    if (cp == 0 || cp == 0x09) cp = ' ';
    uint8_t enc[4];
    int n = lapka_utf8_encode(cp, enc);
    return lapka_buf_append(out, (const char *)enc, (size_t)n);
}

static int needs_sgr(LapkaCell a, LapkaCell b) {
    return a.fg != b.fg || a.bg != b.bg || a.attrs != b.attrs;
}

int lapka_flush(LapkaFramebuffer *fb, int tty_fd, int color_depth) {
    (void)color_depth;

    LapkaBuf out;
    lapka_buf_init(&out);

    bool sync = lapka_cap_sync_supported();
    if (sync) {
        if (lapka_buf_append_str(&out, "\033[?2026h") != 0) goto fail;
    }

    LapkaCell last;
    memset(&last, 0, sizeof(last));
    int have_last = 0;
    int last_x = -1;
    int last_y = -1;

    for (int y = 0; y < fb->h; y++) {
        int prev_was_wide = 0;

        for (int x = 0; x < fb->w; x++) {
            size_t idx = (size_t)y * (size_t)fb->w + (size_t)x;
            LapkaCell c = fb->cur[idx];
            LapkaCell p = fb->prev[idx];

            if (prev_was_wide) {
                prev_was_wide = 0;
                continue;
            }

            if (!fb->full_dirty && lapka_cell_equal(c, p)) {
                if (lapka_cp_width(c.cp) == 2) {
                    prev_was_wide = 1;
                }
                continue;
            }

            if (x != last_x + 1 || y != last_y) {
                if (emit_cursor(&out, x, y) != 0) goto fail;
            }

            if (!have_last || needs_sgr(c, last)) {
                char sgr[64];
                size_t n = lapka_ansi_sgr_build(sgr, sizeof(sgr), c.fg, c.bg, c.attrs);
                if (n == 0) goto fail;
                if (lapka_buf_append(&out, sgr, n) != 0) goto fail;
                last = c;
                have_last = 1;
            }

            if (emit_codepoint(&out, c.cp) != 0) goto fail;

            if (lapka_cp_width(c.cp) == 2) {
                prev_was_wide = 1;
                last_x = x + 1;
                last_y = y;
            } else {
                last_x = x;
                last_y = y;
            }
        }
    }

    if (have_last) {
        if (lapka_buf_append_str(&out, "\033[0m") != 0) goto fail;
    }

    if (sync) {
        if (lapka_buf_append_str(&out, "\033[?2026l") != 0) goto fail;
    }

    if (lapka_write_all(tty_fd, out.data ? out.data : "", out.len) != 0) {
        goto fail;
    }

    memcpy(fb->prev, fb->cur, (size_t)fb->w * (size_t)fb->h * sizeof(LapkaCell));
    fb->full_dirty = false;

    lapka_buf_free(&out);
    return 0;

fail:
    lapka_buf_free(&out);
    return -1;
}