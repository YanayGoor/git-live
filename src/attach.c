#include "attach.h"
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "../lib/err.h"
#include "utils.h"
#include "timing.h"

#define TERMINAL_HASH_LEN (16)

struct attach_session {
    struct timer* timer;
    char session_id[SESSION_ID_LEN + 1];
    watch_id_t session_file_watch;
    watch_id_t terminal_file_watch;
    char prev_session_file_path[PATH_MAX];
    char prev_terminal_file_path[PATH_MAX];
};

static err_t gen_session_id(char *out, uint32_t out_len) {
    err_t err = NO_ERROR;

    ASSERT(out);

    srand(time(NULL));
    snprintf(out, out_len, "%02x", rand());

    cleanup:
    return err;
}

static err_t get_cache_dir(char *buff, uint32_t buff_maxlen) {
    err_t err = NO_ERROR;

    ASSERT(buff);

    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    RETHROW(join_paths(homedir, ".cache/git-live", buff, buff_maxlen));

cleanup:
    return err;
}

static err_t get_sessions_dir(char *buff, uint32_t buff_maxlen) {
    err_t err = NO_ERROR;
    char cache_dir[PATH_MAX] = {0};

    ASSERT(buff);

    RETHROW(get_cache_dir(cache_dir, sizeof(cache_dir)));

    RETHROW(join_paths(cache_dir, "sessions", buff, buff_maxlen));

cleanup:
    return err;
}

static err_t get_terminals_dir(char *buff, uint32_t buff_maxlen) {
    err_t err = NO_ERROR;
    char cache_dir[PATH_MAX] = {0};

    ASSERT(buff);

    RETHROW(get_cache_dir(cache_dir, sizeof(cache_dir)));

    RETHROW(join_paths(cache_dir, "terminals", buff, buff_maxlen));

cleanup:
    return err;
}

static err_t get_session_file_path(char *session_id, char *out, uint32_t out_len) {
    err_t err = NO_ERROR;
    char sessions_dir[PATH_MAX] = {0};

    ASSERT(session_id);
    ASSERT(out);

    RETHROW(get_sessions_dir(sessions_dir, sizeof(sessions_dir)));

    RETHROW(join_paths(sessions_dir, session_id, out, out_len));

cleanup:
    return err;
}

static err_t get_terminal_file_path(char* terminal_hash, char *out, uint32_t out_len) {
    err_t err = NO_ERROR;
    char terminals_dir[PATH_MAX] = {0};

    ASSERT(out);

    RETHROW(get_terminals_dir(terminals_dir, sizeof(terminals_dir)));

    RETHROW(join_paths(terminals_dir, terminal_hash, out, out_len));

cleanup:
    return err;
}

