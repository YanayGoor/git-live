#include <curses.h>
#include <string.h>

#include "../err.h"
#include "../list.h"
#include "layout.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define DIV_OR_ZERO(a, b) ((b) ? (a) / (b) : 0)
#define SUB_OR_ZERO(a, b) (((a) > (b)) ? (a) - (b) : 0)

#define OTHER_DIRECTION(dir) ((dir) == nodes_direction_columns ? nodes_direction_rows : nodes_direction_columns)
#define RECT_SIZE(rect) ((struct size){.width = rect.width, .height = rect.height})

#define GET_NODE_MIN_SIZE(dir, node, bounds)                                                                           \
    ((dir) == nodes_direction_columns ? get_width((node), RECT_SIZE(bounds)) : get_height((node), RECT_SIZE(bounds)))
#define GET_NODE_REQUIRED_SIZE(dir, node, bounds) (node->fit_content ? GET_NODE_MIN_SIZE(dir, node, bounds) : 0)

struct size {
    uint32_t width;
    uint32_t height;
};

static uint32_t get_width(struct node *node, struct size max_size);
static uint32_t get_height(struct node *node, struct size max_size);
static err_t print_node(struct layout *layout, struct node *node, struct rect rect, NCURSES_PAIRS_T color_top);

static uint32_t get_str_width(const char *str) {
    uint32_t sz = 0;
    uint32_t line_sz = 0;
    for (uint32_t i = 0; i < strlen(str); i++) {
        if (str[i] == '\n') {
            sz = MAX(sz, line_sz);
            line_sz = 0;
        } else {
            line_sz++;
        }
    }
    return MAX(sz, line_sz);
}

static uint32_t get_str_height(const char *str) {
    uint32_t sz = 1;
    for (uint32_t i = 0; i < strlen(str); i++) {
        // TODO: strip trailing newline if there is only whitespace after it
        if (str[i] == '\n' && i != strlen(str) - 1) {
            sz++;
        }
    }
    return sz;
}

static uint32_t get_overflow_min_height(struct node *node, struct size max_size) {
    struct node *curr;
    uint32_t sz = 0;
    if (node->nodes_direction == nodes_direction_columns) {
        // a b c |
        // d     |
        uint32_t line_width = 0;
        uint32_t line_height = 0;
        LIST_FOREACH(curr, &node->nodes, entry) {
            int32_t width = get_width(curr, max_size);
            if (line_width + width > max_size.width) {
                line_width = 0;
                sz += line_height;
            }
            line_height = MAX(line_height, get_height(curr, max_size));
            line_width += width;
        }
        sz += line_height;
    } else {
        // a c |
        // b d |

        // start with the max height of all children
        LIST_FOREACH(curr, &node->nodes, entry) { sz = MAX(sz, get_height(curr, max_size)); }

        // increment the size until all children fit
        while (sz < max_size.height) {
            sz++;
            uint32_t column_height = 0;
            uint32_t columns_width = 0;
            uint32_t column_width = 0;
            LIST_FOREACH(curr, &node->nodes, entry) {
                uint32_t height = get_height(curr, max_size);

                // we are generated that height > sz because of the first loop.
                if (column_height + height > sz) {
                    columns_width += column_width;
                    column_height = 0;
                    column_width = 0;
                }
                column_width = MAX(column_width, get_width(curr, (struct size){max_size.width, height}));
                column_height += height;
            }
            columns_width += column_width;
            if (columns_width <= max_size.width)
                break;
        }
    }
    return sz;
}

static uint32_t get_overflow_min_width(struct node *node, struct size max_size) {
    struct node *curr;
    uint32_t sz = 0;
    if (node->nodes_direction == nodes_direction_rows) {
        // a d
        // b
        // c
        // ---
        uint32_t column_height = 0;
        uint32_t column_width = 0;
        LIST_FOREACH(curr, &node->nodes, entry) {
            uint32_t height = get_height(curr, max_size);
            if (column_height + height > max_size.height) {
                column_height = 0;
                sz += column_width;
            }
            column_width = MAX(column_width, get_width(curr, max_size));
            column_height += height;
        }
        sz += column_width;
    } else {
        // a b
        // c d
        // ---

        // start with the max height of all children
        LIST_FOREACH(curr, &node->nodes, entry) { sz = MAX(sz, get_width(curr, max_size)); }

        // increment the size until all children fit
        while (sz < max_size.width) {
            sz++;
            uint32_t row_width = 0;
            uint32_t rows_height = 0;
            uint32_t row_height = 0;
            LIST_FOREACH(curr, &node->nodes, entry) {
                uint32_t width = get_width(curr, max_size);

                // we are generated that width > sz because of the first loop.
                if (row_width + width > sz) {
                    rows_height += row_height;
                    row_width = 0;
                    row_height = 0;
                }
                row_height = MAX(row_height, get_height(curr, (struct size){width, max_size.height}));
                row_width += width;
            }
            rows_height += row_height;
            if (rows_height <= max_size.height)
                break;
        }
    }
    return sz;
}

