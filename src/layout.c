#include <curses.h>
#include <string.h>

#include "layout.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define DIV_OR_ZERO(a, b) ((b) ? (a) / (b) : 0)
#define SUB_OR_ZERO(a, b) (((a) > (b)) ? (a) - (b) : 0)
#define OTHER_DIRECTION(dir) ((dir) == nodes_direction_columns ? nodes_direction_rows : nodes_direction_columns)

struct rect {
    size_t col;
    size_t row;
    size_t width;
    size_t height;
};

size_t get_str_width(const char *str) {
    size_t sz = 0;
    const char *curr_line = str;
    const char *next_line;
    while ((next_line = strchr(curr_line, '\n')) != NULL) {
        sz = MAX(sz, (size_t)(next_line - curr_line));
        curr_line = next_line + 1;
    }
    sz = MAX(sz, strlen(curr_line));
    return sz;
}

size_t get_str_height(const char *str) {
    size_t sz = 1;
    for (size_t i = 0; i < strlen(str); i++) {
        // TODO: strip trailing newline if there is only whitespace after it
        if (str[i] == '\n' && i != strlen(str) - 1) {
            sz++;
        }
    }
    return sz;
}

size_t get_width(struct node *node, size_t max_height);
size_t get_height(struct node *node, size_t max_width);

size_t get_overflow_min_height(struct node *node, size_t max_width) {
    struct node *curr;
    size_t sz = 0;
    if (node->nodes_direction == nodes_direction_columns) {
        // a b c |
        // d     |
        size_t line_width = 0;
        size_t line_height = 0;
        LIST_FOREACH(curr, &node->nodes, entry) {
            size_t width = get_width(curr, 100000);
            if (line_width + width > max_width) {
                line_width = 0;
                sz += line_height;
            }
            line_height = MAX(line_height, get_height(curr, max_width));
            line_width += width;
        }
    } else {
        // a c |
        // b d |
        while (1) {
        again:
            sz++;
            size_t column_height = 0;
            size_t columns_width = 0;
            size_t column_width = 0;
            LIST_FOREACH(curr, &node->nodes, entry) {
                size_t height = get_height(curr, max_width);
                if (height > sz) {
                    goto again;
                }
                if (column_height + height > sz) {
                    column_height = 0;
                    columns_width += column_width;
                }
                column_width = MAX(column_width, get_width(curr, height));
                column_height += height;
            }
            if (columns_width <= max_width)
                break;
        }
    }
    return sz;
}

size_t get_overflow_min_width(struct node *node, size_t max_height) {
    struct node *curr;
    size_t sz = 0;
    if (node->nodes_direction == nodes_direction_rows) {
        // a d
        // b
        // c
        // ---
        size_t column_height = 0;
        size_t column_width = 0;
        LIST_FOREACH(curr, &node->nodes, entry) {
            size_t height = get_height(curr, 100000);
            if (column_height + height > max_height) {
                column_height = 0;
                sz += column_width;
            }
            column_width = MAX(column_width, get_width(curr, max_height));
            column_height += height;
        }
    } else {
        // a b
        // c d
        // ---
        while (1) {
            sz++;
            size_t row_width = 0;
            size_t rows_height = 0;
            size_t row_height = 0;
            LIST_FOREACH(curr, &node->nodes, entry) {
                size_t width = get_width(curr, max_height);
                if (row_width + width > sz) {
                    row_width = 0;
                    rows_height += row_height;
                }
                row_height = MAX(row_height, get_height(curr, width));
                row_width += width;
            }
            if (rows_height <= max_height)
                break;
        }
    }
    return sz;
}

size_t get_width(struct node *node, size_t max_height) {
    size_t sz = 0;
    struct node *curr;
    if (node->content) {
        sz = get_str_width(node->content);
    } else if (node->wrap == node_wrap_wrap) {
        return get_overflow_min_width(node, max_height);
    } else if (node->nodes_direction == nodes_direction_rows) {
        // TODO: if the sum of the height of the nodes is bigger then the node's
        //  max height (which probably needs to be given from above)
        NODES_FOREACH (curr, &node->nodes) {
            sz = MAX(sz, get_width(curr, max_height));
        }
    } else {
        NODES_FOREACH (curr, &node->nodes) {
            sz += get_width(curr, max_height);
        }
    }
    sz += node->padding_left;
    sz += node->padding_right;
    return sz;
}

