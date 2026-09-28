/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LAPKA_ATTR_BOLD      0x0001
#define LAPKA_ATTR_DIM       0x0002
#define LAPKA_ATTR_ITALIC    0x0004
#define LAPKA_ATTR_UNDERLINE 0x0008
#define LAPKA_ATTR_BLINK     0x0010
#define LAPKA_ATTR_REVERSE   0x0020
#define LAPKA_ATTR_STRIKE    0x0040

typedef struct {
    uint32_t cp;
    uint32_t fg;
    uint32_t bg;
    uint16_t attrs;
} LapkaCell;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} LapkaRect;

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

typedef enum {
    LAPKA_ANCHOR_TL, LAPKA_ANCHOR_TC, LAPKA_ANCHOR_TR,
    LAPKA_ANCHOR_LC, LAPKA_ANCHOR_CC, LAPKA_ANCHOR_RC,
    LAPKA_ANCHOR_BL, LAPKA_ANCHOR_BC, LAPKA_ANCHOR_BR
} LapkaAnchor;

typedef enum {
    LAPKA_BORDER_NONE,
    LAPKA_BORDER_SINGLE,
    LAPKA_BORDER_ROUNDED,
    LAPKA_BORDER_DOUBLE
} LapkaBorder;

typedef struct {
    LapkaAnchor anchor;
    LapkaSize   width;
    LapkaSize   height;
    LapkaBorder border;
    int         pad[4];
    int         margin[4];
} LapkaBox;

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

typedef struct {
    int         w;
    int         h;
    LapkaCell  *cur;
    LapkaCell  *prev;
    bool        full_dirty;
} LapkaFramebuffer;

LapkaRect lapka_place(LapkaRect parent, LapkaBox box, int natural_w, int natural_h);