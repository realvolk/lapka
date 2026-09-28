# lapka — API reference

Every public symbol, grouped by header. The spec is in `docs/spec.md`.

---

## `lapka.h`

Umbrella header. Includes everything below.

```c
#define LAPKA_VERSION "0.1.0"
```

---

## `types.h`

### Cell

```c
typedef struct {
    uint32_t cp;
    uint32_t fg;
    uint32_t bg;
    uint16_t attrs;
} LapkaCell;
```

`cp` is a Unicode codepoint. `0` means empty.
`fg` and `bg` are `0xRRGGBB`. `0` means terminal default.

Attribute bits:

```
LAPKA_ATTR_BOLD      0x0001
LAPKA_ATTR_DIM       0x0002
LAPKA_ATTR_ITALIC    0x0004
LAPKA_ATTR_UNDERLINE 0x0008
LAPKA_ATTR_BLINK     0x0010
LAPKA_ATTR_REVERSE   0x0020
LAPKA_ATTR_STRIKE    0x0040
```

### Rect

```c
typedef struct { int x, y, w, h; } LapkaRect;
```

`w` and `h` are cell counts.

### Size

```c
typedef enum {
    LAPKA_SIZE_FIT,
    LAPKA_SIZE_FULL,
    LAPKA_SIZE_FIXED,
    LAPKA_SIZE_PERCENT
} LapkaSizeMode;

typedef struct {
    LapkaSizeMode mode;
    int           val;
} LapkaSize;
```

`FIT` clips to parent. `FULL` equals the parent. `FIXED` is exactly `val`. `PERCENT` is `parent * val / 100`, clamped to `[0, 100]`.

### Anchor

```c
typedef enum {
    LAPKA_ANCHOR_TL, LAPKA_ANCHOR_TC, LAPKA_ANCHOR_TR,
    LAPKA_ANCHOR_LC, LAPKA_ANCHOR_CC, LAPKA_ANCHOR_RC,
    LAPKA_ANCHOR_BL, LAPKA_ANCHOR_BC, LAPKA_ANCHOR_BR
} LapkaAnchor;
```

### Border

```c
typedef enum {
    LAPKA_BORDER_NONE,
    LAPKA_BORDER_SINGLE,
    LAPKA_BORDER_ROUNDED,
    LAPKA_BORDER_DOUBLE
} LapkaBorder;
```

### Box

```c
typedef struct {
    LapkaAnchor anchor;
    LapkaSize   width;
    LapkaSize   height;
    LapkaBorder border;
    int         pad[4];
    int         margin[4];
} LapkaBox;
```

`pad` and `margin` are `[top, right, bottom, left]`.

```c
LapkaRect lapka_place(LapkaRect parent, LapkaBox box,
                      int natural_w, int natural_h);
```

Pure function. No allocation. `natural_w` and `natural_h` are used when the corresponding size mode is `FIT`.

### Events

