/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <lapka/theme.h>
#include <lapka/types.h>
#include <stdbool.h>

typedef enum {
    LAPKA_PRIMITIVE_LIST,
    LAPKA_PRIMITIVE_TEXT,
    LAPKA_PRIMITIVE_TABS,
    LAPKA_PRIMITIVE_PROGRESS,
    LAPKA_PRIMITIVE_SUMMARY,
    LAPKA_PRIMITIVE_TREE,
    LAPKA_PRIMITIVE_NOTIFICATION,
    LAPKA_PRIMITIVE_PAGER,
    LAPKA_PRIMITIVE_EDITOR
} LapkaPrimitiveKind;

typedef struct LapkaPrimitive {
    LapkaPrimitiveKind kind;
    void              *data;
    bool              (*handle)(struct LapkaPrimitive *p, const LapkaEvent *ev);
    void              (*draw)(struct LapkaPrimitive *p, LapkaCell *buf, int w, int h, const LapkaTheme *theme);
    void              (*measure)(struct LapkaPrimitive *p, int bound_w, int bound_h, int *out_w, int *out_h);
    void              (*free)(struct LapkaPrimitive *p);
} LapkaPrimitive;

void lapka_primitive_free(LapkaPrimitive *p);

typedef enum {
    LAPKA_SELECT_ONE,
    LAPKA_SELECT_MANY
} LapkaSelectMode;

typedef bool (*LapkaMatcher)(const char *item, const char *query);

bool lapka_filter_fuzzy(const char *item, const char *query);

typedef enum {
    LAPKA_CURSOR_GLYPH,
    LAPKA_CURSOR_INVERSE,
    LAPKA_CURSOR_BAR
} LapkaCursorStyle;

typedef enum {
    LAPKA_SCROLL_NONE,
    LAPKA_SCROLL_ARROWS,
    LAPKA_SCROLL_COUNTER,
    LAPKA_SCROLL_BOTH
} LapkaScrollStyle;

typedef struct {
    LapkaCursorStyle cursor_style;
    const char      *cursor_glyph;
    uint32_t         cursor_fg;
    uint32_t         cursor_bg;
    uint16_t         cursor_attrs;
    uint32_t         item_fg;
    uint32_t         item_bg;
    uint16_t         item_attrs;
    LapkaScrollStyle scroll;
    const char      *scroll_up;
    const char      *scroll_down;
} LapkaListStyle;

extern const LapkaListStyle LAPKA_LIST_STYLE_DEFAULT;

LapkaPrimitive *lapka_list_new(const char *const *items, int n, LapkaSelectMode mode);
void            lapka_list_free(LapkaPrimitive *p);
void            lapka_list_set_items(LapkaPrimitive *p, const char *const *items, int n);
void            lapka_list_set_style(LapkaPrimitive *p, const LapkaListStyle *style);
void            lapka_list_attach_filter(LapkaPrimitive *p, LapkaPrimitive *text);
void            lapka_list_set_matcher(LapkaPrimitive *p, LapkaMatcher m);
void            lapka_list_refilter(LapkaPrimitive *p);
const char     *lapka_list_result(LapkaPrimitive *p, int index);
int             lapka_list_result_count(LapkaPrimitive *p);
int             lapka_list_cursor(LapkaPrimitive *p);
const char     *lapka_list_cursor_item(LapkaPrimitive *p);

LapkaPrimitive *lapka_text_new(const char *initial, bool masked);
void            lapka_text_set(LapkaPrimitive *p, const char *value);
const char     *lapka_text_value(LapkaPrimitive *p);
void            lapka_text_on_change(LapkaPrimitive *p, void (*fn)(void *), void *ud);
void            lapka_text_set_prefix(LapkaPrimitive *p, const char *prefix);

LapkaPrimitive *lapka_tabs_new(const char **labels, int count);
void            lapka_tabs_set_child(LapkaPrimitive *p, int idx, LapkaPrimitive *child);
void            lapka_tabs_set_active(LapkaPrimitive *p, int idx);
int             lapka_tabs_active(LapkaPrimitive *p);
int             lapka_tabs_count(LapkaPrimitive *p);

LapkaPrimitive *lapka_progress_new(int total);
void            lapka_progress_set(LapkaPrimitive *p, int value);
int             lapka_progress_value(LapkaPrimitive *p);
int             lapka_progress_total(LapkaPrimitive *p);
void            lapka_progress_set_message(LapkaPrimitive *p, const char *msg);
const char     *lapka_progress_message(LapkaPrimitive *p);

LapkaPrimitive *lapka_summary_new(void);
void            lapka_summary_set(LapkaPrimitive *p, const char *text);
void            lapka_summary_append(LapkaPrimitive *p, const char *line);
int             lapka_summary_line_count(LapkaPrimitive *p);
int             lapka_summary_scroll(LapkaPrimitive *p);
void            lapka_summary_reset_scroll(LapkaPrimitive *p);

LapkaPrimitive *lapka_tree_new(void);
int             lapka_tree_add(LapkaPrimitive *p, int parent, const char *label);
void            lapka_tree_clear(LapkaPrimitive *p);
void            lapka_tree_set_userdata(LapkaPrimitive *p, int idx, void *ud);
void           *lapka_tree_userdata(LapkaPrimitive *p, int idx);
void            lapka_tree_set_expanded(LapkaPrimitive *p, int idx, bool expanded);
int             lapka_tree_cursor(LapkaPrimitive *p);
int             lapka_tree_cursor_index(LapkaPrimitive *p);
const char     *lapka_tree_cursor_label(LapkaPrimitive *p);
void           *lapka_tree_cursor_userdata(LapkaPrimitive *p);

LapkaPrimitive *lapka_notification_new(const char *message, int duration_ms);
void            lapka_notification_set(LapkaPrimitive *p, const char *message);
bool            lapka_notification_expired(LapkaPrimitive *p);

LapkaPrimitive *lapka_pager_new(void);
void            lapka_pager_set_lines(LapkaPrimitive *p, const char *const *lines, int count);
int             lapka_pager_set_file(LapkaPrimitive *p, const char *path);
void            lapka_pager_set_hint(LapkaPrimitive *p, const char *hint);
void            lapka_pager_set_search_enabled(LapkaPrimitive *p, bool enabled);
int             lapka_pager_line_count(LapkaPrimitive *p);
const char     *lapka_pager_line(LapkaPrimitive *p, int idx);
int             lapka_pager_scroll(LapkaPrimitive *p);
void            lapka_pager_reset_scroll(LapkaPrimitive *p);
bool            lapka_pager_is_searching(LapkaPrimitive *p);
int             lapka_pager_match_count(LapkaPrimitive *p);

LapkaPrimitive *lapka_editor_new(void);
int             lapka_editor_open(LapkaPrimitive *p, const char *path);
void            lapka_editor_set_text(LapkaPrimitive *p, const char *text);
char           *lapka_editor_text(LapkaPrimitive *p);
void            lapka_editor_set_path(LapkaPrimitive *p, const char *path);
const char     *lapka_editor_path(LapkaPrimitive *p);
bool            lapka_editor_is_dirty(LapkaPrimitive *p);
int             lapka_editor_save(LapkaPrimitive *p);
void            lapka_editor_set_hint(LapkaPrimitive *p, const char *hint);
int             lapka_editor_cursor_row(LapkaPrimitive *p);
int             lapka_editor_cursor_col(LapkaPrimitive *p);
void            lapka_editor_set_cursor_visible(LapkaPrimitive *p, bool visible);
