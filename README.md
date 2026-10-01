# lapka

A layered terminal compositor library for C99 and POSIX.1-2008. Ships a binary called `tui` and a static archive called `liblapka.a`. No dependencies. No comments in the code.

## What it is

- **Fourteen verbs**, called from bash as `tui <verb> ...`. Each prints a result on stdout and exits with a code bash can `case` on.
- **Nine primitives**, linked into your own program as `liblapka.a`. `LapkaList`, `LapkaText`, `LapkaTabs`, `LapkaProgress`, `LapkaSummary`, `LapkaTree`, `LapkaNotification`, `LapkaPager`, `LapkaEditor`.
- **A layer compositor** with `OPAQUE` and `TRANSPARENT` blending, z-ordering, and input routing across layers.
- **UTF-8** everywhere. Wide characters, combining marks, CJK, emoji. Codepoint-based cursor arithmetic.
- **Damage-diffed rendering.** No flicker. No `\033[2J`.
- **Signal-safe teardown.** The terminal is restored on every exit path, including `SIGSEGV`.

## Build

```sh
make
make test
```

Produces `liblapka.a` and `tui`.

## Use it from bash

Every verb takes a title, a body, and its arguments. Results come back on stdout.

```bash
# Message. Waits for Enter before returning.
tui msg "Notice" "The disk will be erased."

# Yes / no
if tui yesno "Confirm" "Erase /dev/sda?"; then
    wipefs -a /dev/sda
fi

# One-of-N
disk=$(tui menu "Disk" "Choose target drive:" /dev/sda /dev/nvme0n1 /dev/sdb)

# Multi-select with space to toggle
groups=$(tui checklist "Groups" "Pick groups for artix:" \
    wheel audio video storage lp network)

# Fuzzy multi-select from stdin
packages=$(printf '%s\n' git vim neovim tmux firefox \
    | tui filter "Packages" "Type to search:" --multi)

# Free text with a default
hostname=$(tui input "Hostname" "Enter system hostname:" --default artix)

# Masked input. Must be captured; stdout cannot be a tty.
password=$(tui password "Root" "Enter root password:") || exit 1

# Confirmed password. Retries up to 3 times by default.
password=$(tui password-confirm "Root" "Enter root password:" --tries 3)

# Long-running command with a spinner and rolling output
tui spin "Installing" "Base system" -- pacstrap /mnt base linux linux-firmware

# Pager with search
tui show-file "Install log" /tmp/install.log

# Full-screen editor
tui editor /etc/hostname

# File and directory browser
path=$(tui explorer /etc) || exit 1

# Non-blocking toast in the bottom-right corner
tui toast "Config saved" --duration=2000

# Hub. Form engine driven by a text file.
tui hub --in=/tmp/hub-in --out=/tmp/hub-out </dev/tty || exit 1
```

Every verb accepts `--theme=FILE`, overriding `LAPKA_THEME`. `spin` also accepts `--where=TL|TC|TR|LC|CC|RC|BL|BC|BR` and `--hold N|enter|none`.

### Session mode

By default each verb enters and leaves the alt screen itself. To run several verbs back-to-back with no screen toggle between them, hold the alt screen from bash:

```bash
tui_session_begin() {
    printf '\033[?1049h\033[H\033[2J' > /dev/tty
    export LAPKA_ALT_SCREEN=1
}

tui_session_end() {
    printf '\033[?1049l' > /dev/tty
    unset LAPKA_ALT_SCREEN
}

trap tui_session_end EXIT INT TERM
tui_session_begin

disk=$(tui menu "Disk" "Pick:" /dev/sda /dev/nvme0n1)
hostname=$(tui input "Hostname" "Enter:")
tui yesno "Confirm" "Proceed?"

tui_session_end
```

## Use it as a library

```c
#include <lapka/lapka.h>
#include <stdio.h>

typedef struct {
    LapkaPrimitive *list;
    int             exit_code;
} App;

static void app_draw(void *ud, LapkaFramebuffer *fb) {
    App *a = ud;
    const LapkaTheme *theme = lapka_theme_active();

    LapkaRect title_r = { 0, 0, fb->w, 1 };
    lapka_draw_text(fb, title_r, "-- Pick one --", theme->title, 0, LAPKA_ATTR_BOLD);

    if (fb->h > 2) {
        LapkaCell *sub = fb->cur + (size_t)fb->w * 2;
        a->list->draw(a->list, sub, fb->w, fb->h - 2, theme);
    }
}

static bool app_handle(void *ud, const LapkaEvent *ev) {
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
    App a = { .list = lapka_list_new(items, 3, LAPKA_SELECT_ONE), .exit_code = 2 };
    if (!a.list) return 2;

    LapkaInitOptions opts = { 1, 1, 1, 0 };
    LapkaRunCallbacks cb = { app_draw, app_handle, NULL, &a };
    int rc = lapka_run(&opts, &cb, -1);

    if (rc == 0 && a.exit_code == 0) {
        printf("%s\n", lapka_list_cursor_item(a.list));
    }

    lapka_primitive_free(a.list);
    return rc != 0 ? rc : a.exit_code;
}
```

