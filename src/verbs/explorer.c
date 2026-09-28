/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "explorer.h"

#include <lapka/lapka.h>
#include <lapka/layer.h>
#include <lapka/primitive.h>
#include <core/utf8.h>
#include <render/draw.h>
#include <term/capabilities.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define EXPLORER_MAX_ENTRIES 4096

typedef enum {
    XPLORE_BROWSE,
    XPLORE_SEARCH,
    XPLORE_ACTION,
    XPLORE_CONFIRM_DELETE,
    XPLORE_RENAME,
    XPLORE_VIEW,
    XPLORE_EDIT
} ExploreMode;

typedef struct {
    char  *name;
    char  *full;
    bool   is_dir;
    off_t  size;
    time_t mtime;
} Entry;

typedef struct {
    const char *start;
    const char *title;
    int         want_dir;

    char       *cwd;
    Entry      *entries;
    int         entry_count;
    int         cursor;
    int         scroll;

    bool        show_hidden;
    bool        show_sizes;

    Entry      *all_entries;
    int         all_count;
    char        search_query[256];
    int        *filtered;
    int         filtered_count;

    ExploreMode mode;
    int         action_cursor;

    LapkaPrimitive *search;
    LapkaPrimitive *rename;
    LapkaPrimitive *pager;
    LapkaPrimitive *editor;

    int      confirm_stage;
    char     confirm_target[PATH_MAX];
    char     confirm_name[256];

    int      exit_code;
    char    *result;
    int      done;

    LapkaPrimitive *toast;
    LapkaLayer     *toast_layer;
    long long       toast_start_ms;
    int             toast_duration_ms;
} ExploreData;

static int terminal_supports_emoji(void) {
    const char *env = getenv("LAPKA_EMOJI");
    if (env && env[0]) {
        if (env[0] == '0') return 0;
        if (env[0] == '1') return 1;
    }

    if (lapka_cap_is_basic_term()) return 0;

    const char *loc = getenv("LC_ALL");
    if (!loc || !loc[0]) loc = getenv("LC_CTYPE");
    if (!loc || !loc[0]) loc = getenv("LANG");
    if (!loc || !loc[0]) return 0;
    if (strstr(loc, "UTF-8") || strstr(loc, "utf8") ||
        strstr(loc, "UTF8")) return 1;
    return 0;
}


static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void show_toast(ExploreData *d, const char *msg, int ms) {
    if (d->toast) lapka_primitive_free(d->toast);
    d->toast = lapka_notification_new(msg, ms);
    d->toast_start_ms = now_ms();
    d->toast_duration_ms = ms;
}

static int process_is_root(void) {
    if (geteuid() == 0) return 1;
    if (getuid() == 0) return 1;
    return 0;
}

