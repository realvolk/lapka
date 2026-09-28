/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "hub.h"

#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static LapkaWidgetType parse_widget(const char *s) {
    if (strcmp(s, "menu") == 0)        return LAPKA_WIDGET_MENU;
    if (strcmp(s, "input") == 0)       return LAPKA_WIDGET_INPUT;
    if (strcmp(s, "password") == 0)    return LAPKA_WIDGET_PASSWORD;
    if (strcmp(s, "yesno") == 0)       return LAPKA_WIDGET_YESNO;
    if (strcmp(s, "multiselect") == 0) return LAPKA_WIDGET_MULTISELECT;
    if (strcmp(s, "filter") == 0)      return LAPKA_WIDGET_FILTER;
    if (strcmp(s, "subhub") == 0)      return LAPKA_WIDGET_SUBHUB;
    return (LapkaWidgetType)-1;
}

static char *read_quoted(const char **pp) {
    const char *p = *pp;
    if (*p != '"') return NULL;
    p++;

    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;

    while (*p && *p != '"') {
        if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) p++;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nv = realloc(out, cap);
            if (!nv) { free(out); return NULL; }
            out = nv;
        }
        out[len++] = *p++;
    }
    if (*p == '"') p++;
    out[len] = '\0';
    *pp = p;
    return out;
}

static int next_field(const char **pp, char **key_out, char **value_out) {
    const char *p = *pp;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return 0;

    const char *kstart = p;
    while (*p && *p != '=' && *p != ' ' && *p != '\t') p++;
    if (*p != '=') return 0;

    size_t klen = (size_t)(p - kstart);
    char *key = malloc(klen + 1);
    if (!key) return 0;
    memcpy(key, kstart, klen);
    key[klen] = '\0';
    p++;

    char *value = NULL;
    if (*p == '"') {
        value = read_quoted(&p);
        if (!value) { free(key); return 0; }
    } else {
        const char *vstart = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        size_t vlen = (size_t)(p - vstart);
        value = malloc(vlen + 1);
        if (!value) { free(key); return 0; }
        memcpy(value, vstart, vlen);
        value[vlen] = '\0';
    }

    *pp = p;
    *key_out = key;
    *value_out = value;
    return 1;
}

static char *read_tail(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        const char *q = p;
        char *s = read_quoted(&q);
        return s;
    }
    return xstrdup(p);
}

static int parse_category(const char *line, char **id_out, char **label_out) {
    const char *p = line + 9;
    while (*p == ' ' || *p == '\t') p++;

    const char *id_start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '"') p++;
    size_t id_len = (size_t)(p - id_start);
    char *id = malloc(id_len + 1);
    if (!id) return -1;
    memcpy(id, id_start, id_len);
    id[id_len] = '\0';

    while (*p == ' ' || *p == '\t') p++;
    char *label = read_tail(p);
    if (!label) { free(id); return -1; }

    *id_out = id;
    *label_out = label;
    return 0;
}

static void free_item(LapkaHubItem *it) {
    free(it->key);
    free(it->label);
    free(it->value);
    for (int i = 0; i < it->choice_count; i++) free(it->choices[i]);
    free(it->choices);
    free(it->choices_cmd);
    free(it->placeholder);
    free(it->message);
    free(it->visible_if_key);
    free(it->visible_if_values);
    free(it->subhub);
    memset(it, 0, sizeof(*it));
}

static int parse_item(const char *line, LapkaHubItem *it) {
    memset(it, 0, sizeof(*it));
    it->value = xstrdup("");

    const char *p = line + 5;
    char *key, *value;
    while (next_field(&p, &key, &value)) {
        if (strcmp(key, "key") == 0) {
            free(it->key); it->key = value;
        } else if (strcmp(key, "label") == 0) {
            free(it->label); it->label = value;
        } else if (strcmp(key, "widget") == 0) {
            LapkaWidgetType w = parse_widget(value);
            if ((int)w >= 0) it->widget = w;
            free(value);
        } else if (strcmp(key, "value") == 0) {
            free(it->value); it->value = value;
        } else if (strcmp(key, "choices_cmd") == 0) {
            free(it->choices_cmd); it->choices_cmd = value;
        } else if (strcmp(key, "placeholder") == 0) {
            free(it->placeholder); it->placeholder = value;
        } else if (strcmp(key, "message") == 0) {
            free(it->message); it->message = value;
        } else if (strcmp(key, "subhub") == 0) {
            free(it->subhub); it->subhub = value;
        } else if (strcmp(key, "visible_if") == 0) {
            char *eq = strchr(value, '=');
            if (eq) {
                *eq = '\0';
                free(it->visible_if_key);
                free(it->visible_if_values);
                it->visible_if_key = value;
                it->visible_if_values = xstrdup(eq + 1);
            } else {
                free(value);
            }
        } else {
            free(value);
        }
        free(key);
    }

    if (!it->key || !it->label) {
        free_item(it);
        return -1;
    }
    return 0;
}

