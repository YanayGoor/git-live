#include "timing.h"
#include <limits.h>
#include <linux/limits.h>
#include <malloc.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

#define INVALID_WATCH_ID (-1)

#define SUBTRACT_OR_ZERO(a, b) ((a) > (b) ? (a) - (b) : 0)
#define DIVIDE_OR_ZERO(a, b) ((b) != 0 ? (a) / (b) : 0)

struct timer {
    int inotify_fd;
    uint64_t cpu_time_used;
    uint64_t total_time_used;
    uint64_t last_wakeup_time;
    struct timer_config config;
};

static err_t get_time_ms(uint64_t *ms) {
    err_t err = NO_ERROR;
    struct timespec ts;

    ASSERT(ms);
    ASSERT(!clock_gettime(CLOCK_MONOTONIC_RAW, &ts));

    *ms = ts.tv_sec * MSEC_IN_SEC;
    *ms += ts.tv_nsec / NSEC_IN_MSEC;

cleanup:
    return err;
}

static err_t clear_inotify_messages(int inotify) {
    err_t err = NO_ERROR;
    char buff[sizeof(struct inotify_event) + NAME_MAX + 1] = {0};
    ssize_t bytes_read = 0;

    ASSERT(inotify != FD_INVALID);

    do {
        bytes_read = read(inotify, buff, sizeof(buff));
    } while (bytes_read > 0);

cleanup:
    return err;
}

static err_t timing_calculate_timeout(struct timer *timer, int *out) {
    err_t err = NO_ERROR;
    uint64_t timeout = 0;

    ASSERT(timer);
    ASSERT(out);

    ASSERT(timer->config.idle_cpu_percent_target > 0);

    timeout = DIVIDE_OR_ZERO(timer->cpu_time_used * 100, timer->config.idle_cpu_percent_target);
    timeout = SUBTRACT_OR_ZERO(timeout, timer->total_time_used);
    timeout = MAX(timeout, timer->config.min_timeout);

    ASSERT(timeout < INT_MAX);
    *out = (int)timeout;

cleanup:
    return err;
}

err_t timing_init(struct timer **timing, struct timer_config config) {
    err_t err = NO_ERROR;
    uint64_t time = 0;

    ASSERT(timing);
    ASSERT(config.idle_cpu_percent_target > 0);
    ASSERT(config.max_cpu_percent_target > 0);
    ASSERT(config.idle_cpu_percent_target < 100);
    ASSERT(config.max_cpu_percent_target < 100);

    *timing = malloc(sizeof(**timing));
    ASSERT(*timing);

    RETHROW(get_time_ms(&time));

    (*timing)->config = config;
    (*timing)->cpu_time_used = 0;
    (*timing)->total_time_used = 0;
    (*timing)->inotify_fd = inotify_init1(IN_NONBLOCK);
    (*timing)->last_wakeup_time = time;

cleanup:
    return err;
}

err_t timing_free(struct timer *timer) {
    err_t err = NO_ERROR;

    ASSERT(timer);

    free(timer);

cleanup:
    return err;
}

err_t timing_add_watch(struct timer *timer, const char *path, int *out) {
    err_t err = NO_ERROR;

    ASSERT(timer);
    ASSERT(path);

    if (timer->inotify_fd == FD_INVALID) {
        goto cleanup;
    }

    if (out) {
        *out = INVALID_WATCH_ID;
    }

    int inotify_watch = inotify_add_watch(timer->inotify_fd, path, IN_CREATE | IN_MODIFY | IN_ATTRIB);
    ASSERT(inotify_watch != INVALID_WATCH_ID);

    if (out) {
        *out = inotify_watch;
    }

cleanup:
    return err;
}

err_t timing_modify_watch(struct timer *timer, int *watch_id, const char *path) {
    err_t err = NO_ERROR;

    ASSERT(timer);
    ASSERT(path);
    ASSERT(watch_id);

    if (timer->inotify_fd == FD_INVALID) {
        goto cleanup;
    }

    ASSERT(!inotify_rm_watch(timer->inotify_fd, *watch_id));
    *watch_id = inotify_add_watch(timer->inotify_fd, path, IN_MODIFY);
    ASSERT(*watch_id >= 0);

cleanup:
    return err;
}

err_t timing_remove_watch(struct timer *timer, int watch_id) {
    err_t err = NO_ERROR;

    ASSERT(timer);

    if (timer->inotify_fd == FD_INVALID) {
        goto cleanup;
    }

    ASSERT(!inotify_rm_watch(timer->inotify_fd, watch_id));

cleanup:
    return err;
}

err_t timing_wait(struct timer *timer) {
    err_t err = NO_ERROR;
    struct pollfd pollfd = {0};
    int timeout = 0;
    uint64_t tm_before_poll = 0;
    uint64_t tm_after_poll = 0;

    ASSERT(timer);

    RETHROW(get_time_ms(&tm_before_poll));
    timer->cpu_time_used += tm_before_poll - timer->last_wakeup_time;
    timer->total_time_used += tm_before_poll - timer->last_wakeup_time;

    pollfd.fd = timer->inotify_fd;
    pollfd.events = POLLIN;
    pollfd.revents = 0;

    RETHROW(timing_calculate_timeout(timer, &timeout));

    errno = 0;
    poll(&pollfd, 1, timeout);
    RETHROW(clear_inotify_messages(pollfd.fd));

    RETHROW(get_time_ms(&tm_after_poll));
    timer->total_time_used += tm_after_poll - tm_before_poll;
    timer->last_wakeup_time = tm_after_poll;

cleanup:
    return err;
}