static int entry_cmp(const void *a, const void *b) {
    const Entry *ea = a;
    const Entry *eb = b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

static int is_hidden(const char *name) {
    return name[0] == '.' && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static int fuzzy_match_ci(const char *text, const char *query) {
    if (!query || !query[0]) return 1;
    const char *t = text;
    const char *q = query;
    while (*q) {
        char qc = *q;
        if (qc >= 'A' && qc <= 'Z') qc += 32;
        int found = 0;
        while (*t) {
            char tc = *t;
            if (tc >= 'A' && tc <= 'Z') tc += 32;
            t++;
            if (tc == qc) { found = 1; break; }
        }
        if (!found) return 0;
        q++;
    }
    return 1;
}

static void rebuild_filter(ExploreData *d) {
    free(d->filtered);
    d->filtered = NULL;
    d->filtered_count = 0;

    if (!d->search_query[0]) {
        d->filtered = malloc((size_t)(d->all_count ? d->all_count : 1) * sizeof(int));
        if (!d->filtered) return;
        for (int i = 0; i < d->all_count; i++) d->filtered[i] = i;
        d->filtered_count = d->all_count;
    } else {
        d->filtered = malloc((size_t)(d->all_count ? d->all_count : 1) * sizeof(int));
        if (!d->filtered) return;
        for (int i = 0; i < d->all_count; i++) {
            if (fuzzy_match_ci(d->all_entries[i].name, d->search_query)) {
                d->filtered[d->filtered_count++] = i;
            }
        }
    }

    d->entry_count = d->filtered_count;
    d->cursor = 0;
    d->scroll = 0;
}

static int load_dir(ExploreData *d, const char *path) {
    char path_copy[PATH_MAX];
    int pn = snprintf(path_copy, sizeof(path_copy), "%s", path);
    if (pn < 0 || pn >= (int)sizeof(path_copy)) return -1;

    DIR *dir = opendir(path_copy);
    if (!dir) return -1;

    for (int i = 0; i < d->all_count; i++) {
        free(d->all_entries[i].name);
        free(d->all_entries[i].full);
    }
    free(d->all_entries);
    d->all_entries = NULL;
    d->all_count = 0;

    int cap = 64;
    d->all_entries = malloc((size_t)cap * sizeof(Entry));
    if (!d->all_entries) { closedir(dir); return -1; }

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (!d->show_hidden && is_hidden(de->d_name)) continue;
        if (d->all_count >= EXPLORER_MAX_ENTRIES) break;

        if (d->all_count >= cap) {
            cap *= 2;
            Entry *ne = realloc(d->all_entries, (size_t)cap * sizeof(Entry));
            if (!ne) break;
            d->all_entries = ne;
        }

        char full[PATH_MAX];
        int n;
        if (strcmp(path_copy, "/") == 0) {
            n = snprintf(full, sizeof(full), "/%s", de->d_name);
        } else {
            n = snprintf(full, sizeof(full), "%s/%s", path_copy, de->d_name);
        }
        if (n < 0 || n >= (int)sizeof(full)) continue;

        struct stat st;
        int is_dir = 0;
        off_t size = 0;
        time_t mtime = 0;
        if (lstat(full, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
            size = st.st_size;
            mtime = st.st_mtime;
        }

        Entry *e = &d->all_entries[d->all_count++];
        e->name = strdup(de->d_name);
        e->full = strdup(full);
        e->is_dir = is_dir;
        e->size = size;
        e->mtime = mtime;
    }
    closedir(dir);

    qsort(d->all_entries, (size_t)d->all_count, sizeof(Entry), entry_cmp);

    free(d->cwd);
    d->cwd = strdup(path_copy);
    d->search_query[0] = '\0';
    rebuild_filter(d);

    return 0;
}

static void parent_path(const char *path, char *out, size_t cap) {
    if (strcmp(path, "/") == 0) {
        snprintf(out, cap, "/");
        return;
    }
    snprintf(out, cap, "%s", path);
    size_t len = strlen(out);
    while (len > 1 && out[len - 1] == '/') out[--len] = '\0';
    char *slash = strrchr(out, '/');
    if (!slash || slash == out) {
        snprintf(out, cap, "/");
    } else {
        *slash = '\0';
    }
}

static const char *mode_action_label(int idx, int want_dir) {
    static const char *file_actions[] = {
        "Select", "Edit", "View", "Delete", "Cancel"
    };
    static const char *dir_actions[] = {
        "Enter", "Select this directory", "Delete", "Cancel"
    };
    if (want_dir) {
        if (idx >= 0 && idx < 4) return dir_actions[idx];
    } else {
        if (idx >= 0 && idx < 5) return file_actions[idx];
    }
    return NULL;
}

static int action_count(int want_dir) {
    return want_dir ? 4 : 5;
}

static int permission_ok(const char *path, int write) {
    if (process_is_root()) return 1;
    return access(path, write ? W_OK : R_OK) == 0;
}

static int file_size_str(off_t size, char *out, size_t cap) {
    const char *unit = "B";
    double v = (double)size;
    if (v >= 1024.0) { v /= 1024.0; unit = "K"; }
    if (v >= 1024.0) { v /= 1024.0; unit = "M"; }
    if (v >= 1024.0) { v /= 1024.0; unit = "G"; }
    return snprintf(out, cap, "%.1f%s", v, unit);
}

static void put_cp(LapkaCell *buf, int w, int h, int x, int y, uint32_t cp,
                   uint32_t fg, uint32_t bg, uint16_t attrs) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    LapkaCell c = { cp, fg, bg, attrs };
    buf[(size_t)y * (size_t)w + (size_t)x] = c;
}

static int draw_str(LapkaCell *buf, int w, int h, int x, int y,
                    const char *s, uint32_t fg, uint32_t bg, uint16_t attrs,
                    int max_cols) {
    if (!s) return 0;
    const uint8_t *p = (const uint8_t *)s;
    int cx = x;
    int used = 0;
    while (*p) {
        uint32_t cp;
        int bytes = lapka_utf8_decode(p, 4, &cp);
        if (bytes <= 0) break;
        p += bytes;
        int cw = lapka_cp_width(cp);
        if (cw == 0) continue;
        if (max_cols >= 0 && used + cw > max_cols) break;
        for (int i = 0; i < cw; i++) {
            put_cp(buf, w, h, cx + i, y, i == 0 ? cp : 0, fg, bg, attrs);
        }
        cx += cw;
        used += cw;
    }
    return used;
}

static void draw_browser(LapkaFramebuffer *fb, ExploreData *d,
                         const LapkaTheme *theme) {
    char header[PATH_MAX + 16];
    snprintf(header, sizeof(header), " %s ", d->cwd ? d->cwd : "/");
    LapkaRect hr = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, hr, header, theme->title, 0, LAPKA_ATTR_BOLD);

    int top = 1;
    int bottom = fb->h - 1;
    int body_h = bottom - top;
    if (body_h < 0) body_h = 0;

    if (d->cursor < d->scroll) d->scroll = d->cursor;
    if (d->cursor >= d->scroll + body_h) d->scroll = d->cursor - body_h + 1;
    if (d->scroll < 0) d->scroll = 0;

    for (int i = 0; i < body_h; i++) {
        int vis = d->scroll + i;
        if (vis >= d->filtered_count) break;
        int idx = d->filtered[vis];
        Entry *e = &d->all_entries[idx];
        int sel = (idx == d->cursor);

        uint32_t fg = sel ? theme->accent : 0;
        uint16_t attrs = sel ? LAPKA_ATTR_BOLD : 0;

        int y = top + i;
        int x = 0;

        static int emoji_ok = -1;
        if (emoji_ok < 0) emoji_ok = terminal_supports_emoji();

        const char *glyph;
        int glyph_w;
        if (e->is_dir) {
            if (emoji_ok) {
                glyph = "\xf0\x9f\x93\x81";
                glyph_w = 2;
            } else {
                glyph = "(dir)";
                glyph_w = 5;
            }
        } else {
            glyph = emoji_ok ? "  " : "     ";
            glyph_w = emoji_ok ? 2 : 5;
        }
        x += draw_str(fb->cur, fb->w, fb->h, x, y, glyph, fg, 0, attrs, glyph_w);
        x += draw_str(fb->cur, fb->w, fb->h, x, y, " ", fg, 0, attrs, 1);

        char line[PATH_MAX + 32];
        if (d->show_sizes && !e->is_dir) {
            char sz[16];
            file_size_str(e->size, sz, sizeof(sz));
            snprintf(line, sizeof(line), "%s  %8s", e->name, sz);
        } else if (e->is_dir) {
            snprintf(line, sizeof(line), "%s/", e->name);
        } else {
            snprintf(line, sizeof(line), "%s", e->name);
        }
        draw_str(fb->cur, fb->w, fb->h, x, y, line, fg, 0, attrs, fb->w - x);
    }

    if (d->filtered_count == 0) {
        const char *msg = d->search_query[0] ? "(no matches)" : "(empty)";
        draw_str(fb->cur, fb->w, fb->h, 0, top, msg,
                 theme->muted, 0, 0, fb->w);
    }

    const char *hint =
        "enter open/act  <- parent  . hidden  s sizes  / search  esc cancel";
    LapkaRect fr = { 0, fb->h - 1, fb->w, 1 };
    lapka_draw_text(fb, fr, hint, theme->muted, 0, 0);
}

