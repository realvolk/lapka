/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include <stdlib.h>

void lapka_primitive_free(LapkaPrimitive *p) {
    if (!p) return;
    if (p->free) p->free(p);
    else free(p);
}