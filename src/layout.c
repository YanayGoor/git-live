#include <curses.h>
#include <string.h>

#include "layout.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct rect {
    size_t col;
    size_t row;
    size_t width;
    size_t height;
};

size_t get_width(struct node *node) {
    size_t sz = 0;
    struct node *curr;
    if (LIST_EMPTY(&node->nodes)) {
        const char *curr_line = node->content;
        const char *next_line;
        while ((next_line = strchr(curr_line, '\n')) != NULL) {
            sz = MAX(sz, (size_t)(next_line - curr_line));
            curr_line = next_line + 1;
        }
        sz = MAX(sz, strlen(curr_line));
    } else if (node->nodes_direction == nodes_direction_rows) {
        NODES_FOREACH (curr, &node->nodes) {
            sz = MAX(sz, get_width(curr));
        }
    } else {
        NODES_FOREACH (curr, &node->nodes) {
            sz += get_width(curr);
        }
    }
    sz += node->padding_left;
    sz += node->padding_right;
    return sz;
}

size_t get_height(struct node *node) {
    size_t sz = 0;
    struct node *curr;
    if (LIST_EMPTY(&node->nodes)) {
        for (size_t i = 0; i < strlen(node->content); i++) {
            if (node->content[i] == '\n') {
                sz++;
            }
        }
    } else if (node->nodes_direction == nodes_direction_columns) {
        NODES_FOREACH (curr, &node->nodes) {
            sz = MAX(sz, get_height(curr));
        }
    } else {
        NODES_FOREACH (curr, &node->nodes) {
            sz += get_height(curr);
        }
    }
    sz += node->padding_top;
    sz += node->padding_bottom;
    return sz;
}

int _print_layout(WINDOW *win, struct node *node, struct rect rect, NCURSES_PAIRS_T color_top, attr_t attr_top) {
  size_t maxsize = node->nodes_direction == nodes_direction_columns ? rect.width : rect.height;
  size_t weights_sum = 0;
  size_t chs_sum = 0;
  size_t contents_sum = 0;
  size_t weighted_nodes_count = 0;
  struct node *curr;

  if (node->attr) wattr_on(win, node->attr, NULL);
  if (node->color) wcolor_set(win, node->color, NULL);

  NODES_FOREACH(curr, &node->nodes) {
    weights_sum += curr->expand;
    if (curr->fit_content) {
      size_t contents_sz = node->nodes_direction == nodes_direction_columns ? get_width(curr) : get_height(curr);
      contents_sum += contents_sz;
      chs_sum += curr->basis > contents_sz ? curr->basis - contents_sz : 0;
    } else {
      chs_sum += curr->basis;
    }
    if (curr->expand) weighted_nodes_count++;
  }
  if (LIST_EMPTY(&node->nodes)) {
    int width = (int)rect.width - node->padding_left - node->padding_right;
    const char *curr_line = node->content;
    const char *next_line;
    int row = (int)node->padding_top;
        int col = (int)(rect.col + node->padding_left);

    wmove(win,  (int)rect.row + row, col);
        while ((next_line = strchr(curr_line, '\n')) != NULL) {
            wmove(win, (int)rect.row + row, col);
            waddnstr(win, curr_line, MIN((int)(next_line - curr_line), width));
            curr_line = next_line + 1;
            row++;
            if (row >= (int)rect.height - (int)node->padding_bottom)
                break;
        }
        if (row < (int)rect.height - (int)node->padding_bottom) {
            waddnstr(win, curr_line, width);
        }
        wmove(win, (int)(rect.row + rect.height - 1), 0);
        waddstr(win, "end");
    } else if (weighted_nodes_count == 0) {
        size_t curr_pos = 0;
        NODES_FOREACH (curr, &node->nodes) {
            size_t contents_sz =
                curr->fit_content
                    ? (node->nodes_direction == nodes_direction_columns ? get_width(curr) : get_height(curr))
                    : 0;
            size_t final_ch = curr->basis + contents_sz;
            if (node->nodes_direction == nodes_direction_columns) {
                _print_layout(
                    win, curr,
                    (struct rect){.col = curr_pos, .row = rect.row, .width = final_ch, .height = rect.height}, node->color, attr_top | node->attr);
            } else {
                _print_layout(win, curr,
                              (struct rect){.col = rect.col, .row = curr_pos, .width = rect.width, .height = final_ch}, node->color, attr_top | node->attr);
            }
            curr_pos += final_ch;
        }
    } else {
        size_t weight_in_ch = (maxsize - chs_sum - contents_sum) / weights_sum;
        size_t leftover = maxsize - chs_sum - contents_sum - weight_in_ch * weights_sum;
        size_t leftover_base = leftover / weighted_nodes_count;
        size_t leftover_leftover = leftover - leftover_base * weighted_nodes_count;
        size_t curr_pos = node->nodes_direction == nodes_direction_columns ? rect.col : rect.row;
        size_t curr_weighted_node_index = 0;
        NODES_FOREACH (curr, &node->nodes) {
            size_t contents_sz =
                curr->fit_content
                    ? (node->nodes_direction == nodes_direction_columns ? get_width(curr) : get_height(curr))
                    : 0;
            size_t final_ch = curr->expand * weight_in_ch + curr->basis + contents_sz + leftover_base +
                              ((curr_weighted_node_index < leftover_leftover) ? 1 : 0);
            if (node->nodes_direction == nodes_direction_columns) {
                _print_layout(
                    win, curr,
                    (struct rect){.col = curr_pos, .row = rect.row, .width = final_ch, .height = rect.height}, node->color, attr_top | node->attr);
            } else {
                _print_layout(win, curr,
                              (struct rect){.col = rect.col, .row = curr_pos, .width = rect.width, .height = final_ch}, node->color, attr_top | node->attr);
      }
      if (curr->expand > 0) curr_weighted_node_index++;
      curr_pos += final_ch;
    }
  }

  if (node->attr && !(node->attr & attr_top)) wattr_off(win, node->attr, NULL);
  if (node->color) wcolor_set(win, color_top, NULL);

    return 0;
}

int print_layout(WINDOW *win, struct node *node) {
    return _print_layout(win, node, (struct rect){.col = 0, .row = 0, .width = getmaxx(win), .height = getmaxy(win)}, 0, 0);
}