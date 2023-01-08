#include "../lib/err.h"
#include "../lib/layout/layout.h"
#include "ncurses.h"

err_t ncurses_draw_text(void *arg, const char *text, int len, int x, int y, int color, int attrs) {
    err_t err = 0;
    WINDOW *win = NULL;

    ASSERT(arg);

    win = arg;

    if (color)
        wcolor_set(win, color, NULL);

    if (attrs)
        wattr_on(win, attrs, NULL);

    wmove(win, x, y);
    waddnstr(win, text, len);

    if (color)
        wcolor_set(win, 0, NULL);

    if (attrs)
        wattr_off(win, attrs, NULL);

cleanup:
    return err;
}

err_t ncurses_draw_color(void *arg, int x, int y, int width, int height, int color) {
    err_t err = 0;
    WINDOW *win = NULL;

    ASSERT(arg);

    win = arg;

    if (color) {
        WINDOW *tempwin = subwin(win, height, width, x, y);
        wbkgd(tempwin, COLOR_PAIR(color));
        wrefresh(tempwin);
        delwin(tempwin);
    }

cleanup:
    return err;
}

err_t init_ncurses_layout(struct layout **layout, WINDOW *win) {
    err_t err = 0;

    ASSERT(layout);
    ASSERT(win);

    RETHROW(init_layout(layout, ncurses_draw_text, ncurses_draw_color, win));

cleanup:
    return err;
}
