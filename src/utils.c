#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/limits.h>
#include <time.h>
#include "../lib/err.h"
#include "utils.h"

#define FD_INVALID (-1)

#define MSEC_TO_USEC (1000)
#define MSEC_IN_SEC (1000000)


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


err_t wait_for_ms(int timeout) {
    err_t err = NO_ERROR;

    ASSERT(timeout < MSEC_IN_SEC);

    usleep(timeout * MSEC_TO_USEC);

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

err_t str_find_right(char *buff, unsigned long maxlen, char sep, char** out) {
    err_t err = NO_ERROR;
    char *curr = NULL;

    ASSERT(buff);
    ASSERT(out);

    *out = NULL;

    curr = buff + strnlen(buff, maxlen);
    while (curr >= buff) {
        if (*curr == sep) {
            *out = curr;
        }
        curr--;
    }

cleanup:
    return err;
}

err_t relative_to(char *curr_pwd, const char *rel_path, char *new_pwd, char *out_buff, unsigned long out_len) {
    err_t err = NO_ERROR;
    char fullpath[PATH_MAX] = {0};
    snprintf(fullpath, sizeof(fullpath) - 1, "%s/%s", curr_pwd, rel_path);

    // TODO: this is not a generic behaviour expected of relative_to, rename the function or add an arg,
    if (!strncmp(curr_pwd, new_pwd, strlen(new_pwd))) {
        // pwds are either the smae of new_pwd is higher than curr_pwd
        strncpy(out_buff, rel_path, out_len);
        goto cleanup;
    }

    int count = 0;
    unsigned long len = strlen(new_pwd);
    char *parent_dir_end = NULL;
    while (len > 0) {
        if (!strncmp(fullpath, new_pwd, len - 1)) {
            break;
        }
        count++;
        RETHROW(str_find_right(new_pwd, len - 1, '/', &parent_dir_end));
        ASSERT(parent_dir_end);
        len = parent_dir_end - new_pwd;
    }
    for (int i = 0; i < count; i++) {
        strncpy(out_buff, "../", MIN(out_len, strlen("../")));
        out_buff += MIN(out_len, strlen("../"));
        out_len -= MIN(out_len, strlen("../"));
    }

    if (len == 0) {
        strncpy(out_buff, ".", out_len);
    } else {
        strncpy(out_buff, fullpath + len + 1, out_len);
    }

    cleanup:
    return err;
}