size_t get_height(struct node *node, size_t max_width) {
    size_t sz = 0;
    struct node *curr;
    if (node->content) {
        sz = get_str_height(node->content);
    } else if (node->wrap == node_wrap_wrap) {
        return get_overflow_min_height(node, max_width);
    } else if (node->nodes_direction == nodes_direction_columns) {
        NODES_FOREACH (curr, &node->nodes) {
            sz = MAX(sz, get_height(curr, max_width));
        }
    } else {
        NODES_FOREACH (curr, &node->nodes) {
            sz += get_height(curr, max_width);
        }
    }
    sz += node->padding_top;
    sz += node->padding_bottom;
    return sz;
}

LIST_HEAD(nodes, node);

int _print_layout(WINDOW *win, struct node *node, struct rect rect, NCURSES_PAIRS_T color_top, attr_t attr_top);

#define NODE_MIN_SZ(dir, node, bounds)                                                                                 \
    (node->fit_content ? ((dir) == nodes_direction_columns ? get_width((node), (bounds).height)                        \
                                                           : get_height((node), (bounds).width))                       \
                       : 0)

int _print_layout_line(WINDOW *win, struct node *nodes, size_t len, enum nodes_direction direction, struct rect rect,
                       NCURSES_PAIRS_T color_top, attr_t attr_top) {
    size_t max_size = direction == nodes_direction_columns ? rect.width : rect.height;
    size_t min_sizes_sum = 0;
    size_t expand_sum = 0;
    size_t expanded_nodes_amount = 0;
    struct node *curr;
    size_t i;

    NODES_FOREACH_N(curr, i, nodes, len) {
        expand_sum += curr->expand;
        min_sizes_sum += MAX(NODE_MIN_SZ(direction, curr, rect), curr->basis);
        if (curr->expand)
            expanded_nodes_amount++;
    }

    // the nodes we get might give us no choice but to overflow
    max_size = MAX(max_size, min_sizes_sum);

    // calculate how many rows/cols each expand takes.
    size_t expand_to_chars = DIV_OR_ZERO(max_size - min_sizes_sum, expand_sum);

    // handle int rounding gracefully
    size_t size_used = 0;
    NODES_FOREACH_N(curr, i, nodes, len) {
        size_used += MAX(curr->expand * expand_to_chars + curr->basis, NODE_MIN_SZ(direction, curr, rect));
    }
    size_t total_leftover = SUB_OR_ZERO(max_size, size_used);
    size_t leftover_base = DIV_OR_ZERO(total_leftover, expanded_nodes_amount);
    size_t leftover_leftover = total_leftover - leftover_base * expanded_nodes_amount;

    size_t curr_pos = direction == nodes_direction_columns ? rect.col : rect.row;
    size_t end_pos = direction == nodes_direction_columns ? rect.col + rect.width : rect.row + rect.height;
    size_t curr_weighted_node_index = 0;
    NODES_FOREACH_N(curr, i, nodes, len) {
        struct rect inner_rect = rect;
        size_t curr_size = MAX(curr->expand * expand_to_chars + curr->basis, NODE_MIN_SZ(direction, curr, rect));

        if (curr->expand) {
            curr_size += leftover_base;
            if (curr_weighted_node_index < leftover_leftover) {
                curr_size++;
            }
            curr_weighted_node_index++;
        }

        if (direction == nodes_direction_columns) {
            inner_rect.col = curr_pos;
            inner_rect.width = curr_size;
        } else {
            inner_rect.row = curr_pos;
            inner_rect.height = curr_size;
        }

        if (curr_pos + curr_size > end_pos) break;

        _print_layout(win, curr, inner_rect, color_top, attr_top);
        curr_pos += curr_size;
    }

    return 0;
}

int _print_layout_content_str(WINDOW *win, const char *content, struct rect rect) {
    int width = (int)rect.width;
    const char *curr_line = content;
    const char *next_line;
    int row = 0;

    wmove(win, (int)rect.row + row, (int)rect.col);
    while ((next_line = strchr(curr_line, '\n')) != NULL) {
        wmove(win, (int)rect.row + row, (int)rect.col);
        waddnstr(win, curr_line, MIN((int)(next_line - curr_line), width));
        curr_line = next_line + 1;
        row++;
        if (row >= (int)rect.height)
            break;
    }
    if (row < (int)rect.height) {
        waddnstr(win, curr_line, width);
    }
    return 0;
}

void clear_list(struct nodes *nodes) {
    while (LIST_FIRST(nodes) != NULL) {
        LIST_REMOVE(LIST_FIRST(nodes), entry);
    }
}