static void draw_action_menu(LapkaFramebuffer *fb, ExploreData *d,
                             const LapkaTheme *theme) {
    Entry *e = d->cursor < d->entry_count ? &d->entries[d->cursor] : NULL;
    if (!e) return;

    int want_dir = d->want_dir && e->is_dir;
    int n = action_count(want_dir);

    int mw = 44;
    int mh = n + 4;
    if (mw > fb->w - 4) mw = fb->w - 4;
    if (mh > fb->h - 4) mh = fb->h - 4;
    int mx = (fb->w - mw) / 2;
    int my = (fb->h - mh) / 2;

    LapkaCell *buf = fb->cur;
    for (int y = my; y < my + mh; y++) {
        for (int x = mx; x < mx + mw; x++) {
            put_cp(buf, fb->w, fb->h, x, y, ' ', 0, theme->modal_bg, 0);
        }
    }

    for (int x = mx; x < mx + mw; x++) {
        put_cp(buf, fb->w, fb->h, x, my, 0x2500, theme->modal_border, theme->modal_bg, 0);
        put_cp(buf, fb->w, fb->h, x, my + mh - 1, 0x2500, theme->modal_border, theme->modal_bg, 0);
    }
    for (int y = my; y < my + mh; y++) {
        put_cp(buf, fb->w, fb->h, mx, y, 0x2502, theme->modal_border, theme->modal_bg, 0);
        put_cp(buf, fb->w, fb->h, mx + mw - 1, y, 0x2502, theme->modal_border, theme->modal_bg, 0);
    }
    put_cp(buf, fb->w, fb->h, mx, my, 0x250C, theme->modal_border, theme->modal_bg, 0);
    put_cp(buf, fb->w, fb->h, mx + mw - 1, my, 0x2510, theme->modal_border, theme->modal_bg, 0);
    put_cp(buf, fb->w, fb->h, mx, my + mh - 1, 0x2514, theme->modal_border, theme->modal_bg, 0);
    put_cp(buf, fb->w, fb->h, mx + mw - 1, my + mh - 1, 0x2518, theme->modal_border, theme->modal_bg, 0);

    char title[256];
    snprintf(title, sizeof(title), " %s ", e->name);
    draw_str(buf, fb->w, fb->h, mx + 2, my, title,
             theme->modal_title, theme->modal_bg, LAPKA_ATTR_BOLD, mw - 4);

    for (int i = 0; i < n; i++) {
        const char *label = mode_action_label(i, want_dir);
        if (!label) continue;
        int sel = (i == d->action_cursor);
        char line[64];
        snprintf(line, sizeof(line), "%s %s", sel ? ">" : " ", label);
        draw_str(buf, fb->w, fb->h, mx + 2, my + 2 + i, line,
                 sel ? theme->accent : 0, theme->modal_bg,
                 sel ? LAPKA_ATTR_BOLD : 0, mw - 4);
    }
}

