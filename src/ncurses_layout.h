#ifndef GIT_LIVE_NCURSES_LAYOUT_H
#define GIT_LIVE_NCURSES_LAYOUT_H

#include "../lib/err.h"
#include "ncurses.h"
#include "../lib/layout/layout.h"

err_t init_ncurses_layout(struct layout** layout, WINDOW* win);

#endif // GIT_LIVE_NCURSES_LAYOUT_H