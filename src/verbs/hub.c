/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include "hub.h"

#include <lapka/lapka.h>
#include <lapka/layer.h>
#include <lapka/primitive.h>
#include <render/draw.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define HUB_MAX_LAYERS 8
#define SIDEBAR_W 22
#define MODAL_W 44
#define MODAL_H 7

typedef enum {
    HUB_LAYER_EDITOR,
    HUB_LAYER_MODAL,
    HUB_LAYER_NESTED,
} HubLayerKind;

typedef enum {
    MODAL_QUIT,
} ModalKind;

typedef struct HubData HubData;
typedef struct HubLayerEntry HubLayerEntry;

struct HubLayerEntry {
    HubData      *hub;
    HubLayerKind  kind;
    LapkaLayer   *layer;
    void         *data;
    int           result;

    bool        (*handle)(HubLayerEntry *e, const LapkaEvent *ev);
    void        (*commit)(HubLayerEntry *e);
    void        (*cancel)(HubLayerEntry *e);
};

struct HubData {
    LapkaHubDef *def;

    int  cat_idx;
    int  item_idx;

    int  screen_w;
    int  screen_h;

    HubLayerEntry *stack[HUB_MAX_LAYERS];
    int            stack_size;

    int  exit_code;
    int  done;
};

typedef struct {
    LapkaHubItem   *item;
    LapkaPrimitive *primitive;
} EditorData;

typedef struct {
    ModalKind kind;
    char      message[256];
} ModalData;

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static LapkaHubItem *item_find(HubData *d, const char *key) {
    if (!key) return NULL;
    for (int c = 0; c < d->def->category_count; c++) {
        LapkaHubCategory *cat = &d->def->categories[c];
        for (int i = 0; i < cat->item_count; i++) {
            if (strcmp(cat->items[i].key, key) == 0) return &cat->items[i];
        }
    }
    return NULL;
}

static const char *value_get(HubData *d, const char *key) {
    LapkaHubItem *it = item_find(d, key);
    return it ? it->value : NULL;
}

static void hub_unset_hidden(HubData *d);
static void value_set(HubData *d, const char *key, const char *value) {
    LapkaHubItem *it = item_find(d, key);
    if (!it) return;
    free(it->value);
    it->value = xstrdup(value ? value : "");
    hub_unset_hidden(d);
}

static int visible_if_matches(HubData *d, LapkaHubItem *it) {
    if (!it->visible_if_key || !it->visible_if_values) return 1;
    const char *v = value_get(d, it->visible_if_key);
    if (!v) return 0;
    const char *list = it->visible_if_values;
    while (*list) {
        const char *comma = strchr(list, ',');
        size_t n = comma ? (size_t)(comma - list) : strlen(list);
        if (strlen(v) == n && strncmp(list, v, n) == 0) return 1;
        if (!comma) break;
        list = comma + 1;
    }
    return 0;
}

static int visible_item_count(HubData *d, LapkaHubCategory *cat) {
    int n = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (visible_if_matches(d, &cat->items[i])) n++;
    }
    return n;
}

static LapkaHubItem *item_at(HubData *d, int vis_idx) {
    LapkaHubCategory *cat = &d->def->categories[d->cat_idx];
    int n = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (!visible_if_matches(d, &cat->items[i])) continue;
        if (n == vis_idx) return &cat->items[i];
        n++;
    }
    return NULL;
}

static void hub_draw_full(HubData *d, LapkaFramebuffer *fb,
                          const LapkaTheme *theme);
static bool hub_dispatch_event(HubData *d, const LapkaEvent *ev);
static void hub_free_stack(HubData *d);