static void draw_confirm(LapkaFramebuffer *fb, ExploreData *d,
                         const LapkaTheme *theme) {
    int mw = 48;
    int mh = 7;
    if (mw > fb->w - 4) mw = fb->w - 4;
    if (mh > fb->h - 4) mh = fb->h - 4;
    int mx = (fb->w - mw) / 2;
    int my = (fb->h - mh) / 2;

    for (int y = my; y < my + mh; y++) {
        for (int x = mx; x < mx + mw; x++) {
            put_cp(fb->cur, fb->w, fb->h, x, y, ' ', 0, theme->modal_bg, 0);
        }
    }
    for (int x = mx; x < mx + mw; x++) {
        put_cp(fb->cur, fb->w, fb->h, x, my, 0x2500, theme->modal_border, theme->modal_bg, 0);
        put_cp(fb->cur, fb->w, fb->h, x, my + mh - 1, 0x2500, theme->modal_border, theme->modal_bg, 0);
    }
    for (int y = my; y < my + mh; y++) {
        put_cp(fb->cur, fb->w, fb->h, mx, y, 0x2502, theme->modal_border, theme->modal_bg, 0);
        put_cp(fb->cur, fb->w, fb->h, mx + mw - 1, y, 0x2502, theme->modal_border, theme->modal_bg, 0);
    }
    put_cp(fb->cur, fb->w, fb->h, mx, my, 0x250C, theme->modal_border, theme->modal_bg, 0);
    put_cp(fb->cur, fb->w, fb->h, mx + mw - 1, my, 0x2510, theme->modal_border, theme->modal_bg, 0);
    put_cp(fb->cur, fb->w, fb->h, mx, my + mh - 1, 0x2514, theme->modal_border, theme->modal_bg, 0);
    put_cp(fb->cur, fb->w, fb->h, mx + mw - 1, my + mh - 1, 0x2518, theme->modal_border, theme->modal_bg, 0);

    const char *title = d->confirm_stage == 0
        ? " Delete? "
        : " Are you sure? ";
    draw_str(fb->cur, fb->w, fb->h, mx + 2, my, title,
             theme->modal_title, theme->modal_bg, LAPKA_ATTR_BOLD, mw - 4);

    char line[512];
    snprintf(line, sizeof(line), " %s", d->confirm_name);
    draw_str(fb->cur, fb->w, fb->h, mx + 2, my + 2, line,
             theme->title, theme->modal_bg, 0, mw - 4);

    const char *hint = d->confirm_stage == 0
        ? " [Y]es  [N]o "
        : " Really delete? [Y]es / [N]o ";
    draw_str(fb->cur, fb->w, fb->h, mx + 2, my + mh - 3, hint,
             theme->muted, theme->modal_bg, 0, mw - 4);
}