static char *dirname_of(const char *path) {
    if (!path) return NULL;
    char *copy = xstrdup(path);
    if (!copy) return NULL;
    char *slash = strrchr(copy, '/');
    if (!slash) {
        free(copy);
        return xstrdup(".");
    }
    if (slash == copy) {
        free(copy);
        return xstrdup("/");
    }
    *slash = '\0';
    return copy;
}

LapkaHubDef *lapka_hub_parse(FILE *fp) {
    LapkaHubDef *def = calloc(1, sizeof(*def));
    if (!def) return NULL;

    LapkaHubCategory *cur_cat = NULL;
    LapkaHubItem *cur_item = NULL;

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&line, &cap, fp)) >= 0) {
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        if (strncmp(p, "hub ", 4) == 0) {
            free(def->title);
            def->title = read_tail(p + 4);
        } else if (strncmp(p, "action ", 7) == 0) {
            char *a = read_tail(p + 7);
            if (a) {
                def->actions = realloc(def->actions,
                    (size_t)(def->action_count + 1) * sizeof(char *));
                def->actions[def->action_count++] = a;
            }
        } else if (strncmp(p, "category ", 9) == 0) {
            char *id, *label;
            if (parse_category(p, &id, &label) == 0) {
                def->categories = realloc(def->categories,
                    (size_t)(def->category_count + 1) * sizeof(LapkaHubCategory));
                cur_cat = &def->categories[def->category_count++];
                memset(cur_cat, 0, sizeof(*cur_cat));
                cur_cat->id = id;
                cur_cat->label = label;
                cur_item = NULL;
            }
        } else if (strncmp(p, "item ", 5) == 0) {
            if (!cur_cat) continue;
            LapkaHubItem item;
            if (parse_item(p, &item) == 0) {
                cur_cat->items = realloc(cur_cat->items,
                    (size_t)(cur_cat->item_count + 1) * sizeof(LapkaHubItem));
                cur_item = &cur_cat->items[cur_cat->item_count++];
                *cur_item = item;
            }
        } else if (strncmp(p, "choice ", 7) == 0) {
            if (!cur_item) continue;
            char *c = read_tail(p + 7);
            if (c) {
                cur_item->choices = realloc(cur_item->choices,
                    (size_t)(cur_item->choice_count + 1) * sizeof(char *));
                cur_item->choices[cur_item->choice_count++] = c;
            }
        }
    }

    free(line);

    if (def->action_count == 0) {
        def->actions = malloc(2 * sizeof(char *));
        if (def->actions) {
            def->actions[0] = xstrdup("Back");
            def->actions[1] = xstrdup("Proceed");
            def->action_count = 2;
        }
    }

    if (def->category_count == 0) {
        lapka_hub_def_free(def);
        return NULL;
    }

    return def;
}

LapkaHubDef *lapka_hub_parse_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    LapkaHubDef *def = lapka_hub_parse(f);
    fclose(f);
    if (!def) return NULL;
    def->dir = dirname_of(path);
    return def;
}

void lapka_hub_def_free(LapkaHubDef *def) {
    if (!def) return;
    free(def->title);
    free(def->dir);
    for (int i = 0; i < def->action_count; i++) free(def->actions[i]);
    free(def->actions);
    for (int c = 0; c < def->category_count; c++) {
        LapkaHubCategory *cat = &def->categories[c];
        free(cat->id);
        free(cat->label);
        for (int i = 0; i < cat->item_count; i++) free_item(&cat->items[i]);
        free(cat->items);
    }
    free(def->categories);
    free(def);
}