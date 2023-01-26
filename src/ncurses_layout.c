#include "../lib/err.h"
#include "../lib/layout/layout.h"
#include "ncurses.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

err_t ncurses_draw_text(void *arg, const char *text, uint32_t len, uint32_t col, uint32_t row, int color, int attrs) {
    err_t err = 0;
    WINDOW *win = NULL;

    ASSERT(arg);

    win = arg;

#ifdef CHECK_BOUNDS
    ASSERT(col + len <= getmaxx(win));
    ASSERT(row < getmaxy(win));
#else
    len = MIN(len, getmaxx(win) - col);
    if (row >= (uint32_t)getmaxy(win))
        goto cleanup;
#endif

    if (color)
        wcolor_set(win, color, NULL);

    if (attrs)
        wattr_on(win, attrs, NULL);

    wmove(win, row, col);
    waddnstr(win, text, len);

    if (color)
        wcolor_set(win, 0, NULL);

    if (attrs)
        wattr_off(win, attrs, NULL);

cleanup:
    return err;
}

err_t ncurses_draw_color(void *arg, uint32_t col, uint32_t row, uint32_t width, uint32_t height, int color) {
    err_t err = 0;
    WINDOW *win = NULL;

    ASSERT(arg);

    win = arg;

#ifdef CHECK_BOUNDS
    ASSERT(col + width <= getmaxx(win));
    ASSERT(row + height <= getmaxy(win));
#else
    width = MIN(width, getmaxx(win) - col);
    height = MIN(height, getmaxy(win) - row);
#endif

    if (color) {
        WINDOW *tempwin = subwin(win, height, width, row, col);
        wbkgd(tempwin, COLOR_PAIR(color));
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
