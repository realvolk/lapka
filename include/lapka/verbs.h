/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>
#include <stdbool.h>

typedef struct {
    bool alt_screen;
    bool mouse;
    bool paste;
    bool focus;
} LapkaInitOptions;

typedef struct {
    void (*draw)(void *ud, LapkaFramebuffer *fb);
    bool (*handle)(void *ud, const LapkaEvent *ev);
    bool (*tick)(void *ud);
    void *ud;
} LapkaRunCallbacks;

int lapka_run(const LapkaInitOptions *opts, const LapkaRunCallbacks *cb,
              int extra_fd);