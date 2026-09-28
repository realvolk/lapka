/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "menu.h"
#include "list_verb.h"

#include <render/draw.h>
#include <stdio.h>

static const LapkaHintItem MENU_HINT[] = {
    { "\xe2\x86\x91\xe2\x86\x93", "move" },
    { "enter", "select" },
    { "esc",   "cancel" },
};

static void print_single(LapkaPrimitive *list, void *ud) {
    (void)ud;
    const char *sel = lapka_list_cursor_item(list);
    if (sel) printf("%s\n", sel);
}

int cmd_menu(int argc, char **argv) {
    ListVerbArgs args;
    int rc = lapka_parse_list_args(argc, argv, LAPKA_SELECT_ONE, &args);
    if (rc != 0) return rc;
    args.mode = LAPKA_SELECT_ONE;
    args.hint = MENU_HINT;
    args.hint_count = (int)(sizeof(MENU_HINT) / sizeof(MENU_HINT[0]));
    return lapka_run_list_verb(&args, print_single, NULL);
}