static char **choices_from_command(const char *cmd, int *count_out) {
    FILE *fp = popen(cmd, "r");
    if (!fp) { *count_out = 0; return NULL; }

    int cap = 16, n = 0;
    char **items = malloc((size_t)cap * sizeof(char *));
    if (!items) { pclose(fp); *count_out = 0; return NULL; }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        if (n >= cap) {
            cap *= 2;
            char **ni = realloc(items, (size_t)cap * sizeof(char *));
            if (!ni) break;
            items = ni;
        }
        items[n++] = xstrdup(line);
    }
    pclose(fp);
    *count_out = n;
    return items;
}

static LapkaPrimitive *make_editor(LapkaHubItem *item) {
    const char *current = item->value;

    switch (item->widget) {
        case LAPKA_WIDGET_MENU: {
            const char **choices;
            int count;
            char **tmp = NULL;

            if (item->choices_cmd) {
                tmp = choices_from_command(item->choices_cmd, &count);
                choices = (const char **)tmp;
            } else {
                choices = (const char **)item->choices;
                count = item->choice_count;
            }

            LapkaPrimitive *p = lapka_list_new(choices, count, LAPKA_SELECT_ONE);

            if (tmp) {
                for (int i = 0; i < count; i++) free(tmp[i]);
                free(tmp);
            }

            if (!p) return NULL;

            if (current && current[0]) {
                for (int i = 0; i < count; i++) {
                    if (strcmp(choices ? choices[i] : "", current) == 0) {
                        for (int k = 0; k < i; k++) {
                            LapkaEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = LAPKA_EVENT_KEY;
                            ev.key.code = LAPKA_KEY_DOWN;
                            p->handle(p, &ev);
                        }
                        break;
                    }
                }
            }
            return p;
        }
        case LAPKA_WIDGET_INPUT: {
            LapkaPrimitive *p = lapka_text_new(current ? current : "", false);
            if (p) lapka_text_set_prefix(p, "> ");
            return p;
        }
        case LAPKA_WIDGET_PASSWORD: {
            LapkaPrimitive *p = lapka_text_new("", true);
            if (p) lapka_text_set_prefix(p, "> ");
            return p;
        }
        case LAPKA_WIDGET_YESNO: {
            static const char *const yn[] = { "yes", "no" };
            LapkaPrimitive *p = lapka_list_new(yn, 2, LAPKA_SELECT_ONE);
            if (!p) return NULL;
            if (current && strcmp(current, "no") == 0) {
                LapkaEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = LAPKA_EVENT_KEY;
                ev.key.code = LAPKA_KEY_DOWN;
                p->handle(p, &ev);
            }
            return p;
        }
        case LAPKA_WIDGET_MULTISELECT: {
            const char **choices;
            int count;
            char **tmp = NULL;

            if (item->choices_cmd) {
                tmp = choices_from_command(item->choices_cmd, &count);
                choices = (const char **)tmp;
            } else {
                choices = (const char **)item->choices;
                count = item->choice_count;
            }

            LapkaPrimitive *p = lapka_list_new(choices, count, LAPKA_SELECT_MANY);

            if (tmp) {
                for (int i = 0; i < count; i++) free(tmp[i]);
                free(tmp);
            }
            return p;
        }
        default:
            return NULL;
    }
}

