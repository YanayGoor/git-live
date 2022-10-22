#ifndef GIT_LIVE_LAYOUT_H
#define GIT_LIVE_LAYOUT_H

#include <sys/queue.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <ncurses.h>

enum unit {
  unit_ch,
  unit_percent,
};

enum overflow {
  overflow_ellipses,
  overflow_cut,
};

enum nodes_direction {
  nodes_direction_columns,
  nodes_direction_rows,
};

struct node {
  LIST_ENTRY(node) entry;
  size_t basis;
  size_t expand;
  bool fit_content;
  size_t padding_top;
  size_t padding_bottom;
  size_t padding_left;
  size_t padding_right;
  enum nodes_direction nodes_direction;
  LIST_HEAD(,node) nodes;
  const char *content;
  const char attr;
  const char color;
};

#define NODES_FOREACH(var, head) LIST_FOREACH(var, head, entry)

int print_layout(WINDOW *win, struct node *node);

#endif // GIT_LIVE_LAYOUT_H
