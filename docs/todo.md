# lapka — TODO

Entries are removed when the work lands.

---

## In development — v0.2.0

The v0.2.0 plan exposes the event loop, adds a retained panel tree on top of it, and gives the panel tree a damage model. None of it changes the existing primitives, the compositor, the flush, or the terminal layer.

When v0.2.0 lands, the library has three interfaces over one substrate:

- **Verbs** — `tui <verb> ...`, shell-driven, results on stdout. Current model. Unchanged.
- **Loop** — an embeddable event loop for C programs. The user owns the main loop; lapka owns the terminal, input, framebuffer, and flush.
- **Panels** — a retained tree of boxes with layout, focus, dispatch, and per-panel damage. Built on `loop`. Verbs are rebuilt on top of it.

The verbs continue to work from bash. A C program can use `loop` without `panels`, or `panels` without writing any verbs, or the whole stack as a shell script author calling `tui`.

### Loop API

`src/render/run.c`, new `include/lapka/loop.h`

`lapka_run` becomes a thin wrapper around an exposed loop. The loop owns the terminal, the framebuffer, and the input parser. The caller drives it.

```c
typedef struct LapkaLoop LapkaLoop;

typedef struct {
    bool alt_screen;
    bool mouse;
    bool paste;
    bool focus;
    int  frame_timeout_ms;
} LapkaLoopOptions;

LapkaLoop *lapka_loop_open(const LapkaLoopOptions *opts);
void       lapka_loop_close(LapkaLoop *l);

LapkaFramebuffer *lapka_loop_framebuffer(LapkaLoop *l);
int               lapka_loop_fd(LapkaLoop *l);

bool lapka_loop_resize_pending(LapkaLoop *l);
int  lapka_loop_check_resize(LapkaLoop *l);

int  lapka_loop_draw_begin(LapkaLoop *l);
int  lapka_loop_draw_end(LapkaLoop *l);

int  lapka_loop_poll(LapkaLoop *l, int timeout_ms);
int  lapka_loop_read(LapkaLoop *l, LapkaEvent *ev);
int  lapka_loop_flush(LapkaLoop *l);
```

The loop owns the terminal for its whole lifetime. Nesting a loop inside another loop, or inside a program that owns the terminal itself, is not supported.

### Suspend and resume

`src/render/run.c`

```c
int lapka_loop_suspend(LapkaLoop *l);
int lapka_loop_resume(LapkaLoop *l);
```

`lapka_loop_suspend` restores the terminal to cooked mode, leaves the alt screen if the loop owns it, disarms the exit signal handlers, drains the output, and releases the tty. `lapka_loop_resume` re-opens `/dev/tty` if necessary, re-reads the size, re-enters raw mode and the alt screen, re-installs the signal handlers, and returns the loop to its previous state.

The intended use is running a child process in the terminal and returning:

```c
lapka_loop_suspend(l);
pid_t p = fork();
if (p == 0) { execl("/bin/sh", "sh", NULL); _exit(127); }
int status;
waitpid(p, &status, 0);
lapka_loop_resume(l);
```

Signal handling during the suspend window is the delicate part. `SIGINT`, `SIGTERM`, and `SIGWINCH` must reach the child while lapka is suspended. The exit signal handlers are reset to `SIG_DFL` on suspend and reinstalled on resume. The resize flag is cleared on resume so a resize during suspend is picked up by the next `lapka_loop_check_resize`.

When the loop runs under `LAPKA_ALT_SCREEN=1`, the alt screen is owned by the caller. Suspend does not leave it; resume does not re-enter it.

The ArtixForge use cases are:

- `chroot` into the target and hand the user a shell.
- `$EDITOR` on a config file.
- `less` on the install log.
- `ping` or `curl` for a network test.
- Any child process the installer wants to run interactively.

### Panels API

New `src/render/panel.c`, `include/lapka/panel.h`

A panel is a node in a tree. Each panel has a rect, an optional primitive, an optional user draw callback, an optional user handle callback, and children. Five panel kinds:

- `LEAF` — terminal node. Holds a primitive or a user draw callback.
- `HBOX` — lays children left to right.
- `VBOX` — lays children top to bottom.
- `STACK` — all children at the same rect; the active one draws and receives input.
- `OVERLAY` — children draw in z-order. Events route top-down with per-panel input policy.

