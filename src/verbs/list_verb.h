/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/primitive.h>
#include <render/draw.h>

typedef struct {
    const char *title;
    const char *body;
    const char *const *items;
    int item_count;
    LapkaSelectMode mode;
    const LapkaHintItem *hint;
    int hint_count;
} ListVerbArgs;

typedef void (*ListResult)(LapkaPrimitive *list, void *ud);

int lapka_parse_list_args(int argc, char **argv, LapkaSelectMode mode,
                          ListVerbArgs *out);

int lapka_run_list_verb(const ListVerbArgs *args,
                        ListResult on_result, void *ud);