int _print_layout(WINDOW *win, struct node *node, struct rect rect, NCURSES_PAIRS_T color_top, attr_t attr_top) {
    if (node->attr) {
        wattr_on(win, node->attr, NULL);
    }
    if (node->color) {
        wcolor_set(win, node->color, NULL);
    }

    struct rect inner_rect = {
        .col = rect.col + node->padding_left,
        .row = rect.row + node->padding_top,
        .height = rect.height - node->padding_bottom - node->padding_top,
        .width = rect.width - node->padding_right - node->padding_left,
    };


    if (node->color) {
        WINDOW *tempwin = subwin(win, rect.height, rect.width, rect.row, rect.col);
        wbkgd(tempwin, COLOR_PAIR(node->color));
        wrefresh(tempwin);
        delwin(tempwin);
    }

    if (LIST_EMPTY(&node->nodes) && node->content != NULL) {
        _print_layout_content_str(win, node->content, inner_rect);
    }
//    wmove(win, (int)(rect.row), (int)(rect.col));
//    waddstr(win, "S");
//    wmove(win, (int)(rect.row + rect.height), (int)(rect.col + rect.width) - 1);
//    waddstr(win, "E");

    // split to lines if warp is true
    size_t max_size = node->nodes_direction == nodes_direction_rows ? rect.height : rect.width;
    size_t prev_other_size = 0;
    struct node *first = LIST_FIRST(&node->nodes);
    struct node *curr = LIST_FIRST(&node->nodes);
    while (first) {
        size_t curr_size = 0;
        size_t curr_other_size = 0;
        size_t len = 0;
        struct node *next = curr;
        while (true) {
            size_t first_size = NODE_MIN_SZ(node->nodes_direction, curr, rect);
            if ((curr_size + first_size) > max_size && node->wrap == node_wrap_wrap) {
                break;
            }

            curr_size += first_size;
            curr_other_size = MAX(curr_other_size, NODE_MIN_SZ(OTHER_DIRECTION(node->nodes_direction), curr, rect));

            len++;
            curr = next;

            next = LIST_NEXT(curr, entry);
            if (next == NULL) {
                curr = next;
                break;
            }
        }

        struct rect inner_line_rect = {
            .row = inner_rect.row + (node->nodes_direction == nodes_direction_columns ? prev_other_size : 0),
            .col = inner_rect.col + (node->nodes_direction == nodes_direction_rows ? prev_other_size : 0),
            // if the line is not the last, take the minimum size of it.
            .height = curr != NULL ? node->nodes_direction == nodes_direction_columns ? curr_other_size : inner_rect.height : inner_rect.height,
            .width = curr != NULL ? node->nodes_direction == nodes_direction_rows ? curr_other_size : inner_rect.width : inner_rect.width,
        };
        _print_layout_line(win, first, len, node->nodes_direction, inner_line_rect, node->color, attr_top | node->attr);

        if (curr == NULL) {
//            first = NULL;
            break;
        }
        curr = LIST_NEXT(curr, entry);
        first = curr;
        prev_other_size += curr_other_size;
    }

    if (node->attr && !(node->attr & attr_top))
        wattr_off(win, node->attr, NULL);
    if (node->color)
        wcolor_set(win, color_top, NULL);

    return 0;
}

int print_layout(WINDOW *win, struct node *node) {
    return _print_layout(win, node, (struct rect){.col = 0, .row = 0, .width = getmaxx(win), .height = getmaxy(win)}, 0,
                         0);
}

struct node *init_node(void) {
    struct node *result = malloc(sizeof(struct node));
    memset(result, '\0', sizeof(*result));
    return result;
}

void append_node(struct node *parent, struct node *child) {
    if (LIST_EMPTY(&parent->nodes)) {
        LIST_INSERT_HEAD(&parent->nodes, child, entry);
    } else {
        struct node *last = LIST_FIRST(&parent->nodes);
        while (LIST_NEXT(last, entry)) {
            last = LIST_NEXT(last, entry);
        }
        LIST_INSERT_AFTER(last, child, entry);
    }
}

struct node* init_child(struct node *node) {
    struct node* result = init_node();
    append_node(node, result);
    return result;
}

void append_text(struct node *parent, const char *text) {
    append_styled_text(parent, text, 0, 0);
}

void append_styled_text(struct node *parent, const char* text, short color, attr_t attrs) {
    char *buff = malloc(strlen(text) + 1);
    strcpy(buff, text);

    struct node *node = init_node();
    node->content = buff;
    node->fit_content = true;
    node->color = color;
    node->attr = attrs;
    append_node(parent, node);
}

void free_node(struct node *node) {
    if (node->content) {
        free(node->content);
    }
    free(node);
}

void clear_nodes(struct node *node) {
    while (!LIST_EMPTY(&node->nodes)) {
        struct node *elm = LIST_FIRST(&node->nodes);
        LIST_REMOVE(elm, entry);
        free_node(elm);
    }
}
