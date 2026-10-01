# lapka — specification

Version 1.7 — draft

---

## 1. Scope

`lapka` is a layered terminal compositor library for C99 and POSIX.1-2008. It is driven by a binary named `tui` exposing fourteen verbs. It is called from bash, reads arguments from argv, writes results to stdout, and exits. It has no runtime dependencies. It does not call `setlocale`. It has no locale dependency.

---

## 2. Non-goals

- No GUI, no windowing system, no framebuffer, no X11, Wayland, SDL, GLFW, GTK, or Qt.
- No terminfo as a hard dependency.
- No plugin system, IPC protocol, daemon, or scripting language.
- No Windows native support.
- No ABI stability before 1.0.
- No mandatory external library dependencies.
- No knowledge of ArtixForge's state file, keys, validators, or storage layout.

---

## 3. Dependencies and build

**Mandatory:** none beyond C99 and POSIX.1-2008.

**Optional compile-time flags:**
- `LAPKA_WITH_TERMINFO` — link `-ltinfo` for capability lookup.
- `LAPKA_WITH_PTHREAD` — link `-lpthread`. Enables a single background worker thread that reads a subprocess's stdout in `spin`. The worker thread only reads the child's stdout into a buffer it owns. It never calls any other `lapka` function. No other function is thread-safe. The library otherwise assumes single-threaded use.

**Required build invocation:**
```
cc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -c src/main.c -o main.o
```

If the library cannot be built with this exact invocation, the design has failed.

---

## 4. Code policy

**No comments in code.** The only permitted non-code text in a source file is:

- The CLEAR License v1 copyright notice and license header. This is required by the license and is the sole exception to the no-comments rule.
- `#include` guards and `#pragma once`.

There are no `TODO`, `FIXME`, `XXX`, or `NOTE` strings in source files. Unfinished work is tracked in `docs/TODO.md`.

There are no comments in public headers. Type names, function names, and field names carry meaning. The documentation lives in `docs/spec.md`.

If a symbol needs explanation, its name is wrong and must be renamed. If a section needs explanation, it belongs in the spec.

---

## 5. Data model

### 5.1 Cell

```c
typedef struct {
    uint32_t cp;
    uint32_t fg;
    uint32_t bg;
    uint16_t attrs;
} LapkaCell;
```

`cp` is a Unicode codepoint. `0` means empty. A cell with `cp == 0` is a continuation cell when it follows a width-2 codepoint in the framebuffer, and an empty cell otherwise. The renderer distinguishes the two by looking at the preceding cell.

`fg` and `bg` are `0xRRGGBB`. `0` means terminal default.

`attrs` uses these bits:

```c
#define LAPKA_ATTR_BOLD      0x0001
#define LAPKA_ATTR_DIM       0x0002
#define LAPKA_ATTR_ITALIC    0x0004
#define LAPKA_ATTR_UNDERLINE 0x0008
#define LAPKA_ATTR_BLINK     0x0010
#define LAPKA_ATTR_REVERSE   0x0020
#define LAPKA_ATTR_STRIKE    0x0040
```

Bits 7 through 15 are reserved and must be zero.

Cells compare equal when all four fields are equal.

### 5.2 Rect

```c
typedef struct { int x, y, w, h; } LapkaRect;
```

The only spatial type. All positions reduce to `LapkaRect`.

`w` and `h` are cell counts. A rect with `w <= 0` or `h <= 0` is empty and draws nothing.

### 5.3 Size

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

`FIT` — as small as content needs, clipped to the parent bound. `val` ignored.

`FULL` — equal to the parent bound on this axis. `val` ignored.

`FIXED` — `val` cells, clamped to the parent bound. Negative `val` treated as zero.

`PERCENT` — `parent * val / 100` cells, rounded down. `val` clamped to `[0, 100]`.

### 5.4 Anchor

```c
typedef enum {
    LAPKA_ANCHOR_TL, LAPKA_ANCHOR_TC, LAPKA_ANCHOR_TR,
    LAPKA_ANCHOR_LC, LAPKA_ANCHOR_CC, LAPKA_ANCHOR_RC,
    LAPKA_ANCHOR_BL, LAPKA_ANCHOR_BC, LAPKA_ANCHOR_BR
} LapkaAnchor;
```

Two-letter codes on the command line: `TL`, `TC`, `TR`, `LC`, `CC`, `RC`, `BL`, `BC`, `BR`.

### 5.5 Border

```c
typedef enum {
    LAPKA_BORDER_NONE,
    LAPKA_BORDER_SINGLE,
    LAPKA_BORDER_ROUNDED,
    LAPKA_BORDER_DOUBLE
} LapkaBorder;
```

A border, if present, occupies one cell on each side of the box rect. Border glyphs are hard-coded in `draw.c`.

### 5.6 Box

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

### 5.7 Box model

Application order, outside to inside:

1. The parent rect is the available space.
2. Margin shrinks the space: `inner.x = parent.x + margin[3]`, `inner.w = parent.w - margin[3] - margin[1]`. Same for vertical.
3. The box rect is placed within `inner` according to `anchor`, with size `width` and `height`.
4. Border draws at the box rect, occupying one cell on each side if not `NONE` and box width/height are at least 3. If the box dimension is less than 3, border is treated as `NONE` for that axis.
5. Padding shrinks inward from the effective border.
6. Content rect is what remains.

If `width.mode == LAPKA_SIZE_FIT`, the box rect width is `natural_w + 2 * border + pad[3] + pad[1]`, then clamped to `inner.w`. Same for height. `border` here is `1` if a border is drawn, `0` otherwise.

If padding exceeds the box size after border, the content rect is clipped to zero.

`FIT` measure is computed from a single primitive's natural size given the parent bound. There is no recursion, no measure-arrange pass.

Content natural size rules per primitive are in section 11.

Layout inside a layer is done by the verb's draw callback, using `lapka_place` for each primitive. There is no container type.

### 5.8 Placement

```c
LapkaRect lapka_place(LapkaRect parent, LapkaBox box, int natural_w, int natural_h);
```

Pure function. No side effects. No allocation. `natural_w` and `natural_h` are ignored if the corresponding size mode is not `FIT`.

---

## 6. Layer model

### 6.1 Layer

```c
typedef enum {
    LAPKA_BLEND_OPAQUE,
    LAPKA_BLEND_TRANSPARENT
} LapkaBlend;

typedef enum {
    LAPKA_INPUT_NONE,
    LAPKA_INPUT_PASS,
    LAPKA_INPUT_TRAP
} LapkaInput;

typedef struct LapkaLayer {
    LapkaRect   rect;
    int         z;
    LapkaBlend  blend;
    LapkaInput  input;
    bool        visible;
    LapkaCell  *buf;

    void      (*draw)(struct LapkaLayer *l, const LapkaTheme *theme);
    bool      (*handle)(struct LapkaLayer *l, const LapkaEvent *ev);
    void      (*free)(struct LapkaLayer *l);
    void       *userdata;
} LapkaLayer;

LapkaLayer *lapka_layer_new(LapkaRect rect, int z,
                            LapkaBlend blend, LapkaInput input);
int         lapka_layer_resize(LapkaLayer *l, LapkaRect rect);
void        lapka_layer_free(LapkaLayer *l);
```

Layers are unbounded. Each owns its buffer of `rect.w * rect.h` cells.

`z` is the stacking order. Higher is on top.

`userdata` is opaque to the compositor and is owned by the caller.

`lapka_layer_free` calls the layer's `free` callback if set, then frees `buf` and the layer itself. The callback is responsible for `userdata` only. It must not free `buf` or the layer.

### 6.2 Blend

All cells are stored in truecolor internally. Color quantization happens at flush.

`LAPKA_BLEND_OPAQUE` — copy every cell in the layer's rect, replacing `cp`, `fg`, `bg`, and `attrs` in the framebuffer. The layer's buffer must fill every cell in its rect. Layers with this blend are solid panels; they fully occlude what is beneath.

`LAPKA_BLEND_TRANSPARENT` — for each cell in the layer's rect, if the layer's cell has `cp == 0`, skip. Otherwise copy the whole cell. Layers with this blend are overlays; the framebuffer shows through wherever the layer has no content.

There is no dimming. A modal that wants to stand out uses `OPAQUE` blend and a darker background color in its own buffer. It does not alter the framebuffer outside its rect.

Layers are clipped to the framebuffer. Portions of a layer outside the framebuffer are not drawn and receive no input.