static void editor_draw(LapkaLayer *l, const LapkaTheme *theme) {
    EditorData *ed = l->userdata;
    int w = l->rect.w, h = l->rect.h;
    LapkaFramebuffer fb = { w, h, l->buf, NULL, true };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            LapkaCell c = { ' ', 0, theme->modal_bg, 0 };
            l->buf[(size_t)y * (size_t)w + (size_t)x] = c;
        }
    }

    lapka_draw_box(&fb, (LapkaRect){ 0, 0, w, h },
                   LAPKA_BORDER_ROUNDED,
                   theme->modal_border, theme->modal_bg);

    char title[256];
    snprintf(title, sizeof(title), " %s ", ed->item->label);
    LapkaRect tr = { 2, 0, w - 4, 1 };
    lapka_draw_text(&fb, tr, title,
                    theme->modal_title, theme->modal_bg, LAPKA_ATTR_BOLD);

    int inner_x = 2;
    int inner_y = 2;
    int inner_w = w - 4;
    int inner_h = h - inner_y - 2;
    if (inner_w < 0) inner_w = 0;
    if (inner_h < 0) inner_h = 0;

    if (inner_w > 0 && inner_h > 0) {
        size_t total = (size_t)inner_w * (size_t)inner_h;
        LapkaCell *tmp = calloc(total, sizeof(LapkaCell));
        if (tmp) {
            ed->primitive->draw(ed->primitive, tmp, inner_w, inner_h, theme);
            for (int y = 0; y < inner_h; y++) {
                LapkaCell *dst = l->buf
                    + (size_t)(inner_y + y) * (size_t)w
                    + (size_t)inner_x;
                memcpy(dst, tmp + (size_t)y * (size_t)inner_w,
                       (size_t)inner_w * sizeof(LapkaCell));
            }
            free(tmp);
        }
    }

    LapkaRect hr = { 2, h - 2, w - 4, 1 };
    lapka_draw_text(&fb, hr, "enter commit  esc cancel",
                    theme->muted, theme->modal_bg, 0);
}

static bool editor_handle(HubLayerEntry *e, const LapkaEvent *ev) {
    EditorData *ed = e->data;
    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER) { e->result = 0; return true; }
        if (ev->key.code == LAPKA_KEY_ESC) { e->result = 1; return true; }
        if (ev->key.code == LAPKA_KEY_CODEPOINT &&
            ev->key.ch == 0x03 && (ev->key.mods & LAPKA_MOD_CTRL)) {
            e->result = 1;
            return true;
        }
    }
    ed->primitive->handle(ed->primitive, ev);
    return false;
}

static void hub_unset_hidden(HubData *d) {
    for (int c = 0; c < d->def->category_count; c++) {
        LapkaHubCategory *cat = &d->def->categories[c];
        for (int i = 0; i < cat->item_count; i++) {
            LapkaHubItem *it = &cat->items[i];
            if (!it->visible_if_key) continue;
            if (!visible_if_matches(d, it)) {
                if (it->value && it->value[0]) {
                    free(it->value);
                    it->value = xstrdup("");
                }
            }
        }
    }
}

static void editor_commit(HubLayerEntry *e) {
    EditorData *ed = e->data;
    HubData *d = e->hub;

    switch (ed->item->widget) {
        case LAPKA_WIDGET_MENU:
        case LAPKA_WIDGET_YESNO: {
            const char *v = lapka_list_cursor_item(ed->primitive);
            if (v) value_set(d, ed->item->key, v);
            return;
        }
        case LAPKA_WIDGET_INPUT:
        case LAPKA_WIDGET_PASSWORD: {
            const char *v = lapka_text_value(ed->primitive);
            if (v) value_set(d, ed->item->key, v);
            return;
        }
        case LAPKA_WIDGET_MULTISELECT: {
            char buf[4096];
            buf[0] = '\0';
            int n = lapka_list_result_count(ed->primitive);
            for (int i = 0; i < n; i++) {
                const char *it = lapka_list_result(ed->primitive, i);
                if (!it) continue;
                if (buf[0]) strncat(buf, ",", sizeof(buf) - strlen(buf) - 1);
                strncat(buf, it, sizeof(buf) - strlen(buf) - 1);
            }
            value_set(d, ed->item->key, buf);
            return;
        }
        default:
            return;
    }
}

static void editor_cancel(HubLayerEntry *e) {
    (void)e;
}

static LapkaRect editor_compute_rect(HubData *d, HubLayerEntry *e) {
    EditorData *ed = e->data;
    int sw = d->screen_w, sh = d->screen_h;
    int pw = 0, ph = 0;
    ed->primitive->measure(ed->primitive, sw - 10, sh - 10, &pw, &ph);

    int w = pw + 6;
    if (w < 34) w = 34;
    if (w > sw - 4) w = sw - 4;

    int h = ph + 6;
    int max_h = sh * 70 / 100;
    if (h > max_h) h = max_h;
    if (h < 8) h = 8;
    if (h > sh - 4) h = sh - 4;

    LapkaRect r = { (sw - w) / 2, (sh - h) / 2, w, h };
    return r;
}

