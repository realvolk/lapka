/* Copyright (c) 2026 realvolk (Volk)
 *
 * This file is part of lapka.
 * Distributed under the terms of the CLEAR License v1.
 * See LICENSE in the repository root for the full text.
 */

#include <lapka/primitive.h>
#include <lapka/theme.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

static void check_str(const char *got, const char *want, const char *what) {
    const char *g = got ? got : "(null)";
    const char *w = want ? want : "(null)";
    if (strcmp(g, w) != 0) {
        fprintf(stderr, "FAIL %s: got \"%s\", want \"%s\"\n", what, g, w);
        failures++;
    }
}

static LapkaEvent key_event(LapkaKeyCode code) {
    LapkaEvent e;
    memset(&e, 0, sizeof(e));
    e.type = LAPKA_EVENT_KEY;
    e.key.code = code;
    return e;
}

static LapkaEvent char_event(uint32_t ch) {
    LapkaEvent e;
    memset(&e, 0, sizeof(e));
    e.type = LAPKA_EVENT_KEY;
    e.key.code = LAPKA_KEY_CODEPOINT;
    e.key.ch = ch;
    return e;
}

static const char *const items3[] = { "alpha", "beta", "gamma" };
static const char *const items5[] = { "one", "two", "three", "four", "five" };

static void test_new_and_basic(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);
    check(l != NULL, "list new");
    check(l->kind == LAPKA_PRIMITIVE_LIST, "list kind");
    check(lapka_list_cursor(l) == 0, "cursor starts at 0");
    check_str(lapka_list_cursor_item(l), "alpha", "cursor item");
    check(lapka_list_result_count(l) == 1, "result count one");

    const char *r0 = lapka_list_result(l, 0);
    check_str(r0, "alpha", "result 0");

    check_str(lapka_list_result(l, 1), NULL, "result out of range");
    check_str(lapka_list_result(l, -1), NULL, "result negative");

    lapka_list_free(l);
}

static void test_cursor_moves(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);

    LapkaEvent down = key_event(LAPKA_KEY_DOWN);
    check(l->handle(l, &down) == true, "down handled");
    check(lapka_list_cursor(l) == 1, "cursor at 1");

    l->handle(l, &down);
    check(lapka_list_cursor(l) == 2, "cursor at 2");

    l->handle(l, &down);
    check(lapka_list_cursor(l) == 2, "clamps at last");

    LapkaEvent up = key_event(LAPKA_KEY_UP);
    l->handle(l, &up);
    check(lapka_list_cursor(l) == 1, "cursor back at 1");

    l->handle(l, &up);
    l->handle(l, &up);
    check(lapka_list_cursor(l) == 0, "clamps at first");

    lapka_list_free(l);
}

static void test_home_end(void) {
    LapkaPrimitive *l = lapka_list_new(items5, 5, LAPKA_SELECT_ONE);

    LapkaEvent end = key_event(LAPKA_KEY_END);
    l->handle(l, &end);
    check(lapka_list_cursor(l) == 4, "end goes to last");

    LapkaEvent home = key_event(LAPKA_KEY_HOME);
    l->handle(l, &home);
    check(lapka_list_cursor(l) == 0, "home goes to first");

    lapka_list_free(l);
}

static void test_page(void) {
    LapkaPrimitive *l = lapka_list_new(items5, 5, LAPKA_SELECT_ONE);

    LapkaCell cell;
    cell.cp = 0;
    int w = 0, h = 0;

    LapkaCell buf[100];
    memset(buf, 0, sizeof(buf));
    l->draw(l, buf, 10, 5, &LAPKA_THEME_DEFAULT);
    l->measure(l, 80, 24, &w, &h);
    check(h <= 24, "measure fits");

    LapkaEvent pgdn = key_event(LAPKA_KEY_PAGEDOWN);
    l->handle(l, &pgdn);
    check(lapka_list_cursor(l) == 4, "pagedown to end when rows=5");

    LapkaEvent pgup = key_event(LAPKA_KEY_PAGEUP);
    l->handle(l, &pgup);
    check(lapka_list_cursor(l) == 0, "pageup to start");

    lapka_list_free(l);
}