```c
typedef enum {
    LAPKA_EVENT_NONE,
    LAPKA_EVENT_KEY,
    LAPKA_EVENT_MOUSE,
    LAPKA_EVENT_PASTE,
    LAPKA_EVENT_RESIZE,
    LAPKA_EVENT_FOCUS_IN,
    LAPKA_EVENT_FOCUS_OUT,
    LAPKA_EVENT_QUIT
} LapkaEventType;

typedef enum {
    LAPKA_KEY_NONE = 0,
    LAPKA_KEY_ENTER, LAPKA_KEY_ESC, LAPKA_KEY_TAB, LAPKA_KEY_BACKTAB,
    LAPKA_KEY_BACKSPACE, LAPKA_KEY_DELETE,
    LAPKA_KEY_UP, LAPKA_KEY_DOWN, LAPKA_KEY_LEFT, LAPKA_KEY_RIGHT,
    LAPKA_KEY_HOME, LAPKA_KEY_END,
    LAPKA_KEY_PAGEUP, LAPKA_KEY_PAGEDOWN, LAPKA_KEY_INSERT,
    LAPKA_KEY_F1, LAPKA_KEY_F2, LAPKA_KEY_F3, LAPKA_KEY_F4,
    LAPKA_KEY_F5, LAPKA_KEY_F6, LAPKA_KEY_F7, LAPKA_KEY_F8,
    LAPKA_KEY_F9, LAPKA_KEY_F10, LAPKA_KEY_F11, LAPKA_KEY_F12,
    LAPKA_KEY_CODEPOINT,
    LAPKA_KEY_UNKNOWN
} LapkaKeyCode;

#define LAPKA_MOD_SHIFT (1 << 0)
#define LAPKA_MOD_CTRL  (1 << 1)
#define LAPKA_MOD_ALT   (1 << 2)

typedef enum {
    LAPKA_MOUSE_PRESS,
    LAPKA_MOUSE_RELEASE,
    LAPKA_MOUSE_MOVE,
    LAPKA_MOUSE_WHEEL_UP,
    LAPKA_MOUSE_WHEEL_DOWN
} LapkaMouseAction;

typedef struct {
    LapkaEventType type;
    union {
        struct { LapkaKeyCode code; uint32_t ch; uint8_t mods; } key;
        struct { int x, y; int button; LapkaMouseAction action; uint8_t mods; } mouse;
        struct { const char *data; size_t len; } paste;
        struct { int w, h; } resize;
    };
} LapkaEvent;
```

`LAPKA_KEY_CODEPOINT` with `ch` carries a printable or control codepoint. `Ctrl+A` is `{LAPKA_KEY_CODEPOINT, 'a', LAPKA_MOD_CTRL}`. Ctrl-C is `{LAPKA_KEY_CODEPOINT, 0x03, LAPKA_MOD_CTRL}` and every verb treats it as user cancel.

`LAPKA_EVENT_PASTE` data is NUL-terminated and valid UTF-8.

Mouse `x` and `y` are screen-absolute, in cells.

### Framebuffer

```c
typedef struct {
    int         w;
    int         h;
    LapkaCell  *cur;
    LapkaCell  *prev;
    bool        full_dirty;
} LapkaFramebuffer;
```

Owned by the run loop. Verbs read `w` and `h` and write into `cur` from their draw callback.

---

## `theme.h`

```c
typedef struct {
    uint32_t bg;
    uint32_t fg;

    uint32_t accent;
    uint32_t title;
    uint32_t muted;
    uint32_t error;
    uint32_t warning;
    uint32_t success;

    uint32_t modal_bg;
    uint32_t modal_border;
    uint32_t modal_title;

    LapkaBorder border;
    const char *cursor;
    const char *check;
    const char *radio_on;
    const char *radio_off;
    const char *bullet;
    const char *arrow;
} LapkaTheme;

extern const LapkaTheme LAPKA_THEME_DEFAULT;

uint32_t          lapka_color_darken(uint32_t rgb, int percent);
int               lapka_theme_load(const char *path, LapkaTheme *out);
const LapkaTheme *lapka_theme_active(void);
int               lapka_theme_set_file(const char *path);
void              lapka_theme_reset(void);
```

`lapka_color_darken` multiplies each RGB channel by `(100 - percent) / 100`. `percent` is clamped to `[0, 100]`.

`lapka_theme_load` reads a `KEY=value` file into `out`. Unknown keys are ignored. Missing keys keep whatever was already in `out`.

`lapka_theme_active` returns the active theme. It reads `LAPKA_THEME` on first call, caches the result, and returns the same pointer on every subsequent call.

`lapka_theme_set_file` overrides the active theme for the rest of the process. `lapka_theme_reset` reverts to the default.

---

## `layer.h`