### 6.3 Input

`LAPKA_INPUT_NONE` — the layer is skipped during input routing.

`LAPKA_INPUT_PASS` — the layer receives input; unhandled events route to the next layer below.

`LAPKA_INPUT_TRAP` — the layer receives input; unhandled events are dropped.

Routing walks visible layers top-down. The topmost layer with `input != NONE` receives the event.

Mouse events follow the same routing. A `TRAP` layer receives clicks outside its rect and ignores them. A `PASS` layer forwards them.

Default policies:

- Menu, checklist, filter, input, password, hub — `LAPKA_INPUT_PASS`.
- Modal dialogs, editors, nested hubs — `LAPKA_INPUT_TRAP`.
- Toasts, progress — `LAPKA_INPUT_NONE`.

### 6.4 Compositor

```c
void lapka_composite(LapkaFramebuffer *fb, LapkaLayer **layers, int n);
```

The compositor sorts `layers` by `z` ascending before compositing. The caller's array is not modified.

For each visible layer in ascending `z`, blend the layer's cells into the framebuffer per the rules in 6.2, clipped to the framebuffer.

Verbs with a single screen may write directly to the framebuffer instead of composing layers. Verbs with overlapping content (modals over content, toasts, sidebars) use the compositor.

---

## 7. Terminal

### 7.1 Init

```c
bool lapka_term_init(const LapkaInitOptions *opts);
void lapka_term_shutdown(void);
```

`LapkaInitOptions`:

```c
typedef struct {
    bool alt_screen;
    bool mouse;
    bool paste;
    bool focus;
} LapkaInitOptions;
```

- Open `/dev/tty` read-write. Save the fd in a file-scope static.
- `tcgetattr`, save the original termios in a file-scope static struct.
- Check if the terminal is already in raw mode (`!(lflag & ECHO) && !(lflag & ICANON)`). If so, restore the cooked defaults below first, then continue.

Cooked defaults:

```c
iflag = ICRNL | IXON
oflag = OPOST | ONLCR
cflag = CS8 | CREAD
lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK
cc[VMIN] = 1
cc[VTIME] = 0
```

- Set raw: clear `ECHO`, `ICANON`, `IEXTEN`, `ISIG`; clear `IXON`, `ICRNL`, `BRKINT`, `INPCK`, `ISTRIP`; set `CS8`; clear `OPOST`; `VMIN=0`, `VTIME=0`.
- If `opts->alt_screen` is true and `LAPKA_ALT_SCREEN=1` is not set in the environment, write `\033[?1049h` and remember that the alt screen was entered.
- Always write `\033[?25l`.
- If `opts->mouse`, enable SGR mouse (`\033[?1000h\033[?1006h`).
- If `opts->paste`, enable bracketed paste (`\033[?2004h`).
- If `opts->focus`, enable focus events (`\033[?1004h`).
- Write `\033[?2026h` (synchronized output) unless `TERM` is `dumb`, `linux`, `vt100`, or `vt102`.

Detect color depth once at init and cache in a file-scope static. Re-detect on `SIGWINCH`.

### 7.1.1 Session ownership

`LAPKA_ALT_SCREEN=1` in the environment means the caller has entered the alt screen and owns it. The verb does not enter it and does not leave it. This allows a shell session to hold the alt screen across many consecutive `tui` invocations, eliminating the screen toggle between verbs.

The bash side is responsible for entering and leaving the alt screen when it sets and unsets the variable. Two helper functions are provided in `core.sh`:

```bash
tui_session_begin() {
    printf '\033[?1049h\033[H\033[2J' > /dev/tty
    export LAPKA_ALT_SCREEN=1
}

tui_session_end() {
    printf '\033[?1049l' > /dev/tty
    unset LAPKA_ALT_SCREEN
}
```

The installer entry point calls `tui_session_begin` once at startup, installs `trap tui_session_end EXIT INT TERM`, and calls `tui_session_end` on normal exit.

When a `tui` verb runs standalone (no `LAPKA_ALT_SCREEN`), it enters and leaves the alt screen itself.

### 7.2 Teardown

```c
void lapka_restore_terminal(void);
```

Static state:

```c
static volatile sig_atomic_t s_restored = 0;
static int                    s_tty_fd   = -1;
static struct termios         s_orig;
static bool                   s_orig_valid = false;
static bool                   s_alt_screen = false;
```

Behavior:

1. If `s_restored`, return.
2. Set `s_restored = 1`.
3. If `s_orig_valid`, `tcsetattr(s_tty_fd, TCSANOW, &s_orig)`.
4. `write(s_tty_fd, "\033[0m\033[?1000l\033[?1006l\033[?2004l\033[?1004l\033[?2026l\033[?25h", N)`.
5. If `s_alt_screen`, write `\033[?1049l`.
6. If not in a signal handler, `tcdrain(s_tty_fd)`.

Called from the normal exit path, from an `atexit` handler as a backstop, and from every signal handler.

`lapka_signals_save` clears `s_restored` on every call, so a second `lapka_term_init` after a `lapka_term_shutdown` re-arms the teardown.

The symbol in `src/term/signals.c` is `lapka_term_restore`.

### 7.3 Signals

`ISIG` is cleared. Keyboard Ctrl-C arrives as byte `0x03`, not as a signal. Every verb treats `0x03` as user cancel and exits 1. The parent bash script receives exit 1, not `SIGINT`, and its `trap 'installer_abort' INT TERM` does not fire.

Handlers are installed for `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGSEGV`, `SIGABRT`, `SIGPIPE`. These fire only when a signal is delivered externally (via `kill`, or when an SSH session disconnects and `SIGHUP` is sent).

Each handler calls `lapka_term_restore()` and then `_exit(128 + signo)`.

Only async-signal-safe calls inside the handler. `write` and `tcsetattr` only.

`SIGSEGV` and `SIGABRT` are caught so the terminal is restored before the crash. Core dumps are suppressed. Debugging uses `strace` or a debugger attached before the crash.

`SIGWINCH` is handled separately (section 7.4).

### 7.4 Resize

A `SIGWINCH` handler sets `static volatile sig_atomic_t s_resize_pending = 1` and returns. It does not redraw.

The central run loop (section 7.6) queries `TIOCGWINSZ` at the top of every iteration. If the size changed, it resizes the framebuffer, re-detects color depth, marks the framebuffer fully dirty, and clears `s_resize_pending`.

If the new size is below 20 columns or 5 rows, the loop draws a single centered line `Terminal too small` and waits for either a resize or Enter or Esc. No verb's draw callback runs while the terminal is too small.

Layout is fully recomputed on every resize. No incremental reflow.

### 7.5 SIGKILL and uncatchable termination

If the process is killed with `SIGKILL`, no handler runs and the terminal is left raw. This is unavoidable.

Mitigation in the caller:

```bash
_term_reset() {
    stty sane 2>/dev/null || true
    printf '\e[?1049l\e[0m\e[?25h' >/dev/tty 2>/dev/null || true
}
```

Called once at the top of the caller's entry script, unconditionally, before any `tui` invocation. Also called before any fallback error output.

`lapka`'s init checks for a terminal left in raw mode (section 7.1) and restores it. A subsequent `tui` invocation self-heals.

### 7.6 Central run loop

Every interactive verb uses the central run loop. No verb implements its own event loop. No verb writes escape sequences directly to the tty outside of `lapka_flush` and `lapka_term_init`/`lapka_term_shutdown`.

