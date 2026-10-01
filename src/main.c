/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/lapka.h>
#include "verbs/msg.h"
#include "verbs/yesno.h"
#include "verbs/input.h"
#include "verbs/menu.h"
#include "verbs/checklist.h"
#include "verbs/filter.h"
#include "verbs/password.h"
#include "verbs/password_confirm.h"
#include "verbs/spin.h"
#include "verbs/show_file.h"
#include "verbs/hub.h"
#include "verbs/toast.h"
#include "verbs/editor.h"
#include "verbs/explorer.h"
#include <stdio.h>
#include <string.h>

static void print_usage(FILE *out) {
    fprintf(out,
        "usage: tui VERB [ARGS...]\n"
        "\n"
        "verbs:\n"
        "  msg                \"Title\" \"Body\"\n"
        "  yesno              \"Title\" \"Body\"\n"
        "  input              \"Title\" \"Body\" [--default D]\n"
        "  menu               \"Title\" \"Body\" item...\n"
        "  checklist          \"Title\" \"Body\" item...\n"
        "  filter             \"Title\" \"Body\" [--multi] [--placeholder P]\n"
        "  show-file          \"Title\" FILE\n"
        "  editor             FILE [--new]\n"
        "  explorer           [PATH] [--dir|--file]\n"
        "  toast              \"Message\" [--duration=N]\n"
        "  spin               \"Title\" \"Body\" [--where=TL|TC|TR|LC|CC|RC|BL|BC|BR] [--hold N|enter|none] -- CMD...\n"
        "  password           \"Title\" \"Body\"\n"
        "  password-confirm   \"Title\" \"Body\" [--confirm-prompt P] [--tries N]\n"
        "  hub                --in FILE --out FILE [--check]\n"
        "\n"
        "flags:\n"
        "  --version          print version\n"
        "  --help, -h         print this message\n"
        "\n"
        "environment:\n"
        "  LAPKA_ALT_SCREEN   if set to 1, the caller owns the alt screen\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(stderr);
        return 3;
    }

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strncmp(a, "--theme=", 8) == 0) {
            if (lapka_theme_set_file(a + 8) != 0) {
                fprintf(stderr, "tui: cannot load theme: %s\n", a + 8);
                return 2;
            }
            for (int j = i; j + 1 < argc; j++) argv[j] = argv[j + 1];
            argc--;
            i--;
            continue;
        }
        if (strcmp(a, "--theme") == 0 && i + 1 < argc) {
            if (lapka_theme_set_file(argv[i + 1]) != 0) {
                fprintf(stderr, "tui: cannot load theme: %s\n", argv[i + 1]);
                return 2;
            }
            for (int j = i; j + 2 < argc; j++) argv[j] = argv[j + 2];
            argc -= 2;
            i--;
            continue;
        }
    }

    const char *verb = argv[1];

    if (strcmp(verb, "--version") == 0) {
        printf("lapka %s\n", LAPKA_VERSION);
        return 0;
    }
    if (strcmp(verb, "--help") == 0 || strcmp(verb, "-h") == 0) {
        print_usage(stdout);
        return 0;
    }
    if (strcmp(verb, "msg") == 0)              return cmd_msg(argc - 2, argv + 2);
    if (strcmp(verb, "yesno") == 0)            return cmd_yesno(argc - 2, argv + 2);
    if (strcmp(verb, "input") == 0)            return cmd_input(argc - 2, argv + 2);
    if (strcmp(verb, "menu") == 0)             return cmd_menu(argc - 2, argv + 2);
    if (strcmp(verb, "checklist") == 0)        return cmd_checklist(argc - 2, argv + 2);
    if (strcmp(verb, "filter") == 0)           return cmd_filter(argc - 2, argv + 2);
    if (strcmp(verb, "spin") == 0)             return cmd_spin(argc - 2, argv + 2);
    if (strcmp(verb, "show-file") == 0)        return cmd_show_file(argc - 2, argv + 2);
    if (strcmp(verb, "password") == 0)         return cmd_password(argc - 2, argv + 2);
    if (strcmp(verb, "password-confirm") == 0) return cmd_password_confirm(argc - 2, argv + 2);
    if (strcmp(verb, "hub") == 0)              return cmd_hub(argc - 2, argv + 2);
    if (strcmp(verb, "toast") == 0)            return cmd_toast(argc - 2, argv + 2);
    if (strcmp(verb, "editor") == 0)           return cmd_editor(argc - 2, argv + 2);
    if (strcmp(verb, "explorer") == 0)         return cmd_explorer(argc - 2, argv + 2);

    fprintf(stderr, "tui: unknown verb: %s\n", verb);
    print_usage(stderr);
    return 3;
}