extern void _hub_nested_setup(LapkaLayer *l, HubLayerEntry *e);

static void hub_push_subhub(HubData *d, LapkaHubItem *item) {
    if (!item->subhub) return;

    char resolved[PATH_MAX];
    if (item->subhub[0] == '/') {
        snprintf(resolved, sizeof(resolved), "%s", item->subhub);
    } else {
        const char *base = d->def->dir ? d->def->dir : ".";
        snprintf(resolved, sizeof(resolved), "%s/%s", base, item->subhub);
    }

    LapkaHubDef *sub_def = lapka_hub_parse_file(resolved);
    if (!sub_def) return;

    HubData *sub = calloc(1, sizeof(*sub));
    if (!sub) { lapka_hub_def_free(sub_def); return; }
    sub->def = sub_def;
    sub->screen_w = d->screen_w;
    sub->screen_h = d->screen_h;

    HubLayerEntry *e = calloc(1, sizeof(*e));
    if (!e) {
        lapka_hub_def_free(sub_def);
        free(sub);
        return;
    }

    e->hub = d;
    e->kind = HUB_LAYER_NESTED;

    int sw = d->screen_w, sh = d->screen_h;
    LapkaRect r = { 0, 0, sw, sh };

    e->layer = lapka_layer_new(r, 30, LAPKA_BLEND_OPAQUE, LAPKA_INPUT_TRAP);
    if (!e->layer) {
        lapka_hub_def_free(sub_def);
        free(sub);
        free(e);
        return;
    }
    e->layer->userdata = sub;

    _hub_nested_setup(e->layer, e);

    if (d->stack_size < HUB_MAX_LAYERS) {
        d->stack[d->stack_size++] = e;
    } else {
        lapka_layer_free(e->layer);
        lapka_hub_def_free(sub_def);
        free(sub);
        free(e);
    }
}

static void hub_push_editor(HubData *d, LapkaHubItem *item) {
    if (item->widget == LAPKA_WIDGET_SUBHUB) {
        hub_push_subhub(d, item);
        return;
    }

    LapkaPrimitive *prim = make_editor(item);
    if (!prim) return;

    HubLayerEntry *e = calloc(1, sizeof(*e));
    if (!e) { lapka_primitive_free(prim); return; }

    EditorData *ed = calloc(1, sizeof(*ed));
    if (!ed) { free(e); lapka_primitive_free(prim); return; }
    ed->item = item;
    ed->primitive = prim;

    e->hub = d;
    e->kind = HUB_LAYER_EDITOR;
    e->data = ed;
    e->handle = editor_handle;
    e->commit = editor_commit;
    e->cancel = editor_cancel;

    LapkaRect r = editor_compute_rect(d, e);
    e->layer = lapka_layer_new(r, 10, LAPKA_BLEND_OPAQUE, LAPKA_INPUT_TRAP);
    if (!e->layer) { free(ed); free(e); lapka_primitive_free(prim); return; }
    e->layer->draw = editor_draw;
    e->layer->userdata = ed;

    if (d->stack_size < HUB_MAX_LAYERS) {
        d->stack[d->stack_size++] = e;
    } else {
        lapka_layer_free(e->layer);
        free(ed);
        free(e);
        lapka_primitive_free(prim);
    }
}

