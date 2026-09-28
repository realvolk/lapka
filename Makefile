# Copyright (c) 2026 realvolk (Volk)
#
# This file is part of lapka.
# Distributed under the terms of the CLEAR License v1.
# See LICENSE in the repository root for the full text.

CC      ?= cc
AR      ?= ar
PREFIX  ?= /usr/local
DESTDIR ?=

CFLAGS  = -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Iinclude -Isrc
ARFLAGS = rcs

CORE_SRC = \
	src/core/utf8.c \
	src/core/cell.c \
	src/core/rect.c \
	src/core/box.c \
	src/core/notice.c

TERM_SRC = \
	src/term/ansi.c \
	src/term/capabilities.c \
	src/term/signals.c \
	src/term/term.c \
	src/term/tty.c

INPUT_SRC = \
	src/input/input.c

UTIL_SRC = \
	src/util/buf.c \
	src/util/log.c

RENDER_SRC = \
	src/render/framebuffer.c \
	src/render/flush.c \
	src/render/draw.c \
	src/render/theme.c \
	src/render/run.c \
	src/render/layer.c \
	src/render/compositor.c

PRIMITIVE_SRC = \
	src/primitive/primitive.c \
	src/primitive/list.c \
	src/primitive/text.c \
	src/primitive/fuzzy.c \
	src/primitive/tabs.c \
	src/primitive/progress.c \
	src/primitive/summary.c \
	src/primitive/tree.c \
	src/primitive/notification.c \
	src/primitive/pager.c \
	src/primitive/editor.c

PROC_SRC = \
	src/proc/proc.c

LIB_SRC = $(CORE_SRC) $(TERM_SRC) $(INPUT_SRC) $(UTIL_SRC) $(RENDER_SRC) $(PRIMITIVE_SRC) $(PROC_SRC)
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB     = liblapka.a

BIN_SRC = \
	src/main.c \
	src/verbs/msg.c \
	src/verbs/yesno.c \
	src/verbs/input.c \
	src/verbs/list_verb.c \
	src/verbs/menu.c \
	src/verbs/checklist.c \
	src/verbs/filter.c \
	src/verbs/spin.c \
	src/verbs/show_file.c \
	src/verbs/hub.c \
	src/verbs/hub_parse.c \
	src/verbs/hub_serialize.c \
	src/verbs/password.c \
	src/verbs/password_confirm.c \
	src/verbs/toast.c \
	src/verbs/editor.c \
	src/verbs/explorer.c

BIN_OBJ = $(BIN_SRC:.c=.o)
BIN     = tui

UNIT_TESTS = \
	tests/unit/test_utf8 \
	tests/unit/test_cell \
	tests/unit/test_rect \
	tests/unit/test_box \
	tests/unit/test_ansi \
	tests/unit/test_capabilities \
	tests/unit/test_buf \
	tests/unit/test_framebuffer \
	tests/unit/test_flush \
	tests/unit/test_draw \
	tests/unit/test_list

.PHONY: all lib bin test clean install

all: lib bin

lib: $(LIB)

bin: $(BIN)

$(LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(BIN): $(BIN_OBJ) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(BIN_OBJ) $(LIB)

$(LIB_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(UNIT_TESTS)
	@for t in $(UNIT_TESTS); do \
		printf '== %s\n' "$$t"; \
		./$$t || exit 1; \
	done

tests/unit/test_utf8: tests/unit/test_utf8.c src/core/utf8.c
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/test_cell: tests/unit/test_cell.c src/core/cell.c
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/test_rect: tests/unit/test_rect.c src/core/rect.c
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/test_box: tests/unit/test_box.c src/core/box.c
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/test_ansi: tests/unit/test_ansi.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

tests/unit/test_capabilities: tests/unit/test_capabilities.c src/term/capabilities.c
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/test_buf: tests/unit/test_buf.c src/util/buf.c
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/test_framebuffer: tests/unit/test_framebuffer.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

tests/unit/test_flush: tests/unit/test_flush.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

tests/unit/test_draw: tests/unit/test_draw.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

tests/unit/test_list: tests/unit/test_list.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

clean:
	rm -f $(LIB_OBJ) $(BIN_OBJ) $(LIB) $(BIN)
	rm -f $(UNIT_TESTS)

install: $(BIN) $(LIB)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	install -Dm644 $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB)

ARCH ?= $(shell uname -m)
RELEASE_DIR = release

release: $(BIN) $(LIB)
	@mkdir -p $(RELEASE_DIR)
	strip -o $(RELEASE_DIR)/tui-$(ARCH) $(BIN)
	cp $(LIB) $(RELEASE_DIR)/liblapka-$(ARCH).a
	@cd $(RELEASE_DIR) && sha256sum \
		tui-$(ARCH) \
		liblapka-$(ARCH).a \
		> SHA256SUMS-$(ARCH)
	@printf 'release/%s artifacts written:\n' "$(ARCH)"
	@ls -la $(RELEASE_DIR)