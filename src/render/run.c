/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "run.h"
#include "flush.h"
#include "framebuffer.h"
#include "draw.h"

#include <lapka/theme.h>
#include <lapka/types.h>

#include "../input/input.h"
#include "../term/term.h"
#include "../term/signals.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>

int lapka_run(const LapkaInitOptions *opts, const LapkaRunCallbacks *cb,
              int extra_fd) {
    if (!lapka_term_init(opts)) return 2;

    int w, h;
    lapka_term_get_size(&w, &h);

    LapkaFramebuffer fb;
    if (lapka_fb_init(&fb, w, h) != 0) {
        lapka_term_shutdown();
        return 2;
    }

    int result = 2;
    int in_fd = lapka_term_fd();

    for (;;) {
        int nw, nh;
        if (lapka_term_get_size(&nw, &nh) && (nw != fb.w || nh != fb.h)) {
            if (lapka_fb_resize(&fb, nw, nh) != 0) { result = 2; break; }
            lapka_fb_mark_full_dirty(&fb);
        }

        if (lapka_signals_take_resize()) {
            lapka_fb_mark_full_dirty(&fb);
        }

        lapka_fb_clear(&fb);

        if (fb.w < 20 || fb.h < 5) {
            LapkaRect r = { 0, 0, fb.w, 1 };
            lapka_draw_text(&fb, r, "Terminal too small",
                            LAPKA_THEME_DEFAULT.error, 0, LAPKA_ATTR_BOLD);
        } else {
            cb->draw(cb->ud, &fb);
        }

        lapka_flush(&fb, in_fd, lapka_term_color_depth());

        if (cb->tick && cb->tick(cb->ud)) { result = 0; break; }

        struct pollfd pfds[2];
        nfds_t nfds = 0;
        pfds[nfds].fd = in_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        nfds++;
        if (extra_fd >= 0) {
            pfds[nfds].fd = extra_fd;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }

        int prc = poll(pfds, nfds, 100);
        if (prc < 0) {
            if (errno == EINTR) continue;
            result = 1;
            break;
        }
        if (prc == 0) continue;

        if (pfds[0].revents & POLLIN) {
            LapkaEvent ev;
            int rc = lapka_event_read_timeout(in_fd, &ev, 0);
            if (rc < 0) { result = 1; break; }
            if (ev.type == LAPKA_EVENT_NONE) continue;
            if (cb->handle(cb->ud, &ev)) { result = 0; break; }
        }
    }

    lapka_fb_free(&fb);
    lapka_term_shutdown();
    return result;
}