static void xplore_draw(void *ud, LapkaFramebuffer *fb) {
    ExploreData *d = ud;
    const LapkaTheme *theme = lapka_theme_active();

    if (d->mode == XPLORE_VIEW && d->pager) {
        char header[PATH_MAX + 16];
        snprintf(header, sizeof(header), " -- %s -- ",
                 d->entries[d->cursor].name);
        LapkaRect hr = { 0, 0, fb->w, 1 };
        lapka_draw_text(fb, hr, header, theme->title, 0, LAPKA_ATTR_BOLD);
        if (fb->h > 1) {
            LapkaCell *sub = fb->cur + (size_t)fb->w;
            d->pager->draw(d->pager, sub, fb->w, fb->h - 1, theme);
        }
    } else if (d->mode == XPLORE_EDIT && d->editor) {
        char header[PATH_MAX + 16];
        snprintf(header, sizeof(header), " -- Edit %s -- ",
                 d->entries[d->cursor].name);
        LapkaRect hr = { 0, 0, fb->w, 1 };
        lapka_draw_text(fb, hr, header, theme->title, 0, LAPKA_ATTR_BOLD);
        if (fb->h > 1) {
            LapkaCell *sub = fb->cur + (size_t)fb->w;
            d->editor->draw(d->editor, sub, fb->w, fb->h - 1, theme);
        }
    } else {
        draw_browser(fb, d, theme);
        if (d->mode == XPLORE_ACTION) draw_action_menu(fb, d, theme);
        if (d->mode == XPLORE_CONFIRM_DELETE) draw_confirm(fb, d, theme);
        if (d->mode == XPLORE_SEARCH && d->search) {
            LapkaCell *sub = fb->cur + (size_t)(fb->h - 1) * (size_t)fb->w;
            d->search->draw(d->search, sub, fb->w, 1, theme);
        }
    }

    if (d->toast) {
        int pw = 0, ph = 0;
        d->toast->measure(d->toast, fb->w - 4, fb->h - 4, &pw, &ph);
        if (pw < 20) pw = 20;
        if (pw > fb->w - 4) pw = fb->w - 4;
        int x = fb->w - pw - 2;
        int y = fb->h - ph - 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;

        LapkaRect want = { x, y, pw, ph };
        if (!d->toast_layer) {
            d->toast_layer = lapka_layer_new(want, 100,
                                             LAPKA_BLEND_TRANSPARENT,
                                             LAPKA_INPUT_NONE);
        } else if (want.w != d->toast_layer->rect.w ||
                   want.h != d->toast_layer->rect.h ||
                   want.x != d->toast_layer->rect.x ||
                   want.y != d->toast_layer->rect.y) {
            lapka_layer_resize(d->toast_layer, want);
        }
        if (d->toast_layer) {
            int tw = d->toast_layer->rect.w;
            int th = d->toast_layer->rect.h;
            for (int yy = 0; yy < th; yy++) {
                for (int xx = 0; xx < tw; xx++) {
                    d->toast_layer->buf[(size_t)yy * (size_t)tw + (size_t)xx] =
                        (LapkaCell){ 0, 0, 0, 0 };
                }
            }
            d->toast->draw(d->toast, d->toast_layer->buf, tw, th, theme);
            LapkaLayer *layers[1] = { d->toast_layer };
            lapka_composite(fb, layers, 1);
        }
    }
}

static void action_select(ExploreData *d) {
    int idx = d->filtered[d->cursor];
    Entry *e = &d->all_entries[idx];
    free(d->result);
    d->result = strdup(e->full);
    d->exit_code = 0;
    d->done = 1;
}