static uint32_t get_width(struct node *node, struct size max_size) {
    uint32_t sz = 0;
    struct node *curr;
    if (node->content) {
        sz = get_str_width(node->content);
    } else if (node->wrap == node_wrap_wrap) {
        sz = get_overflow_min_width(node, max_size);
    } else if (node->nodes_direction == nodes_direction_rows) {
        uint32_t height = 0;
        NODES_FOREACH (curr, &node->nodes) {
            uint32_t width = get_width(curr, max_size);
            sz = MAX(sz, width);

            height += get_height(curr, (struct size){width, max_size.height});
            if (height >= max_size.height) {
                break;
            }
        }
    } else {
        NODES_FOREACH (curr, &node->nodes) {
            sz += get_width(curr, max_size);
        }
    }
    sz += node->padding_left;
    sz += node->padding_right;
    return sz;
}

static uint32_t get_height(struct node *node, struct size max_size) {
    uint32_t sz = 0;
    struct node *curr;
    if (node->content) {
        sz = get_str_height(node->content);
    } else if (node->wrap == node_wrap_wrap) {
        sz = get_overflow_min_height(node, max_size);
    } else if (node->nodes_direction == nodes_direction_columns) {
        uint32_t width = 0;
        NODES_FOREACH (curr, &node->nodes) {
            uint32_t height = get_height(curr, max_size);
            sz = MAX(sz, height);

            width += get_width(curr, (struct size){max_size.width, height});
            if (width >= max_size.width) {
                break;
            }
        }
    } else {
        NODES_FOREACH (curr, &node->nodes) {
            sz += get_height(curr, max_size);
        }
    }
    sz += node->padding_top;
    sz += node->padding_bottom;
    return sz;
}

static err_t print_nodes(struct layout *layout, struct node *nodes, int32_t len, enum nodes_direction direction,
                         struct rect rect, NCURSES_PAIRS_T color_top) {
    err_t err = NO_ERROR;
    int32_t max_size = direction == nodes_direction_columns ? rect.width : rect.height;
    int32_t min_sizes_sum = 0;
    int32_t expand_sum = 0;
    int32_t expanded_nodes_amount = 0;
    struct node *curr;
    int32_t i;

    NODES_FOREACH_N(curr, i, nodes, len) {
        expand_sum += curr->expand;
        min_sizes_sum += MAX(GET_NODE_REQUIRED_SIZE(direction, curr, rect), curr->basis);
        if (curr->expand)
            expanded_nodes_amount++;
    }

    // the nodes we get might give us no choice but to overflow
    max_size = MAX(max_size, min_sizes_sum);

    // calculate how many rows/cols each expand takes.
    int32_t expand_to_chars = DIV_OR_ZERO(max_size - min_sizes_sum, expand_sum);

    // handle int rounding gracefully
    int32_t size_used = 0;
    NODES_FOREACH_N(curr, i, nodes, len) {
        size_used += MAX(curr->expand * expand_to_chars + curr->basis, GET_NODE_REQUIRED_SIZE(direction, curr, rect));
    }
    int32_t total_leftover = SUB_OR_ZERO(max_size, size_used);
    int32_t leftover_base = DIV_OR_ZERO(total_leftover, expanded_nodes_amount);
    int32_t leftover_leftover = total_leftover - leftover_base * expanded_nodes_amount;

    int32_t curr_pos = direction == nodes_direction_columns ? rect.col : rect.row;
    int32_t end_pos = direction == nodes_direction_columns ? rect.col + rect.width : rect.row + rect.height;
    int32_t curr_weighted_node_index = 0;
    NODES_FOREACH_N(curr, i, nodes, len) {
        struct rect inner_rect = rect;
        int32_t curr_size =
            MAX(curr->expand * expand_to_chars + curr->basis, GET_NODE_REQUIRED_SIZE(direction, curr, rect));

        if (curr->expand) {
            curr_size += leftover_base;
            if (curr_weighted_node_index < leftover_leftover) {
                curr_size++;
            }
            curr_weighted_node_index++;
        }

        if (direction == nodes_direction_columns) {
            inner_rect.col = curr_pos;
            inner_rect.width = MIN(curr_size, end_pos - curr_pos);
        } else {
            inner_rect.row = curr_pos;
            inner_rect.height = MIN(curr_size, end_pos - curr_pos);
        }
        RETHROW(print_node(layout, curr, inner_rect, color_top));
        curr_pos += curr_size;

        if (curr_pos >= end_pos)
            break;
    }

cleanup:
    return err;
}

