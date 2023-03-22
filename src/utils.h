#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include "../lib/err.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

err_t get_human_readable_time(int64_t t, char *buff, size_t len);
err_t wait_for_ms(int timeout);
err_t safe_close_fd(int *fd);
err_t join_paths(const char *a, const char *b, char *out_buff, unsigned long out_len);
err_t relative_to(const char* path, const char *dir, char *out_buff, unsigned long out_len);
err_t is_relative_to(const char *path, const char *parent, bool* out);

#endif