/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stdbool.h>
#include <termios.h>

void lapka_signals_save(int fd, const struct termios *orig, bool alt_screen);
void lapka_signals_install(void);
bool lapka_signals_take_resize(void);
int  lapka_signals_fd(void);