static err_t print_text_node(struct layout *layout, const char *content, struct rect rect, int color, int attr) {
    err_t err = NO_ERROR;
    int width = (int)rect.width;
    const char *curr_line = content;
    const char *next_line;
    int row = 0;

    ASSERT(layout);
    ASSERT(content);
    ASSERT(layout->draw_text);

    while ((next_line = strchr(curr_line, '\n')) != NULL) {
        RETHROW(layout->draw_text(layout->draw_arg, curr_line, width, (int)rect.col, (int)rect.row + row, color, attr));
        curr_line = next_line + 1;
        row++;
        if (row >= (int)rect.height)
            break;
    }
    if (row < (int)rect.height) {
        RETHROW(layout->draw_text(layout->draw_arg, curr_line, width, (int)rect.col, (int)rect.row + row, color, attr));
    }

cleanup:
    return err;
}

static err_t print_wrapped_node(struct layout *layout, struct node *node, struct rect rect, NCURSES_PAIRS_T color) {
    err_t err = NO_ERROR;

    ASSERT(layout);
    ASSERT(node);

    // split to lines if warp is true
    uint32_t max_size = node->nodes_direction == nodes_direction_rows ? rect.height : rect.width;
    uint32_t max_other_size = node->nodes_direction == nodes_direction_rows ? rect.width : rect.height;
    uint32_t prev_other_size = 0;
    struct node *first = LIST_FIRST(&node->nodes);
    struct node *next = LIST_FIRST(&node->nodes);
    while (first && prev_other_size <= max_other_size) {
        uint32_t line_size = 0;
        uint32_t line_other_size = 0;
        uint32_t line_nodes_count = 0;

        while (next) {
            uint32_t next_node_size = GET_NODE_REQUIRED_SIZE(node->nodes_direction, next, rect);
            uint32_t next_node_other_size = GET_NODE_REQUIRED_SIZE(OTHER_DIRECTION(node->nodes_direction), next, rect);

            if (line_size + next_node_size > max_size) {
                break;
            }

            line_nodes_count++;
            line_size += next_node_size;
            line_other_size = MAX(line_other_size, next_node_other_size);

            next = LIST_NEXT(next, entry);
        }

        bool is_last = prev_other_size + line_other_size > max_other_size || next == NULL;

        struct rect line_rect = rect;
        if (node->nodes_direction == nodes_direction_columns) {
            line_rect.row = rect.row + prev_other_size;
            line_rect.height = !is_last ? line_other_size : (rect.height - prev_other_size);
        } else {
            line_rect.col = rect.col + prev_other_size;
            line_rect.width = !is_last ? line_other_size : (rect.width - prev_other_size);
        }
        RETHROW(print_nodes(layout, first, line_nodes_count, node->nodes_direction, line_rect, color));

        first = next;
        prev_other_size += line_other_size;
    }

cleanup:
    return err;
}

