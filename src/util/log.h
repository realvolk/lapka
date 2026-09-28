/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

void lapka_log_open(const char *path);
void lapka_log_close(void);
int  lapka_log_fd(void);
int  lapka_log_write(const char *s, unsigned long n);