/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/theme.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t lapka_color_darken(uint32_t rgb, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;

    r = r * (uint32_t)(100 - percent) / 100;
    g = g * (uint32_t)(100 - percent) / 100;
    b = b * (uint32_t)(100 - percent) / 100;

    return (r << 16) | (g << 8) | b;
}

const LapkaTheme LAPKA_THEME_DEFAULT = {
    0x000000,
    0xFFFFFF,
    0x00AF00,
    0xFF87D7,
    0x888888,
    0xCC0000,
    0xCCCC00,
    0x00AF00,
    0x000000,
    0x9F5A85,
    0xC06BA5,
    LAPKA_BORDER_SINGLE,
    "|",
    "x",
    "(*)",
    "( )",
    "*",
    ">",
    false,
};

static LapkaTheme s_active;
static bool       s_active_valid = false;
static char      *s_active_path  = NULL;

static int parse_color(const char *s, uint32_t *out) {
    if (!s) return -1;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    else if (s[0] == '#') s += 1;

    size_t len = strlen(s);
    if (len != 6) return -1;

    uint32_t v = 0;
    for (size_t i = 0; i < 6; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
        else return -1;
    }
    *out = v;
    return 0;
}

static LapkaBorder parse_border(const char *s) {
    if (!s) return LAPKA_BORDER_SINGLE;
    if (strcmp(s, "none") == 0)    return LAPKA_BORDER_NONE;
    if (strcmp(s, "single") == 0)  return LAPKA_BORDER_SINGLE;
    if (strcmp(s, "rounded") == 0) return LAPKA_BORDER_ROUNDED;
    if (strcmp(s, "double") == 0)  return LAPKA_BORDER_DOUBLE;
    return LAPKA_BORDER_SINGLE;
}

int lapka_theme_load(const char *path, LapkaTheme *out) {
    if (!path || !out) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    bool has_modal_bg = false;
    bool has_modal_border = false;
    bool has_modal_title = false;

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&line, &cap, f)) >= 0) {
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0 || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *value = eq + 1;

        while (*value == ' ' || *value == '\t') value++;

        uint32_t color;
        if (strcmp(key, "bg") == 0) {
            if (parse_color(value, &color) == 0) out->bg = color;
        } else if (strcmp(key, "fg") == 0) {
            if (parse_color(value, &color) == 0) out->fg = color;
        } else if (strcmp(key, "accent") == 0) {
            if (parse_color(value, &color) == 0) out->accent = color;
        } else if (strcmp(key, "title") == 0) {
            if (parse_color(value, &color) == 0) out->title = color;
        } else if (strcmp(key, "muted") == 0) {
            if (parse_color(value, &color) == 0) out->muted = color;
        } else if (strcmp(key, "error") == 0) {
            if (parse_color(value, &color) == 0) out->error = color;
        } else if (strcmp(key, "warning") == 0) {
            if (parse_color(value, &color) == 0) out->warning = color;
        } else if (strcmp(key, "success") == 0) {
            if (parse_color(value, &color) == 0) out->success = color;
        } else if (strcmp(key, "modal_bg") == 0) {
            if (parse_color(value, &color) == 0) { out->modal_bg = color; has_modal_bg = true; }
        } else if (strcmp(key, "modal_border") == 0) {
            if (parse_color(value, &color) == 0) { out->modal_border = color; has_modal_border = true; }
        } else if (strcmp(key, "modal_title") == 0) {
            if (parse_color(value, &color) == 0) { out->modal_title = color; has_modal_title = true; }
        } else if (strcmp(key, "border") == 0) {
            out->border = parse_border(value);
        } else if (strcmp(key, "cursor") == 0) {
            out->cursor = strdup(value);
        } else if (strcmp(key, "check") == 0) {
            out->check = strdup(value);
        } else if (strcmp(key, "radio_on") == 0) {
            out->radio_on = strdup(value);
        } else if (strcmp(key, "radio_off") == 0) {
            out->radio_off = strdup(value);
        } else if (strcmp(key, "bullet") == 0) {
            out->bullet = strdup(value);
        } else if (strcmp(key, "arrow") == 0) {
            out->arrow = strdup(value);
        }
    }

    free(line);
    fclose(f);

    if (!has_modal_bg) out->modal_bg = out->bg;
    if (!has_modal_border) out->modal_border = lapka_color_darken(out->title, 40);
    if (!has_modal_title) out->modal_title = lapka_color_darken(out->title, 20);

    out->owns_strings = true;
    return 0;
}

int lapka_theme_set_file(const char *path) {
    if (!path) {
        lapka_theme_reset();
        return 0;
    }

    LapkaTheme t = LAPKA_THEME_DEFAULT;
    if (lapka_theme_load(path, &t) != 0) return -1;

    lapka_theme_free(&s_active);
    s_active = t;
    s_active_valid = true;

    free(s_active_path);
    s_active_path = strdup(path);
    return 0;
}

void lapka_theme_reset(void) {
    lapka_theme_free(&s_active);
    s_active = LAPKA_THEME_DEFAULT;
    s_active_valid = false;
    free(s_active_path);
    s_active_path = NULL;
}

void lapka_theme_free(LapkaTheme *t) {
    if (!t || !t->owns_strings) return;
    free((char *)t->cursor);
    free((char *)t->check);
    free((char *)t->radio_on);
    free((char *)t->radio_off);
    free((char *)t->bullet);
    free((char *)t->arrow);
    t->cursor = NULL;
    t->check = NULL;
    t->radio_on = NULL;
    t->radio_off = NULL;
    t->bullet = NULL;
    t->arrow = NULL;
    t->owns_strings = false;
}

const LapkaTheme *lapka_theme_active(void) {
    if (s_active_valid) return &s_active;

    const char *path = getenv("LAPKA_THEME");
    if (!path || !path[0]) {
        s_active = LAPKA_THEME_DEFAULT;
        s_active_valid = true;
        return &s_active;
    }

    LapkaTheme t = LAPKA_THEME_DEFAULT;
    if (lapka_theme_load(path, &t) != 0) {
        s_active = LAPKA_THEME_DEFAULT;
        s_active_valid = true;
        return &s_active;
    }

    lapka_theme_free(&s_active);
    s_active = t;
    s_active_valid = true;
    free(s_active_path);
    s_active_path = strdup(path);
    return &s_active;
}