static void modal_draw(LapkaLayer *l, const LapkaTheme *theme) {
    ModalData *md = l->userdata;
    int w = l->rect.w, h = l->rect.h;
    LapkaFramebuffer fb = { w, h, l->buf, NULL, true };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            LapkaCell c = { ' ', 0, theme->modal_bg, 0 };
            l->buf[(size_t)y * (size_t)w + (size_t)x] = c;
        }
    }

    lapka_draw_box(&fb, (LapkaRect){ 0, 0, w, h },
                   LAPKA_BORDER_ROUNDED,
                   theme->modal_border, theme->modal_bg);

    LapkaRect mr = { 2, 2, w - 4, 1 };
    lapka_draw_text(&fb, mr, md->message,
                    theme->modal_title, theme->modal_bg, LAPKA_ATTR_BOLD);

    LapkaRect hr = { 2, h - 3, w - 4, 1 };
    lapka_draw_text(&fb, hr, "[Y]es  [N]o",
                    theme->muted, theme->modal_bg, 0);
}

static bool modal_handle(HubLayerEntry *e, const LapkaEvent *ev) {
    if (ev->type != LAPKA_EVENT_KEY) return false;

    if (ev->key.code == LAPKA_KEY_CODEPOINT) {
        if (ev->key.ch == 'y' || ev->key.ch == 'Y' ||
            ev->key.ch == '\r' || ev->key.ch == '\n') {
            e->result = 0;
            return true;
        }
        if (ev->key.ch == 'n' || ev->key.ch == 'N' || ev->key.ch == 0x03) {
            e->result = 1;
            return true;
        }
    }
    if (ev->key.code == LAPKA_KEY_ESC) {
        e->result = 1;
        return true;
    }
    return false;
}

static void modal_commit(HubLayerEntry *e) {
    ModalData *md = e->data;
    HubData *d = e->hub;
    if (md->kind == MODAL_QUIT) {
        d->exit_code = 1;
        d->done = 1;
    }
}

static void modal_cancel(HubLayerEntry *e) {
    (void)e;
}

static void hub_push_modal(HubData *d, ModalKind kind, const char *message) {
    HubLayerEntry *e = calloc(1, sizeof(*e));
    if (!e) return;

    ModalData *md = calloc(1, sizeof(*md));
    if (!md) { free(e); return; }
    md->kind = kind;
    snprintf(md->message, sizeof(md->message), "%s", message);

    e->hub = d;
    e->kind = HUB_LAYER_MODAL;
    e->data = md;
    e->handle = modal_handle;
    e->commit = modal_commit;
    e->cancel = modal_cancel;

    int sw = d->screen_w, sh = d->screen_h;
    int w = MODAL_W, h = MODAL_H;
    if (w > sw - 4) w = sw - 4;
    if (h > sh - 4) h = sh - 4;
    LapkaRect r = { (sw - w) / 2, (sh - h) / 2, w, h };

    e->layer = lapka_layer_new(r, 20, LAPKA_BLEND_OPAQUE, LAPKA_INPUT_TRAP);
    if (!e->layer) { free(md); free(e); return; }
    e->layer->draw = modal_draw;
    e->layer->userdata = md;

    if (d->stack_size < HUB_MAX_LAYERS) {
        d->stack[d->stack_size++] = e;
    } else {
        lapka_layer_free(e->layer);
        free(md);
        free(e);
    }
}

static void nested_draw(LapkaLayer *l, const LapkaTheme *theme) {
    HubData *sub = l->userdata;
    sub->screen_w = l->rect.w;
    sub->screen_h = l->rect.h;
    LapkaFramebuffer fb = { l->rect.w, l->rect.h, l->buf, NULL, true };
    hub_draw_full(sub, &fb, theme);
}

static bool nested_handle(HubLayerEntry *e, const LapkaEvent *ev) {
    HubData *sub = e->layer->userdata;
    hub_dispatch_event(sub, ev);
    if (sub->done) {
        e->result = sub->exit_code;
        return true;
    }
    return false;
}

static void nested_commit(HubLayerEntry *e) {
    HubData *sub = e->layer->userdata;
    HubData *parent = e->hub;
    for (int c = 0; c < sub->def->category_count; c++) {
        LapkaHubCategory *cat = &sub->def->categories[c];
        for (int i = 0; i < cat->item_count; i++) {
            value_set(parent, cat->items[i].key, cat->items[i].value);
        }
    }
}

