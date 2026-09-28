# Changelog

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