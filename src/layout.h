#ifndef GIT_LIVE_LAYOUT_H
#define GIT_LIVE_LAYOUT_H

#include <sys/queue.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <ncurses.h>

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

#define NODES_FOREACH(var, head) LIST_FOREACH(var, head, entry)
#define NODES_FOREACH_N(var, num, node, n) 					\
	for ((num) = 0, (var) = (node);				\
		(var) && (num) < (n);							\
		(var) = LIST_NEXT((var), entry), (num)++)

int print_layout(WINDOW *win, struct node *node);
struct node* init_node(void);
struct node* init_child(struct node *node);
void append_text(struct node *parent, const char*);
void append_styled_text(struct node *parent, const char*, short color, attr_t attrs);
void append_node(struct node *parent, struct node* child);
void clear_nodes(struct node *);
void free_node(struct node *);

#endif // GIT_LIVE_LAYOUT_H