static void nested_cancel(HubLayerEntry *e) {
    (void)e;
}

void _hub_nested_setup(LapkaLayer *l, HubLayerEntry *e) {
    l->draw = nested_draw;
    e->handle = nested_handle;
    e->commit = nested_commit;
    e->cancel = nested_cancel;
}

static void entry_free_data(HubLayerEntry *e) {
    switch (e->kind) {
        case HUB_LAYER_EDITOR: {
            EditorData *ed = e->data;
            if (ed) {
                if (ed->primitive) lapka_primitive_free(ed->primitive);
                free(ed);
            }
            break;
        }
        case HUB_LAYER_MODAL:
            free(e->data);
            break;
        case HUB_LAYER_NESTED: {
            HubData *sub = e->layer ? e->layer->userdata : NULL;
            if (sub) {
                hub_free_stack(sub);
                lapka_hub_def_free(sub->def);
                free(sub);
                e->layer->userdata = NULL;
            }
            break;
        }
    }
    e->data = NULL;
}

static void hub_pop_entry(HubData *d, int commit) {
    if (d->stack_size == 0) return;
    HubLayerEntry *e = d->stack[--d->stack_size];

    if (commit) {
        if (e->commit) e->commit(e);
    } else {
        if (e->cancel) e->cancel(e);
    }

    entry_free_data(e);
    if (e->layer) lapka_layer_free(e->layer);
    free(e);
}

static void hub_free_stack(HubData *d) {
    while (d->stack_size > 0) {
        HubLayerEntry *e = d->stack[--d->stack_size];
        entry_free_data(e);
        if (e->layer) lapka_layer_free(e->layer);
        free(e);
    }
}

static void draw_hub_body(HubData *d, LapkaFramebuffer *fb,
                          const LapkaTheme *theme) {
    LapkaHubDef *def = d->def;

    char header[512];
    snprintf(header, sizeof(header), "-- %s --",
             def->title ? def->title : "Hub");
    LapkaRect hr = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, hr, header, theme->title, 0, LAPKA_ATTR_BOLD);

    int top = 2;
    int bottom = fb->h - 1;
    if (bottom < top) return;

    int sidebar_w = SIDEBAR_W;
    if (sidebar_w > fb->w / 3) sidebar_w = fb->w / 3;

    for (int y = top; y < bottom; y++) {
        LapkaCell sep = { 0x2502, theme->border, 0, 0 };
        lapka_fb_put(fb, sidebar_w, y, sep);
    }

    for (int i = 0; i < def->category_count; i++) {
        int y = top + i;
        if (y >= bottom) break;
        int sel = (i == d->cat_idx);
        char line[256];
        snprintf(line, sizeof(line), "%s %s",
                 sel ? ">" : " ", def->categories[i].label);
        LapkaRect r = { 0, y, sidebar_w, 1 };
        lapka_draw_text(fb, r, line,
                        sel ? theme->accent : 0, 0,
                        sel ? LAPKA_ATTR_BOLD : 0);
    }

    LapkaHubCategory *cat = &def->categories[d->cat_idx];
    int cx = sidebar_w + 2;
    int cw = fb->w - cx;
    if (cw < 0) cw = 0;

    int vis_count = visible_item_count(d, cat);
    int row = top;
    for (int i = 0; i < vis_count && row < bottom; i++) {
        LapkaHubItem *item = item_at(d, i);
        if (!item) break;
        const char *current = item->value;
        if (!current) current = "";

        int sel = (i == d->item_idx);
        char line[512];
        snprintf(line, sizeof(line), "%s %s: %s",
                 sel ? ">" : " ", item->label,
                 current[0] ? current : "(unset)");
        LapkaRect r = { cx, row, cw, 1 };
        lapka_draw_text(fb, r, line,
                        sel ? theme->accent : 0, 0,
                        sel ? LAPKA_ATTR_BOLD : 0);
        row++;
    }

    char actions[512];
    actions[0] = '\0';
    for (int i = 0; i < def->action_count; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "F%d:%s  ", i + 1, def->actions[i]);
        strncat(actions, tmp, sizeof(actions) - strlen(actions) - 1);
    }
    strncat(actions,
            "\xe2\x86\x91\xe2\x86\x93 move  enter edit  tab category  esc quit",
            sizeof(actions) - strlen(actions) - 1);

    LapkaRect fr = { 0, fb->h - 1, fb->w, 1 };
    lapka_draw_text(fb, fr, actions, theme->muted, 0, 0);
}

