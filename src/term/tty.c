/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "tty.h"

#include <fcntl.h>
#include <unistd.h>

int lapka_output_fd(bool prefer_stdout) {
    if (prefer_stdout) return STDOUT_FILENO;

    int fd = open("/dev/tty", O_WRONLY);
    if (fd >= 0) return fd;

    return STDERR_FILENO;
}

int lapka_input_fd(void) {
    int fd = open("/dev/tty", O_RDONLY);
    return fd;
}