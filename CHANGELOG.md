# Changelog

## [0.1.1] - 2026-10-01

### Fixed

**Library**

- `lapka_cp_width` reports U+3099 and U+309A (combining kana voicing marks) as width 0. They were classified as width 2, which advanced the cursor by an extra cell and dropped the following cell.
- The flush routine normalizes U+0009 (tab) to U+0020 (space) at emit time. A raw tab byte written to the terminal moved the cursor to the next tab stop instead of occupying one cell.
- `lapka_proc_start` initializes the `LapkaProc` to a known state before spawning, so a caller that inspects it after a failed `posix_spawnp` sees defined values.
- `lapka_proc_start` resets every catchable signal to `SIG_DFL` in the child, not just `SIGINT`, `SIGTERM`, and `SIGPIPE`. A child that crashed previously ran the parent's exit handler.
- `lapka_composite`'s layer comparator no longer subtracts `z` values directly. Signed overflow was possible; the comparator is overflow-safe.
- The CSI parser in `src/input/input.c` drains trailing bytes after the parameters of a multi-field escape sequence. Sequences with more than two `;`-separated fields, such as the modifyOtherKeys form for Shift+Enter, previously left their tail in the input buffer, and the tail streamed through as codepoint events.
- `LapkaTheme` owns its string fields. `lapka_theme_load` sets `owns_strings` on the output, and `lapka_theme_free` releases the strings. The string fields previously lived in process-global slots, so loading a second theme invalidated the first.
- `lapka_theme_reset` releases the active theme's string fields and the cached path.

**Verbs**

- `tui password` and `tui password-confirm` refuse before collecting input when stdout is a terminal. They previously prompted the user, collected the secret, and only then exited 2.
- `tui password-confirm` no longer leaks the value returned by a discarded prompt when the attempt limit is reached.
- `tui input` returns exit 0 with empty stdout when Enter is pressed on an empty field. The Enter was previously ignored.
- `tui toast` exits on any key press, in addition to its duration expiry. A long-duration toast could previously not be dismissed.
- `tui editor` exits on a second Esc when the buffer is dirty. Esc previously re-showed the confirmation toast indefinitely, and the only exit was `Ctrl-S` or `Ctrl-C`.
- `tui explorer` no longer dereferences an uninitialized entry pointer when opening the action menu on a file.
- `tui explorer` opens the action menu on a directory when `--dir` is given, making directory selection reachable. Enter previously always descended.
- `tui explorer`'s in-view editor exits on a second Esc when the buffer is dirty.
- `tui hub` resolves relative `subhub` paths in the top-level IN file against the IN file's directory. They previously resolved against the current working directory.
- `tui hub` caches the result of `choices_cmd` per item for the lifetime of the session. The command previously ran on every editor open.
- `tui hub` renders an item's `message` field above the open editor.
- `tui hub` no longer allocates a scratch buffer per editor frame; the buffer is reused across frames at a fixed size.
- `tui hub` reports to stderr when the subhub layer stack limit is reached.
- `tui hub` clears a nested subhub's layer buffer before each redraw. The buffer was previously not cleared, so content from earlier frames persisted in cells that a later frame did not overwrite.
- `tui hub` accepts Enter on the quit-confirmation modal. The modal previously matched `\r` and `\n` against `LAPKA_KEY_CODEPOINT`, but the parser emits those as `LAPKA_KEY_ENTER`, so the check never fired.
- `tui hub` merges subhub items whose keys do not exist in the parent. A subhub's key that the parent did not declare is now carried to the OUT file. It was previously dropped by `value_set`, which does nothing when the key is not found.

### Changed

- The `put_cp` and `draw_str` cell-writing helpers used by the primitives are provided by `lapka_put_cp` and `lapka_draw_str` in `src/render/draw.h`. The eight per-file copies have been removed.

### Removed

- `LapkaBlock` and `LapkaPrompt`. Both were declared in the public API and documented, but neither had an implementation and nothing referenced them.
- `tui filter --placeholder`. The flag was accepted and ignored. `LapkaText` has no placeholder concept; if one is added, the flag can return.

### Documentation