static void hub_draw_full(HubData *d, LapkaFramebuffer *fb,
                          const LapkaTheme *theme) {
    d->screen_w = fb->w;
    d->screen_h = fb->h;

    draw_hub_body(d, fb, theme);

    if (d->stack_size == 0) return;

    LapkaLayer *layers[HUB_MAX_LAYERS];
    for (int i = 0; i < d->stack_size; i++) {
        HubLayerEntry *e = d->stack[i];
        LapkaRect want;
        switch (e->kind) {
            case HUB_LAYER_EDITOR:
                want = editor_compute_rect(d, e);
                break;
            case HUB_LAYER_MODAL: {
                int w = MODAL_W, h = MODAL_H;
                if (w > d->screen_w - 4) w = d->screen_w - 4;
                if (h > d->screen_h - 4) h = d->screen_h - 4;
                want = (LapkaRect){ (d->screen_w - w) / 2,
                                    (d->screen_h - h) / 2, w, h };
                break;
            }
            case HUB_LAYER_NESTED:
                want = (LapkaRect){ 0, 0, d->screen_w, d->screen_h };
                break;
            default:
                want = e->layer->rect;
        }
        if (want.w != e->layer->rect.w || want.h != e->layer->rect.h ||
            want.x != e->layer->rect.x || want.y != e->layer->rect.y) {
            if (want.w > 0 && want.h > 0) lapka_layer_resize(e->layer, want);
        }
        if (e->layer->draw) e->layer->draw(e->layer, theme);
        layers[i] = e->layer;
    }

    lapka_composite(fb, layers, d->stack_size);
}

static void move_item(HubData *d, int delta) {
    LapkaHubCategory *cat = &d->def->categories[d->cat_idx];
    int n = visible_item_count(d, cat);
    if (n == 0) { d->item_idx = 0; return; }
    int next = d->item_idx + delta;
    if (next < 0) next = 0;
    if (next >= n) next = n - 1;
    d->item_idx = next;
}

static void move_category(HubData *d, int delta) {
    int n = d->def->category_count;
    if (n == 0) return;
    int next = d->cat_idx + delta;
    if (next < 0) next = n - 1;
    if (next >= n) next = 0;
    d->cat_idx = next;
    d->item_idx = 0;
}

static bool hub_handle_browsing(HubData *d, const LapkaEvent *ev) {
    if (ev->type != LAPKA_EVENT_KEY) return false;

    if (ev->key.code == LAPKA_KEY_ESC) {
        hub_push_modal(d, MODAL_QUIT, "Quit without saving?");
        return false;
    }
    if (ev->key.code == LAPKA_KEY_UP) { move_item(d, -1); return false; }
    if (ev->key.code == LAPKA_KEY_DOWN) { move_item(d, 1); return false; }
    if (ev->key.code == LAPKA_KEY_LEFT) { move_category(d, -1); return false; }
    if (ev->key.code == LAPKA_KEY_RIGHT ||
        ev->key.code == LAPKA_KEY_TAB) { move_category(d, 1); return false; }
    if (ev->key.code == LAPKA_KEY_ENTER) {
        LapkaHubItem *item = item_at(d, d->item_idx);
        if (item) hub_push_editor(d, item);
        return false;
    }
    if (ev->key.code >= LAPKA_KEY_F1 && ev->key.code <= LAPKA_KEY_F12) {
        int idx = ev->key.code - LAPKA_KEY_F1;
        if (idx >= d->def->action_count) return false;
        const char *action = d->def->actions[idx];
        if (strcmp(action, "Proceed") == 0) {
            d->exit_code = 0;
            d->done = 1;
            return true;
        }
        if (strcmp(action, "Back") == 0) {
            d->exit_code = 1;
            d->done = 1;
            return true;
        }
    }
    return false;
}

