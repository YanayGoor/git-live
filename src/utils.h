#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "../lib/err.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

err_t get_human_readable_time(int64_t t, char *buff, size_t len);
err_t wait_for_ms(int timeout);
err_t safe_close_fd(int *fd);
err_t str_find_right(char *buff, unsigned long maxlen, char sep, char** out);
err_t relative_to(char *curr_pwd, const char *rel_path, char *new_pwd, char *out_buff, unsigned long out_len);

#endif