- `docs/spec.md` and `docs/headers.md` updated to reflect the current implementation. Sections covering color quantization, bracketed paste, focus events, tab rendering, the UTF-8 width table, and the box model's `FIXED` mode describe the code as it exists.
- `docs/TODO.md` added, listing the deferred v0.1.1 items and the v0.2.0 plan.

---

## [0.1.0] - 2026-09-28

Initial release.

### Added

**Library**

- `LapkaCell`, `LapkaRect`, `LapkaSize`, `LapkaAnchor`, `LapkaBorder`, `LapkaBox`, `LapkaFramebuffer`.
- Cell-based rendering with a damage-diffed flush. No full-screen clear on redraw.
- UTF-8 decode, encode, and codepoint width table. East Asian wide ranges handled. Continuation cells are never emitted to the terminal.
- Layer compositor. `OPAQUE` and `TRANSPARENT` blend, z-ordering, input policies (`NONE`, `PASS`, `TRAP`).
- Terminal layer. Raw mode, alt screen, cursor control, synchronized output, SGR mouse, bracketed paste, focus events, signal-safe teardown, resize handling via `SIGWINCH`.
- Central run loop: `lapka_run`. Every interactive verb uses it. No verb implements its own event loop.

**Primitives**

- `LapkaBlock` — static multi-line text with word wrapping.
- `LapkaList` — one-of-N or many-of-N. Fuzzy filter hook, cursor styles, scroll styles, mouse.
- `LapkaText` — one-line edit buffer, codepoint-indexed cursor, mask mode, change callback, prefix.
- `LapkaPrompt` — one-line hint.
- `LapkaTabs` — container of N primitives, `F1`..`F9` switching.
- `LapkaProgress` — determinate bar.
- `LapkaSummary` — scrolling text body.
- `LapkaTree` — hierarchical list with expand/collapse.
- `LapkaNotification` — auto-expiring message box.
- `LapkaPager` — scrollable text with search and match highlighting.
- `LapkaEditor` — multi-line editor with undo and redo, capped at 200 snapshots.

**Verbs**

- `tui msg` — informational message.
- `tui yesno` — yes/no prompt.
- `tui menu` — one-of-N.
- `tui checklist` — many-of-N.
- `tui filter` — fuzzy filter over stdin, single or multi-select.
- `tui input` — free text with optional default.
- `tui password` — masked input. Refuses when stdout is a terminal.
- `tui password-confirm` — masked input with confirmation and `--tries`.
- `tui spin` — long-running command with a spinner and rolling output. `--where`, `--hold`.
- `tui show-file` — pager with search.
- `tui toast` — non-blocking notification.
- `tui editor` — full-screen editor with undo/redo and read-only detection.
- `tui explorer` — file and directory browser with search, action menu, and permission checks.
- `tui hub` — form engine driven by a bash-written IN file. Nested subhubs. `visible_if` with automatic clearing of hidden values. `choices_cmd`. `--check` for root-owned state files.

**Themes**

- `KEY=value` theme files.
- `--theme=FILE` on every verb.
- `LAPKA_THEME` environment variable.
- `lapka_theme_load`, `lapka_theme_active`, `lapka_theme_set_file`, `lapka_theme_reset`.
- `lapka_color_darken` for deriving modal colors from the palette.
- Shipped themes: `forge`, `artix`, `jet-black`, `mono`, `retro`.

**Build**

- Static library `liblapka.a`.
- Binary `tui`.
- Makefile with `all`, `lib`, `bin`, `test`, `clean`, `install`, `release`.
- Release target stages `release/tui-$(ARCH)`, `release/liblapka-$(ARCH).a`, and `release/SHA256SUMS-$(ARCH)`.

**Specification**

- `docs/spec.md` covering the data model, layer model, terminal layer, rendering, UTF-8, input, primitives, verbs, the hub IN file format, subprocess handling, the public headers, repository layout, and the test plan.
- `docs/headers.md` covering every public symbol.
- `docs/CONTRIBUTORS.md` for contributions under alternate terms.
- `CONTRIBUTING.md` for contributors.

**License**

- CLEAR License v1.
- NOTICE string embedded in the shared library.