```c
typedef enum { LAPKA_BLEND_OPAQUE, LAPKA_BLEND_TRANSPARENT } LapkaBlend;
typedef enum { LAPKA_INPUT_NONE, LAPKA_INPUT_PASS, LAPKA_INPUT_TRAP } LapkaInput;

typedef struct LapkaLayer LapkaLayer;

struct LapkaLayer {
    LapkaRect   rect;
    int         z;
    LapkaBlend  blend;
    LapkaInput  input;
    bool        visible;
    LapkaCell  *buf;

    void       (*draw)(LapkaLayer *l, const LapkaTheme *theme);
    bool       (*handle)(LapkaLayer *l, const LapkaEvent *ev);
    void       (*free)(LapkaLayer *l);
    void       *userdata;
};

LapkaLayer *lapka_layer_new(LapkaRect rect, int z,
                            LapkaBlend blend, LapkaInput input);
int         lapka_layer_resize(LapkaLayer *l, LapkaRect rect);
void        lapka_layer_free(LapkaLayer *l);

void lapka_composite(LapkaFramebuffer *fb, LapkaLayer **layers, int n);
```

`OPAQUE` replaces every cell in the rect. `TRANSPARENT` skips cells with `cp == 0`. Higher `z` is on top. `NONE` skips input, `PASS` forwards unhandled events to the layer below, `TRAP` drops them.

`lapka_composite` sorts the layers by `z` and blends them into the framebuffer. The caller's array is not modified.

---

## `verbs.h`

```c
typedef struct {
    bool alt_screen;
    bool mouse;
    bool paste;
    bool focus;
} LapkaInitOptions;

typedef struct {
    void (*draw)(void *ud, LapkaFramebuffer *fb);
    bool (*handle)(void *ud, const LapkaEvent *ev);
    bool (*tick)(void *ud);
    void *ud;
} LapkaRunCallbacks;

int lapka_run(const LapkaInitOptions *opts,
              const LapkaRunCallbacks *cb,
              int extra_fd);
```

The run loop owns the terminal, the framebuffer, and the event loop. It calls `lapka_term_init`, then on every iteration: checks for resize, calls `draw`, flushes, calls `tick`, polls, and calls `handle` on input.