static void action_view(ExploreData *d) {
    int idx = d->filtered[d->cursor];
    Entry *e = &d->all_entries[idx];
    if (d->pager) lapka_primitive_free(d->pager);
    d->pager = lapka_pager_new();
    if (!d->pager) { show_toast(d, "Pager unavailable.", 2000); return; }
    if (lapka_pager_set_file(d->pager, e->full) != 0) {
        lapka_primitive_free(d->pager);
        d->pager = NULL;
        show_toast(d, "Cannot read file.", 2000);
        return;
    }
    lapka_pager_set_hint(d->pager,
        "/ find  n/N next/prev  \xe2\x86\x91\xe2\x86\x93 scroll  q quit");
    d->mode = XPLORE_VIEW;
}

static void action_edit(ExploreData *d) {
    int idx = d->filtered[d->cursor];
    Entry *e = &d->all_entries[idx];
    if (!process_is_root() && access(e->full, W_OK) != 0) {
        show_toast(d, "Read-only: cannot edit.", 2000);
        return;
    }
    if (d->editor) lapka_primitive_free(d->editor);
    d->editor = lapka_editor_new();
    if (!d->editor) { show_toast(d, "Editor unavailable.", 2000); return; }
    lapka_editor_open(d->editor, e->full);
    lapka_editor_set_hint(d->editor,
        "^S save  ^Z undo  ^Y redo  ^D delete-line  esc done");
    d->mode = XPLORE_EDIT;
}

static void action_delete(ExploreData *d) {
    int idx = d->filtered[d->cursor];
    Entry *e = &d->all_entries[idx];
    if (!permission_ok(e->full, 1)) {
        show_toast(d, "Permission denied.", 2000);
        return;
    }
    d->confirm_stage = 0;
    snprintf(d->confirm_target, sizeof(d->confirm_target), "%s", e->full);
    snprintf(d->confirm_name, sizeof(d->confirm_name), "%s", e->name);
    d->mode = XPLORE_CONFIRM_DELETE;
}

