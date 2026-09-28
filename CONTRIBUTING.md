# Contributing to lapka

## Before you start

Read `docs/spec.md`. It is the source of truth. If your change contradicts the spec, either the spec is wrong and you should propose a spec change first, or the change is wrong and shouldn't be made. There is no third option.

Read `docs/headers.md` if you're touching the public API. Everything exported is listed there.

## Code policy

**No comments.** This is not a stylistic preference, it is a hard rule checked in CI. The only permitted non-code text in a source file is:

- The CLEAR License v1 copyright notice and license header.
- `#include` guards and `#pragma once`.

No `//`. No `/* */`. No `TODO`, `FIXME`, `XXX`, or `NOTE`. If a symbol needs explanation, its name is wrong. If a section needs explanation, it belongs in the spec.

Unfinished work is tracked in `docs/TODO.md`, not in the code.

## Style

- C99. POSIX.1-2008. No GNU extensions, no C11, no compiler-specific features except where the spec allows them.
- Four-space indent. Braces on the same line as the statement.
- Function names are `lapka_<noun>_<verb>`. Types are `Lapka<Noun>`. Macros are `LAPKA_<NOUN>`.
- Every public function that returns a heap object has a matching `_free`.
- No global mutable state.
- No allocation in draw paths.

The build must succeed with exactly:

```
cc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -c src/main.c -o main.o
```

If your change requires a different invocation, the change is wrong.

## Adding a verb

1. Add it to `docs/spec.md` under section 12.3 with its cancel semantics and result formatting.
2. Create `src/verbs/<name>.c` and `src/verbs/<name>.h`.
3. Implement `cmd_<name>(int argc, char **argv)`.
4. Wire it into `src/main.c`'s dispatch and usage string.
5. Add it to `BIN_SRC` in the Makefile.
6. Add a smoke test under `tests/unit/` if the verb can be tested without a tty.

A verb that is not in the spec does not exist. Add the spec entry first.

## Adding a primitive

1. Add it to `docs/spec.md` under section 11 with its API, keys, and measure rules.
2. Add its kind to `LapkaPrimitiveKind` in `include/lapka/primitive.h`.
3. Declare its constructor and accessors in `include/lapka/primitive.h`.
4. Implement `src/primitive/<name>.c` with the five callbacks: `handle`, `draw`, `measure`, `free`, and a constructor.
5. Add it to `PRIMITIVE_SRC` in the Makefile.
6. Add it to `docs/headers.md`.

The primitive owns everything it allocates. `lapka_primitive_free` frees the primitive and everything it owns. Every pointer the primitive returns is either owned by the primitive or documented in the spec as caller-owned.

## Adding a directive to the hub IN file

The hub IN file format is specified in `docs/spec.md` section 13.1. Adding a directive means:

1. Extending the spec.
2. Extending `src/verbs/hub_parse.c`.
3. Extending `LapkaHubDef` or `LapkaHubItem` in `src/verbs/hub.h`.
4. Ensuring `src/verbs/hub_serialize.c` still produces the same OUT format. Adding to IN does not change OUT.
5. Extending `hub_draw_body` in `src/verbs/hub.c` if the directive affects rendering.

Directives that carry file paths resolve relative paths against the directory of the current IN file.

## Adding or changing a theme key

Theme files are `KEY=value`, one per line. Adding a key means:

1. Extending `docs/spec.md` section 12.7.
2. Adding the field to `LapkaTheme` in `include/lapka/theme.h`.
3. Extending `lapka_theme_load` in `src/render/theme.c` to recognize the key.
4. Updating `LAPKA_THEME_DEFAULT` so the field has a sane default.
5. Adding the key to any shipped file under `themes/` if it should be set.

If the key is a color and is optional, its absent value must fall through to a derived default. Do not require it.

## Testing

`make test` runs everything in `tests/unit/`. Every test that can run without a tty must run without a tty. Tests that need a terminal go in `tests/verbs/` and use the scripted-input harness (not yet written; add it when you add the first such test).

A change that adds code adds a test. A change that fixes a bug adds a test that fails without the fix.

## Submitting

- One change per commit. If you're changing the API and fixing a bug, that's two commits.
- Commit messages describe what changed and why. The why is the part that matters.
- If your change touches the spec, update the spec in the same commit.
- If your change touches the public API, update `docs/headers.md` in the same commit.
- If your change modifies the license header handling, add a line to `docs/CONTRIBUTORS.md` describing the terms you are submitting under. See the license, Section 6.

## License

By submitting a contribution, you agree to Section 6 of the CLEAR License v1. You retain copyright in your contribution and grant the Copyright Holder a perpetual, worldwide, non-exclusive, royalty-free license to use, reproduce, modify, and distribute it under the terms of the CLEAR License.

If you submit under different terms, say so in the pull request and the terms will be recorded in `docs/CONTRIBUTORS.md` as required by Section 6.s