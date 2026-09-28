/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "checklist.h"
#include "list_verb.h"

#include <render/draw.h>
#include <stdio.h>

static const LapkaHintItem CHECKLIST_HINT[] = {
    { "\xe2\x86\x91\xe2\x86\x93", "move" },
    { "space", "toggle" },
    { "enter", "confirm" },
    { "esc",   "cancel" },
};

static void print_many(LapkaPrimitive *list, void *ud) {
    (void)ud;
    int n = lapka_list_result_count(list);
    for (int i = 0; i < n; i++) {
        const char *s = lapka_list_result(list, i);
        if (s) printf("%s\n", s);
    }
}

int cmd_checklist(int argc, char **argv) {
    ListVerbArgs args;
    int rc = lapka_parse_list_args(argc, argv, LAPKA_SELECT_MANY, &args);
    if (rc != 0) return rc;
    args.mode = LAPKA_SELECT_MANY;
    args.hint = CHECKLIST_HINT;
    args.hint_count = (int)(sizeof(CHECKLIST_HINT) / sizeof(CHECKLIST_HINT[0]));
    return lapka_run_list_verb(&args, print_many, NULL);
}