static bool hub_dispatch_event(HubData *d, const LapkaEvent *ev) {
    if (d->stack_size > 0) {
        HubLayerEntry *e = d->stack[d->stack_size - 1];
        if (e->handle && e->handle(e, ev)) {
            int commit = (e->result == 0);
            hub_pop_entry(d, commit);
            return d->done;
        }
        return false;
    }
    return hub_handle_browsing(d, ev);
}

static void hub_run_draw(void *ud, LapkaFramebuffer *fb) {
    HubData *d = ud;
    hub_draw_full(d, fb, lapka_theme_active());
}

static bool hub_run_handle(void *ud, const LapkaEvent *ev) {
    HubData *d = ud;
    return hub_dispatch_event(d, ev);
}

static int check_secure_file(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    if (!S_ISREG(st.st_mode)) return -1;
    if (st.st_uid != 0) return -1;
    if ((st.st_mode & 0777) & ~0600) return -1;
    return 0;
}

int cmd_hub(int argc, char **argv) {
    const char *in_path = NULL;
    const char *out_path = NULL;
    int check = 0;

    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--check") == 0) { check = 1; continue; }
        if (strncmp(a, "--in=", 5) == 0) { in_path = a + 5; continue; }
        if (strcmp(a, "--in") == 0 && i + 1 < argc) { in_path = argv[++i]; continue; }
        if (strncmp(a, "--out=", 6) == 0) { out_path = a + 6; continue; }
        if (strcmp(a, "--out") == 0 && i + 1 < argc) { out_path = argv[++i]; continue; }
        fprintf(stderr, "tui: hub: unknown argument: %s\n", a);
        return 3;
    }

    if (!in_path || !out_path) {
        fprintf(stderr, "tui: hub: --in and --out are required\n");
        return 3;
    }

    if (check) {
        if (check_secure_file(in_path) != 0) {
            fprintf(stderr,
                    "tui: hub: --in is not a root-owned 0600 regular file\n");
            return 2;
        }
        if (check_secure_file(out_path) != 0) {
            fprintf(stderr,
                    "tui: hub: --out is not a root-owned 0600 regular file\n");
            return 2;
        }
    }

    FILE *in = fopen(in_path, "r");
    if (!in) {
        fprintf(stderr, "tui: hub: cannot read %s\n", in_path);
        return 2;
    }
    LapkaHubDef *def = lapka_hub_parse(in);
    fclose(in);
    if (!def) {
        fprintf(stderr, "tui: hub: failed to parse %s\n", in_path);
        return 2;
    }

    HubData d;
    memset(&d, 0, sizeof(d));
    d.def = def;
    d.screen_w = 80;
    d.screen_h = 24;

    LapkaInitOptions opts = { 1, 1, 1, 0 };
    LapkaRunCallbacks cb = { hub_run_draw, hub_run_handle, NULL, &d };

    int rc = lapka_run(&opts, &cb, -1);

    int code = (rc != 0) ? rc : d.exit_code;

    if (code == 0) {
        FILE *out = fopen(out_path, "w");
        if (!out) {
            fprintf(stderr, "tui: hub: cannot write %s\n", out_path);
            code = 2;
        } else {
            lapka_hub_serialize(def, out);
            fclose(out);
        }
    }

    hub_free_stack(&d);
    lapka_hub_def_free(def);

    return code;
}