#ifndef GIT_LIVE_TIMING_H
#define GIT_LIVE_TIMING_H

#include <stdint.h>
#include "../lib/err.h"

/*
 * This module implements a timer that wakes up both by inotify for low latency and periodically for reliability.
 * periodic updates are timed to reach a certain cpu usage percent when idle and
 * (not yet implemented) inotify updates are throttled to prevent cpu usage spikes above the threshold when active.
 */

#define INVALID_WATCH_ID (-1)

typedef uint8_t percent_t;

struct timer_config {
    uint32_t min_timeout;
    percent_t idle_cpu_percent_target;
    percent_t max_cpu_percent_target;
};

struct timer;

err_t timing_init(struct timer**, struct timer_config);
err_t timing_free(struct timer*);

err_t timing_add_watch(struct timer*, const char* path, int* out);
err_t timing_modify_watch(struct timer*, int* watch_id, const char* path);
err_t timing_remove_watch(struct timer*, int watch_id);
err_t timing_wait(struct timer*);

#endif //GIT_LIVE_TIMING_H