```c
typedef struct LapkaPanel LapkaPanel;

typedef enum {
    LAPKA_PANEL_LEAF,
    LAPKA_PANEL_HBOX,
    LAPKA_PANEL_VBOX,
    LAPKA_PANEL_STACK,
    LAPKA_PANEL_OVERLAY,
} LapkaPanelKind;

LapkaPanel *lapka_panel_new(LapkaPanelKind kind);
void        lapka_panel_free(LapkaPanel *p);

void lapka_panel_set_rect(LapkaPanel *p, LapkaRect r);
void lapka_panel_set_box(LapkaPanel *p, LapkaBox box);

void lapka_panel_add(LapkaPanel *parent, LapkaPanel *child);
void lapka_panel_remove(LapkaPanel *parent, LapkaPanel *child);

void lapka_panel_set_primitive(LapkaPanel *p, LapkaPrimitive *prim);
LapkaPrimitive *lapka_panel_primitive(LapkaPanel *p);

void lapka_panel_set_draw(LapkaPanel *p,
    void (*fn)(LapkaPanel *p, LapkaFramebuffer *fb, void *ud), void *ud);
void lapka_panel_set_handle(LapkaPanel *p,
    bool (*fn)(LapkaPanel *p, const LapkaEvent *ev, void *ud), void *ud);

void lapka_panel_draw(LapkaPanel *root, LapkaFramebuffer *fb);
bool lapka_panel_dispatch(LapkaPanel *root, const LapkaEvent *ev);
LapkaPanel *lapka_panel_focus(LapkaPanel *root);
void        lapka_panel_set_focus(LapkaPanel *root, LapkaPanel *p);
```

Layout uses `LapkaBox` and `lapka_place`. Each panel has one box for its placement within the parent's rect. Container kinds split the parent rect among their children according to the children's boxes. There is no grid, no flex, no constraint solver. If a layout cannot be expressed with `LapkaBox` and nesting, the panel system is not the tool for it.

Input routing:

- `HBOX` and `VBOX` — no routing. Each child sees events only when it holds focus.
- `STACK` — events go to the active child.
- `OVERLAY` — events go top-down. The topmost child sees the event first. If it returns `false` and its input policy is `LAPKA_INPUT_PASS`, the next child sees it. If `LAPKA_INPUT_TRAP`, the event is dropped. If `LAPKA_INPUT_NONE`, the panel is skipped. The three input policies are the same ones the layer compositor already uses.

Focus is a tree traversal. `lapka_panel_set_focus` records the currently focused leaf. Panels marked as tab stops participate in `TAB`/`BACKTAB` traversal. This gives form navigation without every primitive implementing it.

### Panel-level damage

`src/render/panel.c`, `src/render/framebuffer.c`

Panels redraw every frame; the framebuffer diffs at flush. That is enough for verbs that draw a still screen. It is not enough for the installer workload: a rolling log, a spinner, a progress bar, or a `pacstrap` output pane updating hundreds of times per second, each frame walking the full framebuffer.

The panel tree carries damage from the leaves up:

- Each panel has a damage flag and a damage rect.
- A primitive or user callback marks damage with `lapka_panel_damage(p)` or `lapka_panel_damage_rect(p, r)`.
- Container kinds accumulate the union of their children's damage and their own.
- `lapka_panel_draw` walks the tree. Panels with no damage are skipped. Panels with damage redraw only within the damaged rect.
- The flush diffs only the damaged region instead of the whole framebuffer.

`lapka_fb_mark_dirty(LapkaFramebuffer *fb, LapkaRect r)` is the new framebuffer entry point. `full_dirty` remains as the "redraw everything" flag used on resize and theme change.

The primitives need to signal damage when their state changes. The API additions are per-primitive `damage` callbacks or a small set of `_touch` accessors. Which one depends on how clean the existing setters can be made. This is the design work of the damage model and it is the largest single piece of v0.2.0.

### `lapka_run_panels`

New function, `src/render/run.c`

```c
int lapka_run_panels(LapkaPanel *root, const LapkaInitOptions *opts, int extra_fd);
```

Same return conventions as `lapka_run`. Same terminal handling, same signal handling, same framebuffer. The verbs are rebuilt on this function.

### Verb refactor

All fourteen verbs.

Each verb becomes a panel tree handed to `lapka_run_panels`. The externally visible behavior does not change: same arguments, same stdout results, same exit codes, same key bindings.

`tui hub` is the largest refactor. Its hand-coded layer stack becomes an `OVERLAY` tree with the same structure: body at the bottom, editor overlay above, modal above that, subhub above that.

`LapkaLayer` becomes an implementation detail of the panel system. The public story is that panels are the user-facing construct. `layer.h` remains for now; whether it stays public is decided when the panel API is stable.

`spin` and `show-file` are the two verbs whose performance depends most on panel-level damage. `spin` reads the child's stdout on every tick and posts the new lines to the loop; without damage the entire ring buffer redraws per line. `show-file` scrolls a pager; without damage the pager redraws the full body per scroll step.

### Optional: threading

`src/input/input.c`, `src/render/run.c`

The current `LAPKA_WITH_PTHREAD` flag spawns one worker thread inside `spin` that reads the child's stdout into a buffer the loop polls. Generalizing this to "any thread can post an event to the loop" is not much more code and it is the right shape for the workloads that are starting to matter: a package download thread, a network-monitor thread, a file-watcher thread.

The design, if it is done:

- The loop thread is the only thread that calls `draw`, `handle`, or `tick`. All user callbacks run on the loop thread.
- A worker thread posts events through a small queue. The queue is a pipe or a lock-free ring; the loop drains it at the top of each frame, before `poll`.
- The `poll` set includes the queue's read fd alongside the tty fd and the optional `extra_fd`.
- `lapka_loop_post(LapkaLoop *l, const LapkaEvent *ev)` is the only thread-safe entry point.

Things that need care:

- **`lapka_theme_active`** is currently lazy and cached in a static. It has to be resolved before any thread starts, or guarded with a mutex.
- **Signal handling** stays as-is. `SIGWINCH` sets a `sig_atomic_t` flag; the loop thread reads it.
- **The framebuffer** is written only by the loop thread.

Whether this lands in v0.2.0 depends on how much the panel damage work costs. If it slips, it goes in v0.3.0.

### Relationship to existing code

Nothing in `core/`, `term/`, `input/`, `proc/`, or `util/` changes beyond what is listed. `render/flush.c`, `render/draw.c`, `render/compositor.c`, and `render/theme.c` do not change, apart from the small change to `flush` to accept a damage region. The primitives do not change, apart from the damage signalling noted above. `LapkaPrimitive` is already a struct with function pointers; a `LEAF` panel holds one without conversion.

The work is:

- Extract the loop from `run.c` into `lapka_loop_*`.
- Add `lapka_loop_suspend` and `lapka_loop_resume`.
- Write `panel.c` and `panel.h`.
- Add panel-level damage; teach the primitives to signal it.
- Add `lapka_fb_mark_dirty`.
- Write `lapka_run_panels`.
- Rewrite each verb against the panel API.
- Update `docs/spec.md`, `docs/headers.md`, and `README.md` for the new public API.

### Out of scope for v0.2.0

- **Multiple terminals.** One tty, one loop.
- **Nesting.** A lapka program inside a lapka program is not a v0.2.0 concern. Signal handling and alt-screen ownership are top-level. The suspend/resume pair is for handing the terminal off to a non-lapka child, not to a second lapka loop.
- **User-registered primitives.** A user can write a `LEAF` panel with a custom `draw` callback that does anything. A full user primitive with `measure` and `handle` is already possible via `LapkaPrimitive`, because it is a public struct. No registration API is planned.

---

## Deferred from v0.1.1

### Color quantization

`src/render/flush.c`

`lapka_flush` accepts a `color_depth` argument and ignores it. Cells are stored in truecolor and emitted as truecolor SGR regardless of the terminal's capability. On `TERM=linux`, `vt100`, `vt102`, and `dumb`, the emitted SGR is not understood and the terminal falls back to default colors.

The specified behavior is in `docs/spec.md` §8.3: quantize to the nearest match in the 256-color or 16-color palette before emitting, and drop colors entirely in mono mode. This needs a palette and a nearest-match routine.

### Bracketed paste

`src/input/input.c`

`opts->paste` enables `\033[?2004h` on the terminal, but the input parser does not accumulate the pasted bytes into a single event. `CSI 200~` and `CSI 201~` parse as `LAPKA_KEY_UNKNOWN`, and the pasted characters arrive as a stream of codepoint events.

The specified behavior is in `docs/spec.md` §10.3: `CSI 200~` starts accumulation into a growable buffer, and everything until `CSI 201~` is emitted as one `LAPKA_EVENT_PASTE`. `LAPKA_EVENT_PASTE` is declared in `types.h`.

### `LapkaText` placeholder

`src/primitive/text.c`

The hub's `placeholder` item field is parsed and stored but never rendered. `LapkaText` has no placeholder concept.

This needs a field on `TextData`, a setter, and draw logic that renders muted text when the value is empty and the cursor is at position 0.

Until then, `placeholder` in a hub IN file is accepted and ignored.

### Public `draw.h`

`src/render/draw.h`

`lapka_draw_text`, `lapka_draw_box`, `lapka_draw_hline`, `lapka_draw_hint`, `lapka_put_cp`, and `lapka_draw_str` are declared in `src/render/draw.h` and used by the verbs and primitives, but the header is not installed. The minimal-program example in `docs/headers.md` depends on `lapka_draw_text`.

Two options: promote `draw.h` to `include/lapka/draw.h`, add it to `lapka.h`, and document it; or move the draw helpers into an existing public header.

### Common flags

`docs/spec.md` §12.2 lists a common flag vocabulary that includes `--alt-screen`, `--mouse`, `--size`, `--width`, `--height`, `--border`, `--color`, and `--title-color`. Only `--theme` and `--where` are parsed.

Either implement the remaining flags or remove them from the spec.

### `spin` signal propagation

`src/verbs/spin.c`

`Ctrl-C` sends `SIGTERM` to the direct child only. A child that spawns its own children, such as `sh -c 'sleep 1000'`, leaves the grandchild running. Correct propagation needs a process group.

### `toast` on the main screen

`src/verbs/toast.c`

`toast` runs with `alt_screen` disabled. On a terminal that is not already in the alt screen, the toast pixels persist after exit until something else redraws.

### Pager wrap-boundary highlighting

`src/primitive/pager.c`

A match that straddles a wrap boundary is not highlighted at all. `docs/spec.md` §11.8 specifies that the first visual row of such a match is highlighted.

### `lapka_draw_text` wrapping

`src/render/draw.c`

`lapka_draw_text` wraps at cell boundaries, not word boundaries. `docs/spec.md` §8.4 specifies word wrapping. The difference is visible for prose text in `msg` and `yesno` bodies.