static void test_selection_one(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);

    LapkaEvent down = key_event(LAPKA_KEY_DOWN);
    l->handle(l, &down);

    check(lapka_list_result_count(l) == 1, "one selected");
    const char *sel = lapka_list_result(l, 0);
    check_str(sel, "beta", "selected beta");

    lapka_list_free(l);
}

static void test_selection_many(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_MANY);

    check(lapka_list_result_count(l) == 0, "nothing selected initially");

    LapkaEvent space = char_event(' ');
    LapkaEvent down = key_event(LAPKA_KEY_DOWN);

    l->handle(l, &space);
    check(lapka_list_result_count(l) == 1, "selected one");
    check_str(lapka_list_result(l, 0), "alpha", "first is alpha");

    l->handle(l, &down);
    l->handle(l, &space);
    check(lapka_list_result_count(l) == 2, "selected two");
    check_str(lapka_list_result(l, 0), "alpha", "order 0 alpha");
    check_str(lapka_list_result(l, 1), "beta", "order 1 beta");

    l->handle(l, &down);
    l->handle(l, &space);
    check(lapka_list_result_count(l) == 3, "selected three");

    l->handle(l, &space);
    check(lapka_list_result_count(l) == 2, "unchecked gamma");
    check_str(lapka_list_result(l, 0), "alpha", "still alpha");
    check_str(lapka_list_result(l, 1), "beta", "still beta");

    l->handle(l, &space);
    check(lapka_list_result_count(l) == 3, "rechecked gamma");
    check_str(lapka_list_result(l, 2), "gamma", "gamma back at end");

    lapka_list_free(l);
}

static void test_set_items(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);

    LapkaEvent down = key_event(LAPKA_KEY_DOWN);
    l->handle(l, &down);
    check(lapka_list_cursor(l) == 1, "cursor at 1 before set_items");

    lapka_list_set_items(l, items5, 5);
    check(lapka_list_cursor(l) == 0, "cursor reset after set_items");
    check(lapka_list_result_count(l) == 1, "still one after set_items");
    check_str(lapka_list_cursor_item(l), "one", "new first item");

    lapka_list_free(l);
}

static void test_style_set(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);

    LapkaListStyle s = LAPKA_LIST_STYLE_DEFAULT;
    s.cursor_style = LAPKA_CURSOR_INVERSE;
    s.cursor_attrs = 0;
    s.scroll = LAPKA_SCROLL_NONE;

    lapka_list_set_style(l, &s);

    LapkaCell buf[100];
    memset(buf, 0, sizeof(buf));
    l->draw(l, buf, 10, 5, &LAPKA_THEME_DEFAULT);

    lapka_list_free(l);
}

static void test_empty_list(void) {
    LapkaPrimitive *l = lapka_list_new(NULL, 0, LAPKA_SELECT_ONE);
    check(l != NULL, "empty list new");
    check(lapka_list_cursor_item(l) == NULL, "empty cursor item null");
    check(lapka_list_result_count(l) == 0, "empty result count");

    LapkaEvent down = key_event(LAPKA_KEY_DOWN);
    l->handle(l, &down);
    check(lapka_list_cursor(l) == 0, "empty cursor stays 0");

    LapkaCell buf[100];
    memset(buf, 0, sizeof(buf));
    l->draw(l, buf, 10, 5, &LAPKA_THEME_DEFAULT);

    lapka_list_free(l);
}

static void test_measure(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);
    int w = 0, h = 0;
    l->measure(l, 80, 24, &w, &h);
    check(h == 3, "measure height 3");
    check(w >= 6, "measure width covers beta+padding");
    lapka_list_free(l);
}

static void test_handle_returns_false_for_unrelated(void) {
    LapkaPrimitive *l = lapka_list_new(items3, 3, LAPKA_SELECT_ONE);
    LapkaEvent e = key_event(LAPKA_KEY_TAB);
    check(l->handle(l, &e) == false, "tab unhandled");
    e = char_event('x');
    check(l->handle(l, &e) == false, "char x unhandled");
    lapka_list_free(l);
}

int main(void) {
    test_new_and_basic();
    test_cursor_moves();
    test_home_end();
    test_page();
    test_selection_one();
    test_selection_many();
    test_set_items();
    test_style_set();
    test_empty_list();
    test_measure();
    test_handle_returns_false_for_unrelated();

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}