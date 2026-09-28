/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/types.h>

int lapka_event_read(int fd, LapkaEvent *out);
int lapka_event_read_timeout(int fd, LapkaEvent *out, int timeout_ms);