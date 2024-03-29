#ifndef GIT_LIVE_LAYOUT_H
#define GIT_LIVE_LAYOUT_H

#include <sys/queue.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <ncurses.h>
#include <stdint.h>
#include "../err.h"

enum nodes_direction {
  nodes_direction_columns = 0,
  nodes_direction_rows,
};

enum node_wrap {
  node_wrap_nowrap = 0,
  node_wrap_wrap,
};

struct node {
  LIST_ENTRY(node) entry;
  uint32_t basis;
  uint32_t expand;
  bool fit_content;
  enum node_wrap wrap;
  uint32_t padding_top;
  uint32_t padding_bottom;
  uint32_t padding_left;
  uint32_t padding_right;
  enum nodes_direction nodes_direction;
  LIST_HEAD(,node) nodes;
  char *content;
  attr_t attr;
  short color;
};

typedef err_t (draw_text_t)(void* arg, const char* text, uint32_t len, uint32_t col, uint32_t row, int color, int attrs);
typedef err_t (draw_color_t)(void* arg, uint32_t col, uint32_t row, uint32_t width, uint32_t height, int color);

struct layout {
    struct node root;
    draw_text_t* draw_text;
    draw_color_t* draw_color;
    void* draw_arg;
};

struct rect {
    uint32_t col;
    uint32_t row;
    uint32_t width;
    uint32_t height;
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