`draw` writes into `fb->cur`. `handle` returns `true` to exit the loop. `tick` runs once per frame; return `true` to exit. `extra_fd` is an additional fd to poll (used by `spin` for the child's stdout); pass `-1` if unused.

`lapka_run` returns `0` on clean exit, `1` on read failure, `2` on init failure.

---

## `primitive.h`

### Base

```c
typedef enum {
    LAPKA_PRIMITIVE_BLOCK,
    LAPKA_PRIMITIVE_LIST,
    LAPKA_PRIMITIVE_TEXT,
    LAPKA_PRIMITIVE_PROMPT,
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
    void              (*draw)(struct LapkaPrimitive *p, LapkaCell *buf,
                              int w, int h, const LapkaTheme *theme);
    void              (*measure)(struct LapkaPrimitive *p,
                                 int bound_w, int bound_h,
                                 int *out_w, int *out_h);
    void              (*free)(struct LapkaPrimitive *p);
} LapkaPrimitive;

void lapka_primitive_free(LapkaPrimitive *p);
```

`draw` writes into a flat `buf` of `w * h` cells. `handle` returns `true` if the primitive consumed the event. `measure` reports natural size given a bound.

### Block

```c
LapkaPrimitive *lapka_block_new(const char *text);
void            lapka_block_set(LapkaPrimitive *p, const char *text);
void            lapka_block_align(LapkaPrimitive *p, LapkaAnchor align);
void            lapka_block_style(LapkaPrimitive *p, uint32_t fg, uint32_t bg,
                                  uint16_t attrs);
```

Wraps at whitespace. No cursor. `handle` always returns `false`.

### List

```c
typedef enum { LAPKA_SELECT_ONE, LAPKA_SELECT_MANY } LapkaSelectMode;

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

LapkaPrimitive *lapka_list_new(const char *const *items, int n,
                               LapkaSelectMode mode);
void            lapka_list_free(LapkaPrimitive *p);
void            lapka_list_set_items(LapkaPrimitive *p,
                                     const char *const *items, int n);
void            lapka_list_set_style(LapkaPrimitive *p,
                                     const LapkaListStyle *style);
void            lapka_list_attach_filter(LapkaPrimitive *p, LapkaPrimitive *text);
void            lapka_list_set_matcher(LapkaPrimitive *p, LapkaMatcher m);
void            lapka_list_refilter(LapkaPrimitive *p);
const char     *lapka_list_result(LapkaPrimitive *p, int index);
int             lapka_list_result_count(LapkaPrimitive *p);
int             lapka_list_cursor(LapkaPrimitive *p);
const char     *lapka_list_cursor_item(LapkaPrimitive *p);
```

The list copies its items; the caller's strings may be freed after the call.

`result(0)` in `ONE` mode is the cursor item after commit. In `MANY` mode, `result(i)` is the i-th checked item in order checked.

`lapka_list_attach_filter` makes the list filter its visible view by the text field's value on every `lapka_list_refilter` call. In `MANY` mode with a filter, `SPACE` goes into the text field and `TAB` toggles.

`lapka_filter_fuzzy` is a subsequence matcher: `nvm` matches `neovim`.

### Text

```c
LapkaPrimitive *lapka_text_new(const char *initial, bool masked);
void            lapka_text_set(LapkaPrimitive *p, const char *value);
const char     *lapka_text_value(LapkaPrimitive *p);
void            lapka_text_on_change(LapkaPrimitive *p,
                                     void (*fn)(void *), void *ud);
void            lapka_text_set_prefix(LapkaPrimitive *p, const char *prefix);
```

Cursor is a codepoint index. `masked` renders `*` per codepoint. `set` places the cursor at the end. `on_change` fires after every edit.

### Prompt

```c
LapkaPrimitive *lapka_prompt_new(const char *hint);
void            lapka_prompt_set(LapkaPrimitive *p, const char *hint);
```

### Tabs

```c
LapkaPrimitive *lapka_tabs_new(const char **labels, int count);
void            lapka_tabs_set_child(LapkaPrimitive *p, int idx,
                                     LapkaPrimitive *child);
void            lapka_tabs_set_active(LapkaPrimitive *p, int idx);
int             lapka_tabs_active(LapkaPrimitive *p);
int             lapka_tabs_count(LapkaPrimitive *p);
```

Tabs owns its children. `F1`..`F9` switch tabs when the tabs primitive itself sees the event. Otherwise everything goes to the active child.

### Progress

```c
LapkaPrimitive *lapka_progress_new(int total);
void            lapka_progress_set(LapkaPrimitive *p, int value);
int             lapka_progress_value(LapkaPrimitive *p);
int             lapka_progress_total(LapkaPrimitive *p);
void            lapka_progress_set_message(LapkaPrimitive *p, const char *msg);
const char     *lapka_progress_message(LapkaPrimitive *p);
```

`value` clamps to `[0, total]`. `handle` always returns `false`.

### Summary

```c
LapkaPrimitive *lapka_summary_new(void);
void            lapka_summary_set(LapkaPrimitive *p, const char *text);
void            lapka_summary_append(LapkaPrimitive *p, const char *line);
int             lapka_summary_line_count(LapkaPrimitive *p);
int             lapka_summary_scroll(LapkaPrimitive *p);
void            lapka_summary_reset_scroll(LapkaPrimitive *p);
```

`set` splits on newlines. Lines are not wrapped; long lines are clipped. Handles arrow keys, page keys, `j`/`k`.

### Tree

```c
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
```

`parent == -1` adds a top-level node. `SPACE` toggles expansion.

### Notification

```c
LapkaPrimitive *lapka_notification_new(const char *message, int duration_ms);
void            lapka_notification_set(LapkaPrimitive *p, const char *message);
bool            lapka_notification_expired(LapkaPrimitive *p);
```

The caller is responsible for freeing it once expired.

### Pager

```c
LapkaPrimitive *lapka_pager_new(void);
void            lapka_pager_set_lines(LapkaPrimitive *p,
                                      const char *const *lines, int count);
int             lapka_pager_set_file(LapkaPrimitive *p, const char *path);
void            lapka_pager_set_hint(LapkaPrimitive *p, const char *hint);
void            lapka_pager_set_search_enabled(LapkaPrimitive *p, bool enabled);
int             lapka_pager_line_count(LapkaPrimitive *p);
const char     *lapka_pager_line(LapkaPrimitive *p, int idx);
int             lapka_pager_scroll(LapkaPrimitive *p);
void            lapka_pager_reset_scroll(LapkaPrimitive *p);
bool            lapka_pager_is_searching(LapkaPrimitive *p);
int             lapka_pager_match_count(LapkaPrimitive *p);
```

Lines wrap. `UP`/`DOWN`, `PAGEUP`/`PAGEDOWN`, `HOME`/`END`, `j`/`k`, `g`/`G`, `n`/`N`, `/` when search is enabled.

### Editor

```c
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
```

`lapka_editor_text` returns a newly allocated string the caller must free.

`Ctrl-Z` undoes, `Ctrl-Y` redoes, `Ctrl-D` deletes the current line. `Ctrl-S` is handled by the verb, not the primitive.

Undo history is a stack of full-buffer snapshots capped at 200. Commands after an undo truncate the redo tail.

---

## Minimal program

```c
#include <lapka/lapka.h>
#include <stdio.h>

typedef struct {
    LapkaPrimitive *list;
    int exit_code;
} App;

static void draw(void *ud, LapkaFramebuffer *fb) {
    App *a = ud;
    const LapkaTheme *t = lapka_theme_active();
    LapkaRect title = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title, "-- Pick one --", t->title, 0, LAPKA_ATTR_BOLD);
    if (fb->h > 2) {
        LapkaCell *sub = fb->cur + (size_t)fb->w * 2;
        a->list->draw(a->list, sub, fb->w, fb->h - 2, t);
    }
}

static bool handle(void *ud, const LapkaEvent *ev) {
    App *a = ud;
    if (ev->type == LAPKA_EVENT_KEY) {
        if (ev->key.code == LAPKA_KEY_ENTER) { a->exit_code = 0; return true; }
        if (ev->key.code == LAPKA_KEY_ESC)   { a->exit_code = 1; return true; }
    }
    a->list->handle(a->list, ev);
    return false;
}

int main(void) {
    const char *items[] = { "alpha", "beta", "gamma" };
    App a = { lapka_list_new(items, 3, LAPKA_SELECT_ONE), 2 };
    if (!a.list) return 2;

    LapkaInitOptions opts = { 1, 1, 1, 0 };
    LapkaRunCallbacks cb = { draw, handle, NULL, &a };
    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && a.exit_code == 0)
        printf("%s\n", lapka_list_cursor_item(a.list));

    lapka_primitive_free(a.list);
    return rc != 0 ? rc : a.exit_code;
}
```

Build: `cc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 app.c liblapka.a -o app`

---

## Private

Internal only, not installed. `src/util/buf.h`, `src/util/log.h`, `src/core/utf8.h`, `src/render/*.h`, `src/term/*.h`, `src/input/input.h`, `src/proc/proc.h`, `src/verbs/*.h`, and the hub internals `hub_parse.c` / `hub_serialize.c`.

`LapkaFramebuffer` is defined in `types.h` because `LapkaRunCallbacks` and `lapka_composite` reference it.

---

## Reference

- `docs/spec.md` — the specification.
- `LICENSE` — CLEAR License v1.