```c
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

The run loop owns the framebuffer and the event loop. It:

1. Calls `lapka_term_init(opts)`.
2. Initializes the framebuffer to the terminal size.
3. Loop:
   1. Queries `TIOCGWINSZ`. If it changed, resizes the framebuffer and marks it fully dirty.
   2. Clears `s_resize_pending`.
   3. If the terminal is too small, draws `Terminal too small`, flushes, and waits for the next event.
   4. Otherwise calls `cb->draw` into the framebuffer, then `lapka_flush`.
   5. Calls `cb->tick`. If it returns `true`, exits the loop.
   6. Polls `in_fd` and `extra_fd` (if `>= 0`) for 100 ms.
   7. On tty input, reads one event and calls `cb->handle`. If it returns `true`, exits the loop.
4. Frees the framebuffer, calls `lapka_term_shutdown`, and returns 0.

`cb->tick` is optional. `extra_fd` is `-1` for verbs with no subprocess.

The `handle` return value means "should the loop exit." It does not mean "did I consume the event." A handler that wants to keep running returns `false` even if it acted on the event.

The `draw` callback writes directly into the framebuffer. Verbs with overlapping content build layers and call `lapka_composite` inside the draw callback.

---

## 8. Rendering

### 8.1 Framebuffer

```c
typedef struct {
    int         w;
    int         h;
    LapkaCell  *cur;
    LapkaCell  *prev;
    bool        full_dirty;
} LapkaFramebuffer;
```

Two cell buffers: current and previous. Both `w * h` cells. Owned by the run loop. Reallocated on resize.

### 8.2 Flush

```c
int lapka_flush(LapkaFramebuffer *fb, int tty_fd, int color_depth);
```

Diff `cur` against `prev`. Emit cursor-position escapes only where cells differ. Emit SGR only when the style changes between adjacent cells.

**Continuation cells are never emitted.** A cell whose `cp == 0` and whose preceding cell has a width-2 codepoint is a continuation cell. The terminal already advanced past it when it drew the wide codepoint. Emitting a space for a continuation cell would shift the row right by one column for every wide codepoint in the row. The loop tracks this: after emitting a width-2 codepoint, the next cell is skipped. The `prev_was_wide` flag is per-row and resets at the start of each row.

Cells containing U+0009 (tab) are normalized to U+0020 (space) at emit time. A raw tab byte written to the terminal would move the cursor to the next tab stop, not occupy one cell.

Cell contents with `cp == 0` are emitted as a single space. This includes empty cells and cells the framebuffer has not touched, unless the previous cell was a width-2 lead, in which case the cell is skipped entirely.

Write the buffer via a loop that retries until all bytes are written or `write` returns `-1` or `0`. A partial write is not an error; it means the loop continues.

Wrap the entire flush in `\033[?2026h` and `\033[?2026l` if synchronized output is supported.

`tcdrain` before returning.

After flush, copy `cur` into `prev`.

There is no full-screen clear on redraw. Never emit `\033[2J` during normal operation.

Synchronized output is assumed supported unless `TERM` is `dumb`, `linux`, `vt100`, or `vt102`.

### 8.3 Color quantization

Cells are stored in truecolor and emitted as truecolor SGR (`38;2;R;G;B` / `48;2;R;G;B`) regardless of the `color_depth` argument.

On terminals that do not support truecolor (`TERM=linux`, `vt100`, `vt102`, `dumb`), the emitted SGR is not understood and the terminal falls back to default colors. On 256-color and 16-color terminals, most modern emulators downconvert gracefully; older ones may not.

The specified behavior:

- Truecolor — emit as-is.
- 256 — nearest match in the 256-color palette, distance = squared Euclidean in sRGB.
- 16 — nearest match in the 16-color palette, distance = squared Euclidean in sRGB.
- Mono — default foreground.

Depth is detected at init from `COLORTERM` (`truecolor` or `24bit` implies truecolor) and `TERM` (contains `256color` implies 256). If neither, default to 16. Re-detected on resize.

Quantization is in `flush.c`, not in the compositor. Layer buffers are always full fidelity.

### 8.4 Text wrapping

Text wraps at whitespace boundaries. A word longer than the available width is hard-broken at the cell boundary. Whitespace at wrap points is consumed.

A word is a maximal run of non-whitespace codepoints. CJK ideographs are treated as single-codepoint words, so they wrap at every codepoint boundary.

`lapka_draw_text` in `src/render/draw.c` wraps at cell boundaries. The whitespace rule above applies to primitives that do their own wrapping.

### 8.5 Output channels

Results are written to stdout via stdio (`printf`, `fputs`). Escape sequences are written to the tty fd via raw `write`. The two streams are never mixed. stdout is flushed via `fflush(stdout)` before normal exit.

Display is always written to `/dev/tty`, never to stdout, regardless of where stdout points.

---

## 9. UTF-8

```c
int lapka_cp_width(uint32_t cp);
int lapka_utf8_decode(const uint8_t *buf, size_t len, uint32_t *cp_out);
int lapka_utf8_encode(uint32_t cp, uint8_t *out);
```

`lapka_utf8_decode` always returns bytes consumed (1 to 4) and sets `cp_out`. On invalid input, consumes exactly one byte and sets `cp_out = 0xFFFD`. Never returns `-1`.

`lapka_utf8_encode` returns bytes written. Never more than 4.

`lapka_cp_width` returns 0, 1, or 2:

- Codepoints below U+0020, U+007F, and U+0080–U+009F — width 0, not rendered.
- U+0009 (tab) — width 1, rendered as one space. No tab-stop expansion. The flush layer normalizes the emitted codepoint to U+0020.
- Combining marks (U+0300–U+036F, U+3099–U+309A, U+1AB0–U+1AFF, U+1DC0–U+1DFF, U+20D0–U+20FF, U+FE20–U+FE2F) — width 0.
- Zero-width characters (U+200B, U+200C, U+200D, U+2060, U+FEFF) — width 0.
- East Asian Wide and Fullwidth (Unicode `EastAsianWidth.txt` classes W and F) — width 2.
- Everything else — width 1.

The width table is a static array of `{start, end}` ranges consulted by a linear scan. It is not derived directly from `EastAsianWidth.txt`; the wide range list covers the common CJK, Hangul, and emoji blocks. No allocation.

No grapheme cluster support. Each codepoint occupies its own cell, or zero cells, or two cells. ZWJ sequences render as their individual codepoints.

---

## 10. Input

### 10.1 Read loop

Bytes are read one at a time from the tty fd. Each byte is preceded by a `poll` with a caller-specified timeout. There is no ring buffer; the parser reads directly from the fd.

If the first byte is `\033`, an escape sequence is parsed. If the sequence is incomplete within 25 ms, the initial `\033` is treated as a lone `ESC` key. If the sequence is malformed, the parser emits `LAPKA_KEY_UNKNOWN` rather than a lone `ESC`.

If the first byte is a UTF-8 lead byte, the remaining bytes of the codepoint are read. If the sequence is invalid, the parser returns an error rather than emitting U+FFFD.

Otherwise, the byte is emitted as a single-byte codepoint.

There is only one waiting model. `poll` handles all waiting. `VMIN` and `VTIME` are zero.

### 10.2 Lone ESC

A solitary `ESC` is only treated as the Escape key if no further bytes arrive within 25 ms.

### 10.3 Bracketed paste

The terminal is configured to send `CSI 200~` and `CSI 201~` around pasted data when a verb requests `paste`, but the input parser does not accumulate the pasted bytes into a single event.

A paste arrives as a stream: `CSI 200~` parses as `LAPKA_KEY_UNKNOWN`, followed by one codepoint event per pasted character, followed by `CSI 201~` which also parses as `LAPKA_KEY_UNKNOWN`. Verbs that accept text see the pasted characters as if they had been typed.

The specified behavior:

`CSI 200~` starts paste accumulation. All input until `CSI 201~` is collected into a single `LAPKA_EVENT_PASTE` with the pasted bytes as one UTF-8 string.

The paste buffer is a growable byte buffer, not a ring buffer. It grows to fit the paste. There is no fixed cap.

Pasted data is truncated at the first U+0000. The remainder is discarded.

### 10.4 Events

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
    LAPKA_KEY_ENTER,
    LAPKA_KEY_ESC,
    LAPKA_KEY_TAB,
    LAPKA_KEY_BACKTAB,
    LAPKA_KEY_BACKSPACE,
    LAPKA_KEY_DELETE,
    LAPKA_KEY_UP,
    LAPKA_KEY_DOWN,
    LAPKA_KEY_LEFT,
    LAPKA_KEY_RIGHT,
    LAPKA_KEY_HOME,
    LAPKA_KEY_END,
    LAPKA_KEY_PAGEUP,
    LAPKA_KEY_PAGEDOWN,
    LAPKA_KEY_INSERT,
    LAPKA_KEY_F1,
    LAPKA_KEY_F2,
    LAPKA_KEY_F3,
    LAPKA_KEY_F4,
    LAPKA_KEY_F5,
    LAPKA_KEY_F6,
    LAPKA_KEY_F7,
    LAPKA_KEY_F8,
    LAPKA_KEY_F9,
    LAPKA_KEY_F10,
    LAPKA_KEY_F11,
    LAPKA_KEY_F12,
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
        struct {
            LapkaKeyCode code;
            uint32_t     ch;
            uint8_t      mods;
        } key;
        struct {
            int              x, y;
            int              button;
            LapkaMouseAction action;
            uint8_t          mods;
        } mouse;
        struct {
            const char *data;
            size_t      len;
        } paste;
        struct {
            int w, h;
        } resize;
    };
} LapkaEvent;
```

`LAPKA_KEY_CODEPOINT` with `ch` carries a codepoint, printable or control. Non-ASCII codepoints also arrive as `LAPKA_KEY_CODEPOINT`. Modifiers are separate; `Ctrl+A` is `{LAPKA_KEY_CODEPOINT, 'a', LAPKA_MOD_CTRL}`.

Byte `0x03` (Ctrl-C) arrives as `{LAPKA_KEY_CODEPOINT, 0x03, LAPKA_MOD_CTRL}`. Every verb treats it as user cancel.

`LAPKA_EVENT_PASTE`, `LAPKA_EVENT_FOCUS_IN`, and `LAPKA_EVENT_FOCUS_OUT` are declared but not produced by the input layer.

Mouse `x` and `y` are screen-absolute, measured in cells. Verbs translate to their own layer's coordinate space before routing to primitives.

---

## 11. Primitives

All primitives share a common header:

```c
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
```

Every primitive is a self-contained object with its own state. A verb composes primitives, draws them into the framebuffer or a sub-buffer at a computed rect, and routes events to them.

Primitives that need a stride-independent sub-buffer must be drawn into a temporary buffer of the target width, then copied row by row into the destination. Direct sub-buffer drawing only works when the destination rows are contiguous at the same stride.

The list of primitives is closed: there is no `LAPKA_PRIMITIVE_BLOCK` and no `LAPKA_PRIMITIVE_PROMPT`.

### 11.1 List

```c
typedef enum {
    LAPKA_SELECT_ONE,
    LAPKA_SELECT_MANY
} LapkaSelectMode;

typedef bool (*LapkaMatcher)(const char *item, const char *query);

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

bool lapka_filter_fuzzy(const char *item, const char *query);
```

The list copies its items. The caller's array and strings may be freed after `lapka_list_new` or `lapka_list_set_items` returns.

In `LAPKA_SELECT_ONE`, `result_count` is 1 after commit or 0 if cancelled. `result(0)` returns the cursor item.

In `LAPKA_SELECT_MANY`, `result_count` is the number of checked items. `result(i)` returns the i-th checked item in order of first check. Unchecking and rechecking preserves the original position.

The default matcher is case-insensitive substring match. `lapka_filter_fuzzy` is a subsequence matcher and is used by the `filter` verb.

The filter hook is optional. When a `LapkaText` is attached via `lapka_list_attach_filter`, the list filters its items by the text's value on every `lapka_list_refilter` call. The list keeps a `visible` array of indices into the item list. All cursor operations and result lookups operate on the visible array. `lapka_list_refilter` rebuilds it.

`lapka_list_set_items` rebuilds `visible` as an identity mapping. If a filter is attached, the caller must call `lapka_list_refilter` afterwards to reapply it.

In `MANY` mode, `SPACE` toggles the cursor item unless a filter is attached, in which case `SPACE` is passed through to the filter and `TAB` toggles.

Measure: `min(visible_count, bound.h)` rows; max visible item width clipped to `bound.w`.

### 11.2 Text

```c
LapkaPrimitive *lapka_text_new(const char *initial, bool masked);
void            lapka_text_set(LapkaPrimitive *p, const char *value);
const char     *lapka_text_value(LapkaPrimitive *p);
void            lapka_text_on_change(LapkaPrimitive *p, void (*fn)(void *), void *ud);
void            lapka_text_set_prefix(LapkaPrimitive *p, const char *prefix);
```

State: UTF-8 buffer, cursor, mask flag, prefix string.

Cursor is a codepoint index, not a byte offset. Text is stored as UTF-8 but cursor arithmetic is codepoint-based.

Masked mode renders `*` per codepoint. The cell at the cursor position is rendered as a reverse-video space regardless of mask mode.

`lapka_text_set` places the cursor at the end of the new value.

`lapka_text_set_prefix` sets a prefix drawn before the value. `input`, `password`, and `password-confirm` set it to `"> "`.

`lapka_text_on_change` registers a callback invoked after every edit that changes the value.

Measure: 1 row; prefix width plus value width clipped to `bound.w`.

### 11.3 Tabs

A container that hosts N primitives, shows one at a time, and forwards input to the active one.

```c
LapkaPrimitive *lapka_tabs_new(const char **labels, int count);
void            lapka_tabs_set_child(LapkaPrimitive *p, int idx, LapkaPrimitive *child);
void            lapka_tabs_set_active(LapkaPrimitive *p, int idx);
int             lapka_tabs_active(LapkaPrimitive *p);
int             lapka_tabs_count(LapkaPrimitive *p);
```

The tabs primitive owns its children and frees them when it is freed. `lapka_tabs_set_child` replaces any existing child at that index and frees the old one.

Switching tabs is via `F1`..`F9` when the tabs primitive itself receives the event. Otherwise all events go to the active child.

The tab bar is drawn on the first row. Children are drawn on the remaining rows.

Count is clamped to `[1, 9]`. Measure is the tab bar width plus the active child's measured height plus one.

### 11.4 Progress

A determinate bar. The caller sets the value.

```c
LapkaPrimitive *lapka_progress_new(int total);
void            lapka_progress_set(LapkaPrimitive *p, int value);
int             lapka_progress_value(LapkaPrimitive *p);
int             lapka_progress_total(LapkaPrimitive *p);
void            lapka_progress_set_message(LapkaPrimitive *p, const char *msg);
const char     *lapka_progress_message(LapkaPrimitive *p);
```

The bar occupies one row when there is no message, two rows when there is. `value` is clamped to `[0, total]`. `total == 0` is a valid state for a bar that has not been initialized; the bar renders as full if `value > 0`.

`handle` always returns `false`.

Measure: 40 columns wide by 1 or 2 rows.

### 11.5 Summary

A scrolling text body. Used by review screens and log views.

```c
LapkaPrimitive *lapka_summary_new(void);
void            lapka_summary_set(LapkaPrimitive *p, const char *text);
void            lapka_summary_append(LapkaPrimitive *p, const char *line);
int             lapka_summary_line_count(LapkaPrimitive *p);
int             lapka_summary_scroll(LapkaPrimitive *p);
void            lapka_summary_reset_scroll(LapkaPrimitive *p);
```

`lapka_summary_set` splits its argument on newlines and replaces the existing content. `lapka_summary_append` adds one line to the end.

Lines are not wrapped. A line longer than the visible width is clipped.

Handles `UP`, `DOWN`, `PAGEUP`, `PAGEDOWN`, `HOME`, `END`, `j`, `k`.

Measure: max line width clipped to `bound.w`; line count clipped to `bound.h`.

### 11.6 Tree

A hierarchical list with expand/collapse. Flat-node storage.

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

Nodes are added with `lapka_tree_add`. `parent == -1` adds a top-level node. The parent must already exist. A node's `has_children` flag is set when its first child is added.

The cursor moves over the visible subset: top-level nodes, plus descendants of any expanded node. `SPACE` toggles expansion of the cursor node if it has children.

`handle` returns `false` for events it does not consume.

Measure: max indented label width clipped to `bound.w`; visible count clipped to `bound.h`.

### 11.7 Notification

An auto-expiring message box.

```c
LapkaPrimitive *lapka_notification_new(const char *message, int duration_ms);
void            lapka_notification_set(LapkaPrimitive *p, const char *message);
bool            lapka_notification_expired(LapkaPrimitive *p);
```

`lapka_notification_new` records the current monotonic time. The primitive is "expired" when `now - start >= duration_ms`. The caller is responsible for freeing it once expired.

`handle` always returns `false`.

Measure: message width plus six columns, minimum 20; 3 rows.

### 11.8 Pager

A scrollable text view with search.

```c
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
```

Lines are wrapped. The last visual row of the primitive is reserved for a hint or, while searching, for the search field.

Keys: `UP`/`DOWN`, `PAGEUP`/`PAGEDOWN`, `HOME`/`END`, `j`/`k`, `g`/`G`, `b`/`SPACE` for page up/down, `n`/`N` for next/previous match, `/` to start a search (when enabled). While searching, `ENTER` commits, `ESC` cancels, everything else goes into the field.

Match highlighting draws the matched substring in `theme.accent` with `LAPKA_ATTR_BOLD`. Matches that straddle a wrap boundary are highlighted only on the first visual row. The current implementation does not highlight split matches.

Measure: max line width clipped to `bound.w`; total wrapped rows plus one clipped to `bound.h`.

### 11.9 Editor

A multi-line text editor with undo and redo.

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

`lapka_editor_text` returns a newly allocated string the caller must `free`. It is valid until the caller frees it. The next call returns a fresh allocation.

Keys: arrow keys, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `ENTER`, `BACKSPACE`, `DELETE`, printable codepoints, and tab (inserted as `\t`). `Ctrl-S` is handled by the caller, not the primitive. `Ctrl-Z` undoes, `Ctrl-Y` redoes, `Ctrl-D` deletes the current line.

Undo history is a stack of full-buffer snapshots capped at 200 entries. Commands after an undo truncate the redo tail. The cursor position is part of each snapshot.

The cursor is drawn as a reverse-video cell when `cursor_visible` is true. The caller flips the flag on a timer.

The primitive does not enforce read-only. The caller checks writability and decides whether to allow `lapka_editor_save`.

`lapka_editor_save` writes every line followed by a newline. A file that does not end with a newline gains one on the first save.

Measure: max line width clipped to `bound.w`; line count clipped to `bound.h`.

---

## 12. Verbs

The binary is `tui`. It dispatches on `argv[1]`.

### 12.1 Conventions

- stdout carries the result, if any. Nothing else.
- stderr carries diagnostics and log lines.
- Display is written to `/dev/tty`, never to stdout.
- stdout is flushed via `fflush(stdout)` before normal exit. Signal exits use `_exit` and do not flush.
- Exit codes: `0` success, `1` user cancelled, `2` internal error, `3` usage error.
- No hidden state. A verb never reads or writes `/tmp/artix-installer/state.conf`. A verb never connects to a running process. A verb is one-shot.
- If required arguments are missing, exit 3 with usage on stderr.
- `--help` and `-h` print usage to stdout and exit 0.
- `--version` prints `lapka MAJOR.MINOR.PATCH` to stdout and exit 0.
- Arguments after `--` are not parsed as flags. For `spin`, they are the child command.
- Verbs that produce a sensitive result (`password`, `password-confirm`) require stdout to not be a terminal. If `isatty(STDOUT_FILENO)` is true, the verb exits 2 with a message on stderr before collecting any input. The value is written to stdout, and only to stdout.
- If `LAPKA_ALT_SCREEN=1` is set in the environment, the verb assumes the alt screen is already entered and does not manage it.

### 12.2 Common flags

The common flag vocabulary. Verbs accept these where noted.

```
--theme=FILE            main.c, before dispatch
--where=TL|TC|TR|LC|CC|RC|BL|BC|BR   spin
--alt-screen, --no-alt-screen
--mouse, --no-mouse
--size=fit|full|fixed|percent
--width=N
--height=N
--border=none|single|rounded|double
--color=N
--title-color=N
```

Only `--theme` and `--where` are parsed. The remaining flags are specified but not accepted by any verb; passing them results in exit 3.

`--border`, if given, overrides the theme's border style for this invocation.

### 12.3 Verb list

```
tui msg              "Title" "Body"
tui yesno            "Title" "Body"
tui menu             "Title" "Body" [--height=N] item...
tui checklist        "Title" "Body" [--height=N] item...
tui filter           "Title" "Body" [--height=N] [--multi]
tui input            "Title" "Body" [--default D]
tui password         "Title" "Body"
tui password-confirm "Title" "Body" [--confirm-prompt P] [--tries N]
tui spin             "Title" "Body" [--where=ANCHOR] [--hold N|enter|none] -- CMD...
tui show-file        "Title" FILE
tui toast            "Message" [--duration=MS]
tui editor           FILE [--new]
tui explorer         [PATH] [--dir|--file]
tui hub              --in FILE --out FILE [--check]
```

Fourteen verbs.

`--check` makes the hub refuse to open `--in` or `--out` unless both are root-owned regular files with mode 0600 or stricter. It is off by default.

### 12.4 Per-verb defaults

| Verb | alt_screen | mouse | paste |
|---|---|---|---|
| `msg` | yes | no | no |
| `yesno` | yes | no | no |
| `menu` | yes | yes | yes |
| `checklist` | yes | yes | yes |
| `filter` | yes | yes | yes |
| `input` | yes | no | yes |
| `password` | yes | no | yes |
| `password-confirm` | yes | no | yes |
| `spin` | yes | no | no |
| `show-file` | yes | yes | no |
| `toast` | no | no | no |
| `editor` | yes | no | yes |
| `explorer` | yes | yes | yes |
| `hub` | yes | yes | yes |

`toast` runs on the main screen. On a terminal that is not already in the alt screen, the toast pixels persist after exit until something else redraws. Toasts are best used in a session that already holds the alt screen.

Every other verb enters the alt screen. When run under `LAPKA_ALT_SCREEN=1`, the caller has already entered it.

### 12.5 Cancel semantics

| Verb | Enter | Esc | Ctrl-C | Empty submit |
|---|---|---|---|---|
| `msg` | exit 0 | exit 0 | exit 0 | n/a |
| `yesno` | exit 0 | exit 1 | exit 1 | n/a |
| `menu` | exit 0, item on stdout | exit 1 | exit 1 | n/a |
| `checklist` | exit 0, items on stdout | exit 1 | exit 1 | exit 0, empty stdout |
| `filter` | exit 0, items on stdout | exit 1 | exit 1 | exit 0, empty stdout |
| `filter --multi` | exit 0, items on stdout | exit 1 | exit 1 | exit 0, empty stdout |
| `input` | exit 0, value on stdout | exit 1 | exit 1 | exit 0, empty stdout |
| `password` | exit 0, value on stdout | exit 1 | exit 1 | re-prompt |
| `password-confirm` | exit 0, value on stdout | exit 1 | exit 1 | re-prompt |
| `spin` | n/a | n/a | send SIGTERM to child, wait, exit with child status | n/a |
| `show-file` | n/a | exit pager, exit 0 | exit pager, exit 0 | n/a |
| `toast` | n/a | n/a | exit 0 | n/a |
| `editor` | insert newline | if dirty, first Esc shows toast, second Esc discards and exits; if clean, exit 0 | exit 1 | n/a |
| `explorer` | open file action menu; on a directory, descend (or open action menu if `--dir`) | exit 1 | exit 1 | n/a |
| `hub` | commit, exit 0 | confirm quit, then exit 1 | exit 1 | n/a |

`msg` exits 0 always. Any key event other than resize or focus acknowledges.

`checklist`, `filter`, and `filter --multi` return exit 0 with empty stdout when the user confirms an empty selection.

`input` returns exit 0 with empty stdout on Enter with no text. It does not re-prompt.

`password` re-prompts on empty submission. It never returns an empty value.

`password-confirm` re-prompts on empty submission on either field. A mismatch counts against the attempt counter; empty does not. `--tries N` sets the attempt limit; `--tries 0` allows unlimited attempts. When the limit is reached, exit 1.

`editor` does not auto-save on exit. The only path that writes the file is `Ctrl-S`. `Esc` with unsaved changes shows a toast and stays open. A second `Esc` discards and exits. Any other key between the two Escs re-arms the confirmation.

`toast` exits 0 when its duration expires or when any key is pressed, whichever comes first.

For shell scripts, `|| return 1` is the correct pattern.

### 12.6 Result formatting

`menu` — the chosen item on stdout, followed by a newline.

`checklist`, `filter`, `filter --multi` — one item per line, in the order selected.

`input`, `password`, `password-confirm` — the value on stdout, followed by a newline.

`yesno`, `msg`, `toast` — no output.

`spin` — the child's stdout and stderr are drawn to the framebuffer as rolling lines. Nothing is written to stdout by the verb. The child's exit code becomes the verb's exit code. If the child exits non-zero and `--hold enter` is set (the default when the child fails), the verb waits for a key press before exiting. `--hold N` holds N seconds after the child exits regardless of status. `--hold none` exits immediately. `--where` positions the spinner bar; the default is `BL`.

`show-file` — no output. The pager draws to the tty.

`editor` — no output when a path was given. When invoked with `--new` and no path, the buffer is written to stdout on exit.

`explorer` — the selected path on stdout, followed by a newline. Nothing is written if the user cancels.

`hub` — on Proceed, one `KEY=value` per line, in arbitrary order, to `--out` or stdout. On Esc, no output.

Values must not contain newlines. Verbs that read text reject newlines in submitted values and re-prompt. Pasted text containing newlines is truncated at the first newline.

### 12.7 Theme

```c
typedef struct {
    uint32_t    bg;
    uint32_t    fg;
    uint32_t    accent;
    uint32_t    title;
    uint32_t    muted;
    uint32_t    error;
    uint32_t    warning;
    uint32_t    success;
    uint32_t    modal_bg;
    uint32_t    modal_border;
    uint32_t    modal_title;
    LapkaBorder border;
    const char *cursor;
    const char *check;
    const char *radio_on;
    const char *radio_off;
    const char *bullet;
    const char *arrow;
    bool        owns_strings;
} LapkaTheme;

extern const LapkaTheme LAPKA_THEME_DEFAULT;

uint32_t          lapka_color_darken(uint32_t rgb, int percent);
int               lapka_theme_load(const char *path, LapkaTheme *out);
const LapkaTheme *lapka_theme_active(void);
int               lapka_theme_set_file(const char *path);
void              lapka_theme_reset(void);
void              lapka_theme_free(LapkaTheme *t);
```

`LAPKA_THEME_DEFAULT` is defined in `theme.h`. Its `owns_strings` field is `false`; its string fields point to string literals.

`lapka_color_darken` returns the given color with each RGB channel multiplied by `(100 - percent) / 100`. `percent` is clamped to `[0, 100]`.

A theme file is `KEY=value`, one per line. Blank lines and lines whose first non-whitespace character is `#` are ignored. Values are `0xRRGGBB` or `#RRGGBB` for colors, plain strings for glyphs, and enum names for `border`. Unknown keys are ignored. Missing keys keep the default.

Known keys: `bg`, `fg`, `accent`, `title`, `muted`, `error`, `warning`, `success`, `modal_bg`, `modal_border`, `modal_title`, `border`, `cursor`, `check`, `radio_on`, `radio_off`, `bullet`, `arrow`.

If `modal_bg`, `modal_border`, or `modal_title` are not set, they are derived from `bg` and `title` using `lapka_color_darken`.

`lapka_theme_load` fills `out` from a theme file and sets `out->owns_strings = true`. The caller becomes the owner of `out`'s string fields and must release them with `lapka_theme_free`. A `LapkaTheme` value loaded with `lapka_theme_load` and then copied by value shares the string pointers; `lapka_theme_free` must be called exactly once on the original.

`lapka_theme_free` is a no-op if `t` is NULL or if `t->owns_strings` is `false`. Otherwise it frees the six string fields and sets `owns_strings` back to `false`.

`lapka_theme_active` reads `LAPKA_THEME` from the environment on first call, loads the file, caches the result, and returns a pointer to the cached theme. Every verb reads the active theme through this function. The returned pointer is owned by the library and remains valid until `lapka_theme_set_file` or `lapka_theme_reset` is called.

`lapka_theme_set_file` overrides the active theme for the rest of the process. `lapka_theme_reset` reverts to the default and frees any previously loaded strings.

`--theme=FILE` calls `lapka_theme_set_file` before dispatch, overriding both `LAPKA_THEME` and the default. If the file cannot be read, the verb exits 2.

`GUM_TITLE_COLOR` and `GUM_ACCENT_COLOR`, if set, override `theme.title` and `theme.accent` respectively for the current invocation. `--title-color` overrides `GUM_TITLE_COLOR`. `--color` overrides `GUM_ACCENT_COLOR`. `--border` overrides `theme.border`. These environment overrides are not parsed.

### 12.8 Hint line

Every interactive verb draws a hint line at the bottom of its screen.

```c
typedef struct {
    const char *key;
    const char *label;
} LapkaHintItem;

int lapka_draw_hint(LapkaFramebuffer *fb, int y,
                    const LapkaHintItem *items, int n, uint32_t fg);
```

Keys render in the given `fg` with `LAPKA_ATTR_BOLD`. Labels render in the same `fg` without attributes. Items are separated by two spaces.

`LapkaHintItem` and `lapka_draw_hint` are declared in `src/render/draw.h`, which is not installed as a public header.

Standard hints per verb:

- `menu` — `↑↓ move  enter select  esc cancel`
- `checklist` — `↑↓ move  space toggle  enter confirm  esc cancel`
- `filter` — `↑↓ move  enter select  esc cancel`
- `input`, `password`, `password-confirm` — `enter submit  esc cancel`
- `yesno` — `y yes  n no  esc cancel`
- `show-file` — `/ find  ↑↓ scroll  pgup/pgdn page  home/end  q quit`
- `editor` — `^S save  ^Z undo  ^Y redo  ^D delete-line  esc done`
- `explorer` — `enter open/act  ← parent  . hidden  s sizes  / search  esc cancel`
- `hub` — `↑↓ move  enter edit  tab category  esc quit`

---

## 13. `tui hub`

### 13.1 IN file format

The hub reads a text file that describes both the form's shape and its current values. The file is line-oriented. Blank lines and lines whose first non-whitespace character is `#` are ignored. Leading whitespace on a line is ignored.

Directives:

```
hub TITLE
action NAME
category ID "LABEL"
item key=K label="L" widget=W [field=value ...]
  choice VALUE
  choice VALUE
```

- `hub` sets the title. Optional. If absent, the header shows `Hub`.
- `action` adds a footer button. Order matters. The strings `Proceed` and `Back` are special. Other values become `F<n>` actions that return the string as a result. If no `action` directive is present, the file defaults to `action Back` followed by `action Proceed`.
- `category` starts a new category. First argument is the id, second is the label. Everything after it belongs to that category until the next `category` directive.
- `item` describes one row in the content pane. Fields are space-separated `key=value` pairs. Values with spaces must be quoted with `"`. Backslash escapes `\"` and `\\` inside quoted values.
- `choice` adds one entry to the previous item's choice list. Only valid for `menu`, `multiselect`, and `filter`.

Item fields:

- `key` — required, the state key.
- `label` — required, human label.
- `widget` — required, one of `menu`, `input`, `password`, `yesno`, `multiselect`, `filter`, `subhub`.
- `value` — current value from the state. Optional; if absent, the item is unset.
- `choices_cmd` — a shell command whose stdout lines become the choices. Only valid for `menu` and `multiselect`. Replaces `choice` directives if both are present. Runs once per item, on first focus, and the result is cached for the remainder of the session.
- `placeholder` — greyed text shown inside an empty `input`. Parsed and stored, not rendered.
- `message` — help text shown above the field. Rendered one row above the editor overlay while the item is open.
- `visible_if` — a `KEY=value` condition, or `KEY=v1,v2,v3` for multiple. Item is hidden unless it matches.
- `subhub` — path to another IN file, for `widget=subhub`. If the path is relative, it is resolved against the directory of the IN file that declared the item. For the top-level hub, that is the directory of the file passed to `--in`.

### 13.2 Interface

```
tui hub --in FILE --out FILE [--check]
```

Both `--in` and `--out` are required. There is no spec name, no compiled-in hub table, no library-side knowledge of what the form contains. Bash writes the IN file, the hub reads it, the hub writes the OUT file, bash reads the OUT file.

When `--check` is set, both files must exist and be root-owned regular files with mode 0600 or stricter. Otherwise the verb exits 2.

The keyboard always reads from `/dev/tty`, never stdin.

On Proceed, the hub writes one `KEY=value` per line to `--out`.

On Esc, the hub asks for confirmation and, if confirmed, exits 1 without writing `--out`.

### 13.3 OUT file format

One `KEY=value` per line. No structure, no categories, no widgets. Just the current values of every item the hub touched. Format matches `state.sh`'s `KEY=value` convention so bash can feed it directly to `state_set`.

Values are quoted with `'...'` when they contain characters outside `[A-Za-z0-9_./:@+=]`. Embedded single quotes are escaped as `'\''`.

Values must not contain newlines.

### 13.4 Layout

The hub is a layer stack:

- Layer 0 — the hub body, `OPAQUE`, `PASS`.
  - Header — `FIT`, top.
  - Sidebar — `FIXED 22` wide, full height below header.
  - Content — remaining width, full height below header.
  - Footer — `FIXED 1`, bottom.
- Layer 10 — inline editor overlays, `OPAQUE`, `TRAP`. Sized to the editor's measure plus chrome.
- Layer 20 — modals (quit confirm, error), `OPAQUE`, `TRAP`. Fixed 44 × 7, centered.
- Layer 30 — nested subhubs, `OPAQUE`, `TRAP`. Full screen.

If `screen.w < 55`, the sidebar is hidden and the content pane occupies the full width. If `screen.w < 25` or `screen.h < 8`, the hub refuses to draw and shows the too-small message from section 7.4.

### 13.5 Value visibility

Whenever any item's value changes, the hub re-evaluates every item's `visible_if` condition. An item whose condition no longer matches has its value cleared to empty. This ensures the OUT file does not carry stale values for hidden fields.

The re-evaluation happens inside the value-setting path, so every caller of `value_set` gets the behavior automatically. There is no call site that can forget it.

### 13.6 Subhubs

An item with `widget=subhub` and a `subhub` field pushes the referenced IN file as a full-screen layer on top of the parent. The subhub is a normal hub with its own categories, items, editor stack, and modal stack.

On Proceed, the subhub's item values are merged into the parent's. On Esc, they are discarded. On either path, the subhub's layer is popped and the parent reappears.

Relative `subhub` paths resolve against the directory of the parent IN file.

### 13.7 Bash integration

```bash
cat > /tmp/hub-in <<'EOF'
hub Disk Configuration
action Back
action Proceed
category target "Target"
item key=DISK label="Disk" widget=menu choices_cmd="lsblk -dpno NAME,SIZE,MODEL -e 7" message="All data on this disk will be erased." value=
item key=FS_TYPE label="Filesystem" widget=menu value=ext4
  choice ext4
  choice btrfs
  choice xfs
  choice f2fs
item key=HOSTNAME label="Hostname" widget=input value=artix placeholder=artix
category boot "Boot"
item key=USE_LUKS label="Encrypt root" widget=yesno value=no
item key=LUKS_PASS label="Encryption passphrase" widget=password visible_if="USE_LUKS=yes"
EOF

hub_out=$(mktemp)
tui hub --in=/tmp/hub-in --out="$hub_out" </dev/tty || return 1

while IFS='=' read -r k v; do state_set "$k" "$v"; done < "$hub_out"
```

The hub knows nothing about ArtixForge. Bash writes the shape, the hub renders it, bash reads the values back.

---

## 14. Subprocesses

Used by `spin` and `choices_cmd`.

- Spawn via `posix_spawn` with `POSIX_SPAWN_SETSIGDEF` and a full signal set (`sigfillset`), so every catchable signal is reset to `SIG_DFL` in the child.
- Pipes for stdout and stderr.
- The parent polls its own tty and the child's stdout in the same loop, and checks the resize flag.
- On receipt of byte `0x03` (Ctrl-C), the parent sends `SIGTERM` to the child, waits with a timeout, then `SIGKILL` if needed, and exits with the child's status.
- On parent `SIGINT` (delivered externally), the same shutdown sequence runs.
- On child exit, propagate exit code. `WEXITSTATUS` for normal exit, `128 + signo` for signal.
- No PTY. `spin` captures stdout, not a pseudo-terminal. Commands that require a tty cannot be used with `spin`.
- If the child writes faster than the parent reads, the pipe fills and the child blocks. There is no intermediate buffer.

On resize, the spinner repositions; the child continues unaffected. If the terminal becomes too small, the spinner shows the too-small message and continues reading the child until exit.

`choices_cmd` in the hub runs once per item, on first focus, and its stdout is split on newlines. The command is not run again for the lifetime of the hub session.

---

## 15. Public headers

```
include/lapka/lapka.h
include/lapka/types.h
include/lapka/layer.h
include/lapka/primitive.h
include/lapka/theme.h
include/lapka/verbs.h
```

`types.h` defines `LapkaCell`, `LapkaRect`, `LapkaSize`, `LapkaAnchor`, `LapkaBorder`, `LapkaBox`, `LapkaFramebuffer`, the `LAPKA_ATTR_*` constants, and the event types (`LapkaEventType`, `LapkaKeyCode`, `LapkaMouseAction`, `LapkaEvent`).

`layer.h` defines `LapkaBlend`, `LapkaInput`, `LapkaLayer`, and the compositor functions.

`primitive.h` defines `LapkaPrimitive`, `LapkaPrimitiveKind`, `LapkaSelectMode`, `LapkaListStyle`, `LapkaCursorStyle`, `LapkaScrollStyle`, `LapkaMatcher`, and the primitive constructor functions.

`theme.h` defines `LapkaTheme`, `LAPKA_THEME_DEFAULT`, `lapka_color_darken`, `lapka_theme_load`, `lapka_theme_active`, `lapka_theme_set_file`, `lapka_theme_reset`, and `lapka_theme_free`.

`verbs.h` defines `LapkaInitOptions`, `LapkaRunCallbacks`, and `lapka_run`.

`lapka.h` includes only `types.h`, `theme.h`, and `verbs.h`. `layer.h` and `primitive.h` are not included.

`lapka_draw_text` and `lapka_draw_hint` are declared in `src/render/draw.h`. That header is not installed.

Private headers live under `src/util/`: `buf.h`, `log.h`.

---

## 16. Repository layout

```
lapka/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── VERSION
├── Makefile
├── .gitignore
├── include/lapka/
│   ├── lapka.h
│   ├── types.h
│   ├── layer.h
│   ├── primitive.h
│   ├── theme.h
│   └── verbs.h
├── src/
│   ├── core/
│   │   ├── cell.c
│   │   ├── rect.c
│   │   ├── box.c
│   │   ├── utf8.c
│   │   └── notice.c
│   ├── term/
│   │   ├── term.c
│   │   ├── signals.c
│   │   ├── capabilities.c
│   │   ├── ansi.c
│   │   └── tty.c
│   ├── input/
│   │   └── input.c
│   ├── render/
│   │   ├── framebuffer.c
│   │   ├── compositor.c
│   │   ├── flush.c
│   │   ├── draw.c
│   │   ├── run.c
│   │   └── theme.c
│   ├── primitive/
│   │   ├── primitive.c
│   │   ├── list.c
│   │   ├── text.c
│   │   ├── fuzzy.c
│   │   ├── tabs.c
│   │   ├── progress.c
│   │   ├── summary.c
│   │   ├── tree.c
│   │   ├── notification.c
│   │   ├── pager.c
│   │   └── editor.c
│   ├── proc/
│   │   └── proc.c
│   ├── verbs/
│   │   ├── msg.c
│   │   ├── yesno.c
│   │   ├── list_verb.c
│   │   ├── menu.c
│   │   ├── checklist.c
│   │   ├── filter.c
│   │   ├── input.c
│   │   ├── password.c
│   │   ├── password_confirm.c
│   │   ├── spin.c
│   │   ├── show_file.c
│   │   ├── toast.c
│   │   ├── editor.c
│   │   ├── explorer.c
│   │   ├── hub.c
│   │   ├── hub_parse.c
│   │   └── hub_serialize.c
│   ├── util/
│   │   ├── buf.c
│   │   └── log.c
│   └── main.c
├── tests/
│   └── unit/
├── themes/
│   ├── artix.theme
│   ├── forge.theme
│   ├── jet-black.theme
│   ├── mono.theme
│   └── retro.theme
├── docs/
│   ├── spec.md
│   ├── headers.md
│   └── CONTRIBUTORS.md
└── packaging/
    └── PKGBUILD
```

`src/core/notice.c` contains the CLEAR License v1 notice string with `__attribute__((section(".notice")))`.

---

## 17. Test plan

**Unit.** `utf8.c`, `rect.c`, `box.c`, `fuzzy.c`, `theme.c`. No tty required.

**Verb.** One test per verb, using the scripted-input harness and comparing rendered cells against snapshots. No tty required.

**Signal.** Send `SIGINT` mid-render, verify the terminal is restored and the exit code is `128 + SIGINT`.

**Resize.** For every verb, run under the scripted-input harness, resize the virtual terminal mid-run, assert no crash and correct redraw.

**UTF-8 rendering.** For every codepoint in `EastAsianWidth.txt` classes W and F, and every combining mark range listed in section 9, verify `lapka_cp_width` matches the expected value. For a corpus of mixed-script strings, verify wrapping and cursor arithmetic. Verify that a wide codepoint emits exactly one cell to the terminal and that the following continuation cell is not emitted.

**Fuzz.** Feed random bytes into the escape parser. Assert no crashes and no infinite loops.

**Terminal matrix.** Run every verb under `TERM=xterm`, `TERM=linux`, `TERM=xterm-256color`, `TERM=screen-256color`, `TERM=tmux-256color`, and inside `tmux`. Compare snapshots where they should match. `TERM=linux` output differs from the others because quantization is not implemented.

**Snapshot.** Golden cell buffers for every verb in default layout. Diff on regression.

**Session.** Run three verbs in sequence under `LAPKA_ALT_SCREEN=1`. Assert the alt screen is entered once and left once, and no `\033[?1049h` or `\033[?1049l` appears in the output of the middle verb.

**Password stdout.** Run `password` and `password-confirm` with stdout as a tty, assert exit 2 and no value on screen. Run with stdout as a pipe, assert exit 0 and value captured.

**License.** Verify every `.c` and `.h` file begins with the CLEAR License v1 header.

**No-comments.** Verify no source file contains `//`, `/*`, `TODO`, `FIXME`, `XXX`, or `NOTE`.

**No-external-deps.** Verify `ldd liblapka.so` shows only `libc`, and additionally `libtinfo` and `libpthread` if the corresponding flags were used.

---

## 18. Build order

1. `msg`
2. `yesno`
3. `menu`
4. `input`
5. `checklist`
6. `password`, `password-confirm`
7. Refactor onto the central run loop and framebuffer (section 7.6). Confirm the `LAPKA_ALT_SCREEN=1` session behavior.
8. `filter`
9. `spin`, `show-file`
10. Layer stack and `--where` / `--border` flags
11. `hub`, with the IN-file parser and OUT-file serializer
12. `toast`, `editor`, `explorer`
13. Primitive additions: `tabs`, `progress`, `summary`, `tree`, `notification`, `pager`, `editor`
14. Theme engine: `lapka_theme_load`, `lapka_theme_active`, `--theme` flag, `themes/` directory

Verbs 1 through 6 ship as one-offs to get the installer working quickly. Step 7 retrofits them onto the shared foundation before any further verb is added. From step 7 onward, every interactive verb uses `lapka_run`, the framebuffer, and the compositor where needed. No verb writes escape sequences directly to the tty outside of `lapka_flush` and `lapka_term_init`/`lapka_term_shutdown`.

---

## 19. Bash migration

Every `tui_*` function in `bashisms/tui/core.sh` becomes a two-line wrapper over the corresponding verb. Call sites in `menus/`, `common.sh`, `installer/`, `recovery/`, `poweruser/`, `iso/`, and `migrations/` do not change.

`core.sh` provides `tui_session_begin` and `tui_session_end`. The installer entry point calls `tui_session_begin` once at startup, installs `trap tui_session_end EXIT INT TERM`, and calls `tui_session_end` on normal exit. Every verb invoked between the two calls inherits `LAPKA_ALT_SCREEN=1`.

`core.sh` calls `_term_reset` once at startup, unconditionally, and before every fallback error message. `_term_reset` runs `stty sane` and writes the alt-screen-leave, SGR-reset, and cursor-show escapes to `/dev/tty`.

The binary resolution order is: `$LAPKA_TUI`, then `/usr/local/bin/tui`, then `command -v tui`, then error. The PKGBUILD installs the binary to `/usr/local/bin/tui`.

`tui_password` and `tui_password_confirm` must be called via command substitution so stdout is a pipe:

```bash
pass=$(tui password "Title" "Body") || return 1
```

The hub is called with a bash-generated IN file. Bash owns the shape; the library knows nothing about it.

The theme is selected by pointing `LAPKA_THEME` at a `.theme` file. Once exported, every subsequent `tui` invocation uses that theme for the rest of the install.

---

## 20. Naming

Library: `lapka`. Header: `<lapka/lapka.h>`. Static archive: `liblapka.a`. Shared: `liblapka.so`. Binary: `tui`.

Shared library SONAME: `liblapka.so.0` for major version 0, `liblapka.so.1` for major version 1, and so on. SONAME matches the major version number.

Version scheme: `MAJOR.MINOR.PATCH`. ABI stable at `1.0.0`.

`VERSION` contains a single semver string, `MAJOR.MINOR.PATCH`, no trailing whitespace, no date.

---

## 21. License

All source files begin with the CLEAR License v1 copyright notice and license header. This header is required by the license (Section 1, Attribution) and is the sole permitted non-code text in a source file.

The `LICENSE` file at the repository root contains the full text of the CLEAR License v1.

Contributions are accepted under the terms of Section 6 of the CLEAR License. Contributors retain copyright and grant the Copyright Holder a perpetual, worldwide, non-exclusive, royalty-free license to use, reproduce, modify, and distribute the contribution under the terms of the CLEAR License. Contributions submitted under different terms must be recorded in `docs/CONTRIBUTORS.md`, as required by Section 6.

lapka is distributed as a static archive (`liblapka.a`) and a shared library (`liblapka.so`). The CLEAR License is permissive and is not compatible with copyleft licenses. Section 11 prohibits distributing a Combined Work — a single work that incorporates the Software and material from a copyleft-licensed work in such a manner that the resulting work is treated as a combined or derivative work under applicable copyright law. Distributing lapka alongside such software as separate, independent programs on the same medium is permitted.

The `tui` binary is a compiled artifact of the Software and carries the same license as the library.

Installer scripts that invoke `tui` as a subprocess are Independent Works under the CLEAR License. They incorporate only the Software's generated output and compiled artifacts, not the Software's source code. Attribution and other obligations under the CLEAR License apply to the Software itself, not to Independent Works created through its use.

Modified Versions of lapka distributed publicly must use a Project Name different from `lapka`, or must prominently display a notice that the version is modified and is not the original Software, per Section 3(b).

Factual statements of the form "based on lapka, CLEAR-licensed software by realvolk" are permitted under Section 4.

### 21.1 Downstream visibility

The `README.md` at the repository root states the CLEAR-to-copyleft incompatibility in plain language. The `PKGBUILD` `pkgdesc` names it. The shared library embeds an ASCII-only `NOTICE` string containing the phrase `CLEAR License v1 - not GPL-compatible` in a `.notice` section, retrievable via `strings liblapka.so`. The section is emitted via `__attribute__((section(".notice")))`.

### 21.2 Contributor recording

The file `docs/CONTRIBUTORS.md` exists in the repository from the first commit. It begins with a header line and the statement `No contributions under alternate terms recorded.` It records any contribution submitted under terms other than the CLEAR License v1, as required by Section 6. The file is never deleted, even if its contents become stale, because its presence is itself the record.

---

## 22. Memory model

- Every `_new` function returns an object the caller owns and must free.
- `lapka_list_new` and `lapka_list_set_items` copy the caller's strings. The caller's arrays and strings may be freed immediately after the call returns.
- `lapka_text_new` and `lapka_text_set` copy the value.
- `lapka_text_set_prefix` copies the prefix.
- `lapka_list_result` returns a pointer owned by the list. It is valid until the list is freed or its items are replaced.
- `lapka_editor_text` returns a newly allocated string the caller owns and must free.
- `lapka_tabs_set_child` frees the previous child at that index, if any. Children not installed are owned by the caller.
- `lapka_tree_add` copies the label. `lapka_tree_userdata` returns the pointer set by `lapka_tree_set_userdata`, owned by the caller.
- `lapka_notification_new` copies the message.
- `lapka_pager_set_lines` copies the lines. `lapka_pager_set_file` reads the file and copies its lines.
- `lapka_theme_load` fills the caller's `LapkaTheme` and sets `owns_strings`. The caller must call `lapka_theme_free` to release the string fields. Copying the struct by value shares the string pointers; `lapka_theme_free` must be called exactly once on the original.
- `lapka_layer_new` allocates the buffer. `lapka_layer_resize` reallocates it. `lapka_layer_free` frees it and calls the layer's `free` callback, which is responsible for `userdata` only.
- The compositor owns no memory. It reads layer buffers in place.
- The run loop owns the framebuffer. It is allocated at run entry and freed at run exit.
- Verbs allocate for the duration of their run and free everything before returning.
- The width table for `lapka_cp_width` is a static array. No allocation.
- The paste buffer is a growable buffer owned by the input subsystem. It is freed at teardown.

---

## 23. Contributing

See `CONTRIBUTING.md`. The short version: no comments in code, C99 + POSIX.1-2008, the spec is the source of truth, and a change that adds code adds a test.