static void perform_delete(ExploreData *d) {
    int is_dir = 0;
    struct stat st;
    if (lstat(d->confirm_target, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;

    int rc = is_dir ? rmdir(d->confirm_target) : unlink(d->confirm_target);
    if (rc != 0) {
        show_toast(d, "Delete failed.", 2000);
        return;
    }
    show_toast(d, "Deleted.", 1500);

    char save[PATH_MAX];
    snprintf(save, sizeof(save), "%s", d->cwd);
    load_dir(d, save);
    d->mode = XPLORE_BROWSE;
}

static void apply_search(ExploreData *d, const char *query) {
    snprintf(d->search_query, sizeof(d->search_query), "%s",
             query ? query : "");
    rebuild_filter(d);
}

static bool browse_key(ExploreData *d, const LapkaEvent *ev) {
    if (ev->type != LAPKA_EVENT_KEY) return false;

    switch (ev->key.code) {
        case LAPKA_KEY_UP:
            if (d->cursor > 0) d->cursor--;
            return true;
        case LAPKA_KEY_DOWN:
            if (d->cursor + 1 < d->entry_count) d->cursor++;
            return true;
        case LAPKA_KEY_PAGEUP:
            d->cursor -= 10;
            if (d->cursor < 0) d->cursor = 0;
            return true;
        case LAPKA_KEY_PAGEDOWN:
            d->cursor += 10;
            if (d->cursor >= d->entry_count) d->cursor = d->entry_count - 1;
            if (d->cursor < 0) d->cursor = 0;
            return true;
        case LAPKA_KEY_HOME:
            d->cursor = 0;
            return true;
        case LAPKA_KEY_END:
            d->cursor = d->entry_count > 0 ? d->entry_count - 1 : 0;
            return true;
        case LAPKA_KEY_LEFT:
        case LAPKA_KEY_BACKSPACE: {
            char parent[PATH_MAX];
            parent_path(d->cwd, parent, sizeof(parent));
            if (strcmp(parent, d->cwd) != 0) load_dir(d, parent);
            return true;
        }
        case LAPKA_KEY_ENTER: {
            if (d->entry_count == 0) return true;
            int idx = d->filtered[d->cursor];
            Entry *e = &d->all_entries[idx];
            if (e->is_dir) {
                if (load_dir(d, e->full) != 0) {
                    show_toast(d, "Cannot enter directory.", 2000);
                }
                return true;
            }
            d->action_cursor = 0;
            d->mode = XPLORE_ACTION;
            return true;
        }
        case LAPKA_KEY_ESC:
            d->exit_code = 1;
            d->done = 1;
            return true;
        case LAPKA_KEY_CODEPOINT:
            if (ev->key.ch == '.') {
                d->show_hidden = !d->show_hidden;
                char save[PATH_MAX];
                snprintf(save, sizeof(save), "%s", d->cwd);
                load_dir(d, save);
                return true;
            }
            if (ev->key.ch == 's') {
                d->show_sizes = !d->show_sizes;
                return true;
            }
            if (ev->key.ch == '/') {
                if (!d->search) {
                    d->search = lapka_text_new("", false);
                    if (d->search) lapka_text_set_prefix(d->search, "/ ");
                }
                if (d->search) {
                    lapka_text_set(d->search, "");
                    d->mode = XPLORE_SEARCH;
                }
                return true;
            }
            if (ev->key.ch == 0x03) {
                d->exit_code = 1;
                d->done = 1;
                return true;
            }
            return false;
        default:
            return false;
    }
}

static bool action_key(ExploreData *d, const LapkaEvent *ev) {
    if (ev->type != LAPKA_EVENT_KEY) return false;

    Entry *e = d->cursor < d->entry_count ? &d->entries[d->cursor] : NULL;
    if (!e) { d->mode = XPLORE_BROWSE; return true; }

    int want_dir = d->want_dir && e->is_dir;
    int n = action_count(want_dir);

    if (ev->key.code == LAPKA_KEY_ESC) {
        d->mode = XPLORE_BROWSE;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_UP) {
        if (d->action_cursor > 0) d->action_cursor--;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_DOWN) {
        if (d->action_cursor + 1 < n) d->action_cursor++;
        return true;
    }
    if (ev->key.code == LAPKA_KEY_ENTER) {
        const char *label = mode_action_label(d->action_cursor, want_dir);
        if (!label) { d->mode = XPLORE_BROWSE; return true; }
        if (strcmp(label, "Select") == 0) { action_select(d); return true; }
        if (strcmp(label, "Select this directory") == 0) { action_select(d); return true; }
        if (strcmp(label, "Edit") == 0) { action_edit(d); return true; }
        if (strcmp(label, "View") == 0) { action_view(d); return true; }
        if (strcmp(label, "Delete") == 0) { action_delete(d); return true; }
        if (strcmp(label, "Enter") == 0) {
            if (load_dir(d, e->full) != 0) show_toast(d, "Cannot enter directory.", 2000);
            else d->mode = XPLORE_BROWSE;
            return true;
        }
        if (strcmp(label, "Cancel") == 0) { d->mode = XPLORE_BROWSE; return true; }
        d->mode = XPLORE_BROWSE;
        return true;
    }
    return false;
}

static bool confirm_key(ExploreData *d, const LapkaEvent *ev) {
    if (ev->type != LAPKA_EVENT_KEY) return false;
    if (ev->key.code != LAPKA_KEY_CODEPOINT) return false;

    char c = (char)ev->key.ch;
    if (c == 'y' || c == 'Y') {
        if (d->confirm_stage == 0) {
            d->confirm_stage = 1;
            return true;
        }
        perform_delete(d);
        return true;
    }
    if (c == 'n' || c == 'N' || ev->key.ch == 0x03) {
        d->mode = XPLORE_BROWSE;
        return true;
    }
    return false;
}

static bool search_key(ExploreData *d, const LapkaEvent *ev) {
    if (!d->search) return false;

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER || ev->key.code == LAPKA_KEY_ESC) {
            d->mode = XPLORE_BROWSE;
            return true;
        }
    }
    d->search->handle(d->search, ev);
    apply_search(d, lapka_text_value(d->search));
    return true;
}

