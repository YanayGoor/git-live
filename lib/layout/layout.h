#ifndef GIT_LIVE_LAYOUT_H
#define GIT_LIVE_LAYOUT_H

#include <sys/queue.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <ncurses.h>
#include "../err.h"

enum nodes_direction {
  nodes_direction_none = 0,
  nodes_direction_columns,
  nodes_direction_rows,
};

enum node_wrap {
  node_wrap_nowrap = 0,
  node_wrap_wrap,
};

struct node {
  LIST_ENTRY(node) entry;
  size_t basis;
  size_t expand;
  bool fit_content;
  enum node_wrap wrap;
  size_t padding_top;
  size_t padding_bottom;
  size_t padding_left;
  size_t padding_right;
  enum nodes_direction nodes_direction;
  LIST_HEAD(,node) nodes;
  char *content;
  attr_t attr;
  NCURSES_PAIRS_T color;
};

typedef err_t (draw_text_t)(void* arg, const char* text, int len, int x, int y, int color, int attrs);
typedef err_t (draw_color_t)(void* arg, int x, int y, int width, int height, int color);

struct layout {
    struct node root;
    draw_text_t* draw_text;
    draw_color_t* draw_color;
    void* draw_arg;
};

struct rect {
    size_t col;
    size_t row;
    size_t width;
    size_t height;
};

err_t init_layout(struct layout**, draw_text_t* draw_text, draw_color_t* draw_color, void* draw_arg);
err_t free_layout(struct layout*);
err_t clear_layout(struct layout*);
err_t draw_layout(struct layout* layout, struct rect rect);
err_t get_layout_root(struct layout* layout, struct node **);

err_t append_text(struct node *, const char*);
err_t append_styled_text(struct node *, const char*, short color, attr_t attrs);
err_t append_child(struct node *, struct node** child);
err_t clear_children(struct node *);

#endif // GIT_LIVE_LAYOUT_H
