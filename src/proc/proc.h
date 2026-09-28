/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    int   fd;
    int   status;
    bool  done;
} LapkaProc;

int  lapka_proc_start(const char *const *argv, LapkaProc *p);
int  lapka_proc_read(LapkaProc *p, char *buf, size_t cap);
int  lapka_proc_reap(LapkaProc *p);
void lapka_proc_terminate(LapkaProc *p);
void lapka_proc_close(LapkaProc *p);