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
    size_t sz = 0;
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == '\n') {
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
        //      printf("max width %ld\n", max_width);
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
    if (LIST_EMPTY(&node->nodes)) {
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
    if (LIST_EMPTY(&node->nodes)) {
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

int _print_layout_line(WINDOW *win, struct nodes *nodes, enum nodes_direction direction, struct rect rect,
                       NCURSES_PAIRS_T color_top, attr_t attr_top) {
    size_t maxsize = direction == nodes_direction_columns ? rect.width : rect.height;
    size_t weights_sum = 0;
    size_t chs_sum = 0;
    size_t contents_sum = 0;
    size_t weighted_nodes_count = 0;
    struct node *curr;

    NODES_FOREACH (curr, nodes) {
        weights_sum += curr->expand;
        if (curr->fit_content) {
            size_t contents_sz =
                direction == nodes_direction_columns ? get_width(curr, rect.height) : get_height(curr, rect.width);
            contents_sum += contents_sz;
            chs_sum += curr->basis > contents_sz ? curr->basis - contents_sz : 0;
        } else {
            chs_sum += curr->basis;
        }
        if (curr->expand)
            weighted_nodes_count++;
    }
    if (weighted_nodes_count == 0) {
        size_t curr_pos = direction == nodes_direction_columns ? rect.col : rect.row;
        NODES_FOREACH (curr, nodes) {
            size_t contents_sz = curr->fit_content
                                     ? (direction == nodes_direction_columns ? get_width(curr, rect.height)
                                                                             : get_height(curr, rect.width))
                                     : 0;
            size_t final_ch = curr->basis + contents_sz;
            if (direction == nodes_direction_columns) {
                _print_layout(win, curr,
                              (struct rect){.col = curr_pos, .row = rect.row, .width = final_ch, .height = rect.height},
                              color_top, attr_top);
            } else {
                _print_layout(win, curr,
                              (struct rect){.col = rect.col, .row = curr_pos, .width = rect.width, .height = final_ch},
                              color_top, attr_top);
            }
            curr_pos += final_ch;
        }
    } else {
        size_t weight_in_ch = (maxsize - chs_sum - contents_sum) / weights_sum;
        size_t leftover = maxsize - chs_sum - contents_sum - weight_in_ch * weights_sum;
        size_t leftover_base = leftover / weighted_nodes_count;
        size_t leftover_leftover = leftover - leftover_base * weighted_nodes_count;
        size_t curr_pos = direction == nodes_direction_columns ? rect.col : rect.row;
        size_t curr_weighted_node_index = 0;
        NODES_FOREACH (curr, nodes) {
            size_t contents_sz = curr->fit_content
                                     ? (direction == nodes_direction_columns ? get_width(curr, rect.height)
                                                                             : get_height(curr, rect.width))
                                     : 0;
            size_t final_ch =
                curr->expand * weight_in_ch + curr->basis + contents_sz +
                (curr->expand > 0 ? (leftover_base + ((curr_weighted_node_index < leftover_leftover) ? 1 : 0)) : 0);
            if (direction == nodes_direction_columns) {
                _print_layout(win, curr,
                              (struct rect){.col = curr_pos, .row = rect.row, .width = final_ch, .height = rect.height},
                              color_top, attr_top);
            } else {
                _print_layout(win, curr,
                              (struct rect){.col = rect.col, .row = curr_pos, .width = rect.width, .height = final_ch},
                              color_top, attr_top);
            }
            if (curr->expand > 0)
                curr_weighted_node_index++;
            curr_pos += final_ch;
        }
    }

    return 0;
}

int _print_layout_content_str(WINDOW *win, const char *content, struct rect rect) {
    int width = (int)rect.width;
    const char *curr_line = content;
    const char *next_line;
    int row = 0;
    int col = 0;

    wmove(win, (int)rect.row + row, rect.col + col);
    while ((next_line = strchr(curr_line, '\n')) != NULL) {
        wmove(win, (int)rect.row + row, rect.col + col);
        waddnstr(win, curr_line, MIN((int)(next_line - curr_line), width));
        curr_line = next_line + 1;
        row++;
        if (row >= (int)rect.height)
            break;
    }
    if (row < (int)rect.height) {
        waddnstr(win, curr_line, width);
    }
    //    wmove(win, (int)(rect.row + rect.height - 1), (int)rect.col);
    //    waddstr(win, "end");
    return 0;
}

int _print_layout(WINDOW *win, struct node *node, struct rect rect, NCURSES_PAIRS_T color_top, attr_t attr_top) {
    if (node->attr)
        wattr_on(win, node->attr, NULL);
    if (node->color)
        wcolor_set(win, node->color, NULL);
    struct nodes nodes = LIST_HEAD_INITIALIZER();

    struct rect inner_rect = {
        .col = rect.col + node->padding_left,
        .row = rect.row + node->padding_top,
        .height = rect.height - node->padding_bottom - node->padding_top,
        .width = rect.width - node->padding_right - node->padding_left,
    };

    if (LIST_EMPTY(&node->nodes) && node->content != NULL) {
        _print_layout_content_str(win, node->content, inner_rect);
    }

    // split to lines if warp is true
    size_t max_size = node->nodes_direction == nodes_direction_rows ? rect.height : rect.width;
    size_t prev_other_size = 0;
    while (true) {
        struct node *last = NULL;
        struct node *first = NULL;
        size_t curr_size = 0;
        size_t curr_other_size = 0;
        while (LIST_FIRST(&nodes) != NULL) {
            LIST_REMOVE(LIST_FIRST(&nodes), entry);
        }
        while (true) {
            first = LIST_FIRST(&node->nodes);

            if (first == NULL) {
                break;
            }
            size_t first_size = node->nodes_direction == nodes_direction_rows ? get_height(first, rect.width)
                                                                              : get_width(first, rect.height);
            //            printf("- %ld %ld %ld\n", curr_size, first_size, max_size);
            if ((curr_size + first_size) > max_size && node->wrap == node_wrap_wrap)
                break;

            curr_size += first_size;
            curr_other_size =
                MAX(curr_other_size, node->nodes_direction == nodes_direction_rows ? get_width(first, rect.height)
                                                                                   : get_height(first, rect.width));

            LIST_REMOVE(first, entry);
            if (last == NULL) {
                LIST_INSERT_HEAD(&nodes, first, entry);
                last = first;
            } else {
                LIST_INSERT_AFTER(last, first, entry);
                last = first;
            }
        }

        struct rect inner_line_rect = {
            .row = inner_rect.row + (node->nodes_direction == nodes_direction_rows ? 0 : prev_other_size),
            .col = inner_rect.col + (node->nodes_direction == nodes_direction_columns ? 0 : prev_other_size),
            .height = inner_rect.height - (node->nodes_direction == nodes_direction_rows ? 0 : prev_other_size),
            .width = inner_rect.width - (node->nodes_direction == nodes_direction_columns ? 0 : prev_other_size),
        };
        _print_layout_line(win, &nodes, node->nodes_direction, inner_line_rect, node->color, attr_top | node->attr);

        if (first == NULL) {
            break;
        }
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