static err_t print_node(struct layout *layout, struct node *node, struct rect rect, NCURSES_PAIRS_T color_top) {
    err_t err = NO_ERROR;

    ASSERT(layout);
    ASSERT(node);

    int color = node->color ? node->color : color_top;
    struct rect inner_rect = {
        .col = rect.col + node->padding_left,
        .row = rect.row + node->padding_top,
        // TODO: not add one of the padding if node is cut-off?
        .height = rect.height - node->padding_bottom - node->padding_top,
        .width = rect.width - node->padding_right - node->padding_left,
    };

    ASSERT(layout->draw_color);
    RETHROW(layout->draw_color(layout->draw_arg, rect.col, rect.row, rect.width, rect.height, color));

    if (node->content != NULL) {
        ASSERT(LIST_EMPTY(&node->nodes));
        RETHROW(print_text_node(layout, node->content, inner_rect, color, node->attr));
    } else if (node->wrap == node_wrap_wrap) {
        RETHROW(print_wrapped_node(layout, node, inner_rect, color));
    } else {
        RETHROW(print_nodes(layout, LIST_FIRST(&node->nodes), -1, node->nodes_direction, inner_rect, color));
    }

cleanup:
    return err;
}

static err_t alloc_node(struct node **node) {
    err_t err = NO_ERROR;

    ASSERT(node);

    *node = malloc(sizeof(**node));
    ASSERT(*node);

cleanup:
    return err;
}

static err_t free_node(struct node *node) {
    err_t err = NO_ERROR;

    ASSERT(node);

    if (node->content != NULL) {
        free(node->content);
    }
    free(node);

cleanup:
    return err;
}

static err_t init_node(struct node *node) {
    err_t err = NO_ERROR;

    ASSERT(node);

    memset(node, '\0', sizeof(*node));

cleanup:
    return err;
}

/** public functions **/

err_t init_layout(struct layout **out, draw_text_t *draw_text, draw_color_t *draw_color, void *draw_arg) {
    err_t err = NO_ERROR;
    struct layout *result = NULL;

    ASSERT(out);
    ASSERT(draw_text);
    ASSERT(draw_color);

    *out = NULL;

    result = malloc(sizeof(*result));
    ASSERT(result);

    result->draw_text = draw_text;
    result->draw_color = draw_color;
    result->draw_arg = draw_arg;

    RETHROW(init_node(&result->root));

    result->root.expand = 1;

    *out = result;

cleanup:
    return err;
}

err_t clear_children(struct node *parent) {
    err_t err = NO_ERROR;

    ASSERT(parent);

    while (!LIST_EMPTY(&parent->nodes)) {
        struct node *elm = LIST_FIRST(&parent->nodes);
        LIST_REMOVE(elm, entry);
        RETHROW(clear_children(elm));
        RETHROW(free_node(elm));
    }

cleanup:
    return err;
}

err_t clear_layout(struct layout *layout) {
    err_t err = NO_ERROR;

    ASSERT(layout);

    RETHROW(clear_children(&layout->root));

cleanup:
    return err;
}

err_t free_layout(struct layout *layout) {
    err_t err = NO_ERROR;

    ASSERT(layout);

    RETHROW(clear_layout(layout));
    free(layout);

cleanup:
    return err;
}

err_t draw_layout(struct layout *layout, struct rect rect) {
    err_t err = NO_ERROR;

    ASSERT(layout);

    RETHROW(print_node(layout, &layout->root, rect, 0));

cleanup:
    return err;
}

err_t get_layout_root(struct layout *layout, struct node **root) {
    err_t err = NO_ERROR;

    ASSERT(layout);
    ASSERT(root);

    *root = &layout->root;

cleanup:
    return err;
}

err_t append_child(struct node *parent, struct node **child) {
    err_t err = NO_ERROR;

    ASSERT(parent);
    ASSERT(child);

    RETHROW(alloc_node(child));
    RETHROW(init_node(*child));
    LIST_APPEND(&parent->nodes, *child, entry);

cleanup:
    return err;
}

err_t append_styled_text(struct node *parent, const char *text, short color, attr_t attrs) {
    err_t err = NO_ERROR;
    char *buff = NULL;
    struct node *node = NULL;

    buff = malloc(strlen(text) + 1);
    ASSERT(buff);

    strcpy(buff, text);

    RETHROW(append_child(parent, &node));
    node->content = buff;
    node->fit_content = true;
    node->color = color;
    node->attr = attrs;

cleanup:
    return err;
}

err_t append_text(struct node *parent, const char *text) {
    err_t err = NO_ERROR;

    ASSERT(parent);
    ASSERT(text);

    RETHROW(append_styled_text(parent, text, 0, 0));

cleanup:
    return err;
}