err_t attach_terminal_to_session(char *session_id) {
    err_t err = NO_ERROR;
    char sessions_dir[PATH_MAX] = {0};
    char session_file[PATH_MAX] = {0};
    char *terminal_hash = getenv("GIT_LIVE_TERMINAL_ID");
    bool does_dir_exist = false;
    int fd = FD_INVALID;

    ASSERT(session_id);
    ASSERT(terminal_hash);

    RETHROW(get_sessions_dir(sessions_dir, sizeof(sessions_dir)));

    RETHROW(file_exists(sessions_dir, &does_dir_exist));
    if (!does_dir_exist) {
        mkdir(sessions_dir, S_IXUSR | S_IWUSR | S_IRUSR);
    }

    RETHROW(get_session_file_path(session_id, session_file, sizeof(session_file)));

    fd = open(session_file, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    ASSERT(fd != FD_INVALID);

    ssize_t res = write(fd, terminal_hash, strlen(terminal_hash) + 1);
    ASSERT(res == (int32_t)strlen(terminal_hash) + 1);

cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    return err;
}

err_t detach_terminal_to_session(char *session_id) {
    err_t err = NO_ERROR;
    char session_file[PATH_MAX] = {0};
    int fd = FD_INVALID;

    ASSERT(session_id);

    RETHROW(get_session_file_path(session_id, session_file, sizeof(session_file)));

    ASSERT(!unlink(session_file));

    cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    return err;
}

err_t init_attach_session(struct attach_session** session, struct timer* timer) {
    err_t err = NO_ERROR;

    ASSERT(session);
    ASSERT(!*session);
    ASSERT(timer);

    *session = malloc(sizeof(**session));
    ASSERT(*session);

    (*session)->timer = timer;
    (*session)->terminal_file_watch = INVALID_WATCH_ID;
    (*session)->session_file_watch = INVALID_WATCH_ID;
    memset((*session)->prev_session_file_path, '\0', sizeof((*session)->prev_session_file_path));
    memset((*session)->prev_terminal_file_path, '\0', sizeof((*session)->prev_terminal_file_path));

    RETHROW(gen_session_id((*session)->session_id, sizeof((*session)->session_id)));

cleanup:
    if (err && *session) {
        free(*session);
    }
    return err;
}

err_t free_attach_session(struct attach_session* session) {
    err_t err = NO_ERROR;

    ASSERT(session);

    if (session->timer && session->session_file_watch) {
        RETHROW_PRINT(timing_remove_watch(session->timer, session->session_file_watch));
    }

    if (session->timer && session->terminal_file_watch) {
        RETHROW_PRINT(timing_remove_watch(session->timer, session->terminal_file_watch));
    }

    free(session);

cleanup:
    return err;
}

err_t get_attached_workdir(struct attach_session* session, char *out, uint32_t out_len, bool *is_attached) {
    err_t err = NO_ERROR;
    char session_file_path[PATH_MAX] = {0};
    char terminal_file_path[PATH_MAX] = {0};
    char terminal_hash[TERMINAL_HASH_LEN + 1] = {0};
    int fd = FD_INVALID;
    int fd2 = FD_INVALID;
    bool does_file_exist = false;

    ASSERT(session);
    ASSERT(out);
    ASSERT(is_attached);

    *is_attached = false;

    RETHROW(get_session_file_path(session->session_id, session_file_path, sizeof(session_file_path)));

    RETHROW(file_exists(session_file_path, &does_file_exist));
    if (does_file_exist && strcmp(session_file_path, session->prev_session_file_path)) {
        strcpy(session->prev_session_file_path, session_file_path);
        RETHROW(timing_add_or_modify_watch(session->timer, &session->session_file_watch, session_file_path));
    }

    fd = open(session_file_path, O_RDONLY);
    if (fd == FD_INVALID) {
        *is_attached = false;
        goto cleanup;
    }

    ssize_t res = read(fd, terminal_hash, sizeof(terminal_hash) - 1);
    if (res <= 0) {
        *is_attached = false;
        goto cleanup;
    }

    RETHROW(get_terminal_file_path(terminal_hash, terminal_file_path, sizeof(terminal_file_path)));

    RETHROW(file_exists(terminal_file_path, &does_file_exist));
    if (does_file_exist && strcmp(terminal_file_path, session->prev_terminal_file_path)) {
        strcpy(session->prev_terminal_file_path, terminal_file_path);
        RETHROW(timing_add_or_modify_watch(session->timer, &session->terminal_file_watch, terminal_file_path));
    }

    fd2 = open(terminal_file_path, O_RDONLY);
    if (fd2 == FD_INVALID) {
        *is_attached = false;
        goto cleanup;
    }

    res = read(fd2, out, out_len);
    if (res <= 0) {
        *is_attached = false;
        goto cleanup;
    }
    out[res - 1] = '\0'; // override newline with null terminator

    *is_attached = true;

cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    RETHROW_PRINT(safe_close_fd(&fd2));
    return err;
}

err_t get_attach_session_id(struct attach_session* session, char *out, uint32_t out_len) {
    err_t err = NO_ERROR;

    ASSERT(session);

    strncpy(out, session->session_id, MIN(out_len, sizeof(session->session_id)));

cleanup:
    return err;
}
