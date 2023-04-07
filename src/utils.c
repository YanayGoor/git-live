#include "utils.h"
#include <curses.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

err_t get_human_readable_time(int64_t t, char *buff, size_t len) {
    err_t err = NO_ERROR;

    ASSERT(buff);

    int64_t diff = time(NULL) - t;
    if (diff < 60) {
        snprintf(buff, len, "now");
    } else if (diff < 60 * 60) {
        snprintf(buff, len, "%ld minutes ago", diff / 60);
    } else if (diff < 60 * 60 * 24) {
        snprintf(buff, len, "%ld hours ago", diff / 60 / 60);
    } else if (diff < 60 * 60 * 24 * 7) {
        snprintf(buff, len, "%ld days ago", diff / 60 / 60 / 24);
    } else {
        snprintf(buff, len, "%ld weeks ago", diff / 60 / 60 / 24 / 7);
    }

cleanup:
    return err;
}

err_t safe_close_fd(int *fd) {
    err_t err = NO_ERROR;

    ASSERT(fd);

    if (*fd != FD_INVALID) {
        close(*fd);
        *fd = FD_INVALID;
    }

cleanup:
    return err;
}

static size_t get_path_part_size(const char *path) {
    size_t len = strlen(path);
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            return i;
        }
    }
    return strlen(path);
}

static bool is_path_part_eq(const char *a, const char *b) {
    size_t a_part_sz = get_path_part_size(a);
    size_t b_part_sz = get_path_part_size(b);
    if (a_part_sz != b_part_sz) {
        return false;
    }
    for (size_t i = 0; i < MIN(a_part_sz, b_part_sz); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void advance_to_next_part(const char **str) {
    *str += MIN(strlen(*str), get_path_part_size(*str) + 1);
}

err_t join_paths(const char *a, const char *b, char *out_buff, unsigned long out_len) {
    err_t err = NO_ERROR;

    ASSERT(a);
    ASSERT(b);
    ASSERT(out_buff);

    char *sep = a[strlen(a) - 1] == '/' ? "" : "/";

    snprintf(out_buff, out_len, "%s%s%s", a, sep, b);

cleanup:
    return err;
}

err_t relative_to(const char *path, const char *dir, char *out_buff, unsigned long out_len) {
    err_t err = NO_ERROR;

    ASSERT(path);
    ASSERT(dir);
    ASSERT(out_buff);

    // consume all common path
    while (strlen(dir) > 0 && is_path_part_eq(path, dir)) {
        advance_to_next_part(&path);
        advance_to_next_part(&dir);
    }

    // count the parts left in the dir and add as ../
    while (strlen(dir) > 0) {
        advance_to_next_part(&dir);

        strncpy(out_buff, "../", MIN(out_len, strlen("../")));
        out_buff += MIN(out_len, strlen("../"));
        out_len -= MIN(out_len, strlen("../"));
    }

    if (strlen(path) == 0) {
        strncpy(out_buff, ".", out_len);
    } else {
        strncpy(out_buff, path, out_len);
    }

cleanup:
    return err;
}

err_t is_relative_to(const char *path, const char *parent, bool *out) {
    err_t err = NO_ERROR;

    ASSERT(path);
    ASSERT(parent);
    ASSERT(out);

    *out = FALSE;

    while (strlen(parent) > 0) {
        if (!is_path_part_eq(path, parent)) {
            *out = FALSE;
            goto cleanup;
        }
        advance_to_next_part(&parent);
        advance_to_next_part(&path);
    }

    *out = TRUE;

cleanup:
    return err;
}