static bool view_key(ExploreData *d, const LapkaEvent *ev) {
    if (!d->pager) return false;

    if (lapka_pager_is_searching(d->pager)) {
        d->pager->handle(d->pager, ev);
        return true;
    }

    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ESC) {
            d->mode = XPLORE_BROWSE;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_CODEPOINT &&
            (ev->key.ch == 'q' || ev->key.ch == 'Q')) {
            d->mode = XPLORE_BROWSE;
            return true;
        }
    }
    d->pager->handle(d->pager, ev);
    return true;
}

static bool edit_key(ExploreData *d, const LapkaEvent *ev) {
    if (!d->editor) return false;
    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ESC) {
            if (lapka_editor_is_dirty(d->editor)) {
                show_toast(d, "Unsaved changes. Ctrl-S to save.", 2000);
                return true;
            }
            d->mode = XPLORE_BROWSE;
            return true;
        }
        if (ev->key.code == LAPKA_KEY_CODEPOINT && ev->key.ch == 0x13) {
            int rc = lapka_editor_save(d->editor);
            show_toast(d, rc == 0 ? "Saved." : "Save failed.",
                       rc == 0 ? 1500 : 2000);
            return true;
        }
    }
    d->editor->handle(d->editor, ev);
    return true;
}

static bool xplore_handle(void *ud, const LapkaEvent *ev) {
    ExploreData *d = ud;

    if (d->toast && ev->type == LAPKA_EVENT_KEY) {
        lapka_primitive_free(d->toast);
        d->toast = NULL;
        if (d->toast_layer) {
            lapka_layer_free(d->toast_layer);
            d->toast_layer = NULL;
        }
    }

    switch (d->mode) {
        case XPLORE_BROWSE:          browse_key(d, ev); break;
        case XPLORE_ACTION:          action_key(d, ev); break;
        case XPLORE_CONFIRM_DELETE:  confirm_key(d, ev); break;
        case XPLORE_SEARCH:          search_key(d, ev); break;
        case XPLORE_VIEW:            view_key(d, ev); break;
        case XPLORE_EDIT:            edit_key(d, ev); break;
        default: break;
    }
    return d->done;
}

static bool xplore_tick(void *ud) {
    ExploreData *d = ud;
    if (d->toast && (now_ms() - d->toast_start_ms) >= d->toast_duration_ms) {
        lapka_primitive_free(d->toast);
        d->toast = NULL;
        if (d->toast_layer) {
            lapka_layer_free(d->toast_layer);
            d->toast_layer = NULL;
        }
    }
    return d->done;
}

int cmd_explorer(int argc, char **argv) {
    ExploreData d;
    memset(&d, 0, sizeof(d));
    d.show_hidden = true;
    d.show_sizes = true;

    const char *start = NULL;
    const char *title = "Explorer";

    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--dir") == 0) { d.want_dir = 1; continue; }
        if (strcmp(a, "--file") == 0) { d.want_dir = 0; continue; }
        if (a[0] == '-' && a[1] == '-' && a[2] != '\0') {
            fprintf(stderr, "tui: explorer: unknown flag: %s\n", a);
            return 3;
        }
        if (!start) start = a;
        else title = a;
    }

    d.title = title;

    if (!start) {
        const char *home = getenv("HOME");
        start = home && home[0] ? home : "/";
    }

    struct stat st;
    if (stat(start, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "tui: explorer: %s is not a directory\n", start);
        return 2;
    }

    if (load_dir(&d, start) != 0) {
        fprintf(stderr, "tui: explorer: cannot read %s\n", start);
        return 2;
    }

    LapkaInitOptions opts = { 1, 1, 1, 0 };
    LapkaRunCallbacks cb = { xplore_draw, xplore_handle, xplore_tick, &d };

    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && d.exit_code == 0 && d.result) {
        printf("%s\n", d.result);
        fflush(stdout);
    }

    if (d.search) lapka_primitive_free(d.search);
    if (d.rename) lapka_primitive_free(d.rename);
    if (d.pager) lapka_primitive_free(d.pager);
    if (d.editor) lapka_primitive_free(d.editor);
    if (d.toast) lapka_primitive_free(d.toast);
    if (d.toast_layer) lapka_layer_free(d.toast_layer);
    for (int i = 0; i < d.all_count; i++) {
        free(d.all_entries[i].name);
        free(d.all_entries[i].full);
    }
    free(d.all_entries);
    free(d.filtered);
    free(d.cwd);
    free(d.result);

    return rc != 0 ? rc : d.exit_code;
}