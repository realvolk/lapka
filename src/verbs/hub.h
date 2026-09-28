/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    LAPKA_WIDGET_MENU,
    LAPKA_WIDGET_INPUT,
    LAPKA_WIDGET_PASSWORD,
    LAPKA_WIDGET_YESNO,
    LAPKA_WIDGET_MULTISELECT,
    LAPKA_WIDGET_FILTER,
    LAPKA_WIDGET_SUBHUB,
} LapkaWidgetType;

typedef struct {
    char           *key;
    char           *label;
    LapkaWidgetType widget;
    char           *value;
    char          **choices;
    int             choice_count;
    char           *choices_cmd;
    char           *placeholder;
    char           *message;
    char           *visible_if_key;
    char           *visible_if_values;
    char           *subhub;
} LapkaHubItem;

typedef struct {
    char          *id;
    char          *label;
    LapkaHubItem  *items;
    int            item_count;
} LapkaHubCategory;

typedef struct {
    char              *title;
    char             **actions;
    int                action_count;
    LapkaHubCategory  *categories;
    int                category_count;
    char              *dir;
} LapkaHubDef;

LapkaHubDef *lapka_hub_parse(FILE *fp);
LapkaHubDef *lapka_hub_parse_file(const char *path);
void         lapka_hub_def_free(LapkaHubDef *def);
int          lapka_hub_serialize(LapkaHubDef *def, FILE *fp);

int cmd_hub(int argc, char **argv);