`lapka_draw_text` is declared in `src/render/draw.h`, which is not installed. To build this example against the current tree, copy `src/render/draw.h` into your include path or include the source.

Build with:

```sh
cc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 myapp.c liblapka.a -o myapp
```

## The hub

`tui hub` renders a form described by a text file that bash generates. There is no compiled-in form table.

```bash
cat > /tmp/hub-in <<'EOF'
hub Disk Configuration
action Back
action Proceed
category target "Target"
item key=DISK label="Disk" widget=menu choices_cmd="lsblk -dpno NAME,SIZE,MODEL -e 7" value=
item key=FS_TYPE label="Filesystem" widget=menu value=ext4
  choice ext4
  choice btrfs
  choice xfs
  choice f2fs
item key=HOSTNAME label="Hostname" widget=input value=artix
category boot "Boot"
item key=USE_LUKS label="Encrypt root" widget=yesno value=no
item key=LUKS_PASS label="Passphrase" widget=password visible_if="USE_LUKS=yes"
EOF

touch /tmp/hub-out
tui hub --in=/tmp/hub-in --out=/tmp/hub-out </dev/tty || exit 1

while IFS='=' read -r k v; do state_set "$k" "$v"; done < /tmp/hub-out
```

Values come back as flat `KEY=value` lines for `state_set`. Items with `visible_if` are hidden unless their condition matches; hidden items have their values cleared automatically.

Items can also point at another hub file:

```
item key=MANAGE label="Users" widget=subhub subhub="users.hub"
```

`users.hub` is a full hub in its own right. It opens as a full-screen layer on top of the parent. On Proceed, its values merge into the parent's; on Esc, they are discarded. A relative `subhub` path resolves against the directory of the IN file that declared it.

`--check` makes the hub refuse to open `--in` or `--out` unless both are root-owned regular files with mode 0600 or stricter. Use it when the state contains passwords.

## Themes

Every verb accepts `--theme=FILE`, and reads `LAPKA_THEME` from the environment if the flag is absent. The theme file is `KEY=value`, one per line. Values are `0xRRGGBB` for colors, plain strings for glyphs, enum names for borders.

```
bg=0x000000
fg=0xFFFFFF
accent=0x00AF00
title=0xFF87D7
muted=0x888888
error=0xCC0000
warning=0xCCCC00
success=0x00AF00
modal_bg=0x000000
modal_border=0x9F5A85
modal_title=0xC06BA5
border=single
cursor=|
check=x
radio_on=(*)
radio_off=( )
bullet=*
arrow=>
```

Unknown keys are ignored. Missing keys keep the default. If `modal_bg`, `modal_border`, or `modal_title` are not set, they are derived from `bg` and `title`.

## Layout

Every verb uses one placement vocabulary. Nine anchors, four size modes, four border styles, padding, margins.

```c
LapkaBox box = {
    .anchor = LAPKA_ANCHOR_CC,
    .width  = { LAPKA_SIZE_PERCENT, 60 },
    .height = { LAPKA_SIZE_FIT, 0 },
    .border = LAPKA_BORDER_ROUNDED,
    .pad    = { 1, 2, 1, 2 },
};
LapkaRect r = lapka_place(screen, box, natural_w, natural_h);
```

Layers compose with `OPAQUE` (solid panels, modals, editors) and `TRANSPARENT` (toasts, floating hints). No dimming; a modal stands out by drawing a solid rectangle of a different color on top.

## Docs

- `docs/spec.md` — the specification.
- `docs/headers.md` — every public symbol grouped by header.
- `docs/TODO.md` — deferred work.
- `docs/CONTRIBUTORS.md` — contributions under alternate terms.
- `CONTRIBUTING.md` — how to contribute.

## License

CLEAR License v1. Permissive, not compatible with copyleft. See `LICENSE`.

Linking `liblapka.a` or `liblapka.so` into a program licensed under the GPL, LGPL, AGPL, or a compatible copyleft license may constitute a Combined Work under Section 11 and is not permitted without separate written permission. Invoking `tui` as a subprocess from a shell script is not a Combined Work.