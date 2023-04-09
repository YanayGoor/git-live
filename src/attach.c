#include "attach.h"
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../lib/err.h"
#include "utils.h"

#define TERMINAL_HASH_LEN (16)

static err_t get_cache_dir(char *buff, uint32_t maxlen) {
    err_t err = NO_ERROR;

    ASSERT(buff);

    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    snprintf(buff, maxlen, "%s/%s", homedir, ".cache/git-live");

cleanup:
    return err;
}

err_t get_attached_terminal_hash(char *session_id, char *out, uint32_t out_len, bool *successful) {
    err_t err = NO_ERROR;
    char cache_dir[PATH_MAX] = {0};
    char attached_file[PATH_MAX] = {0};
    int fd = FD_INVALID;

    ASSERT(session_id);
    ASSERT(out);
    ASSERT(successful);

    RETHROW(get_cache_dir(cache_dir, sizeof(cache_dir)));

    *successful = false;

    uint32_t written = snprintf(attached_file, sizeof(attached_file), "%s/%s/%s", cache_dir, "attached", session_id);
    ASSERT(written < sizeof(attached_file));

    fd = open(attached_file, O_RDONLY);
    if (fd == FD_INVALID) {
        *successful = false;
        goto cleanup;
    }

    ssize_t res = read(fd, out, out_len);
    if (res <= 0) {
        *successful = false;
        goto cleanup;
    }

    *successful = true;

cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    return err;
}

err_t set_attached_terminal_hash(char *session_id, char *terminal_hash) {
    err_t err = NO_ERROR;
    char cache_dir[PATH_MAX] = {0};
    char attached_dir[PATH_MAX] = {0};
    char attached_file[PATH_MAX] = {0};
    struct stat st = {0};
    int fd = FD_INVALID;

    ASSERT(session_id);

    RETHROW(get_cache_dir(cache_dir, sizeof(cache_dir)));

    uint32_t written = snprintf(attached_dir, sizeof(attached_dir), "%s/%s", cache_dir, "attached");
    ASSERT(written < sizeof(attached_dir));

    if (stat(attached_dir, &st) == -1) {
        mkdir(attached_dir, S_IXUSR | S_IWUSR | S_IRUSR);
    }

    written = snprintf(attached_file, sizeof(attached_file), "%s/%s", attached_dir, session_id);
    ASSERT(written < sizeof(attached_file));

    fd = open(attached_file, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    ASSERT(fd != FD_INVALID);

    ssize_t res = write(fd, terminal_hash, strlen(terminal_hash) + 1);
    ASSERT(res == (int32_t)strlen(terminal_hash) + 1);

cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    return err;
}

err_t clear_attached_terminal_hash(char *session_id) {
    err_t err = NO_ERROR;
    char cache_dir[PATH_MAX] = {0};
    char attached_file[PATH_MAX] = {0};
    int fd = FD_INVALID;

    ASSERT(session_id);

    RETHROW(get_cache_dir(cache_dir, sizeof(cache_dir)));

    uint32_t written = snprintf(attached_file, sizeof(attached_file), "%s/%s/%s", cache_dir, "attached", session_id);
    ASSERT(written < sizeof(attached_file));

    ASSERT(!unlink(attached_file));

cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    return err;
}

static err_t get_terminal_workdir_path(char *terminal_hash, char *out, uint32_t out_len) {
    err_t err = NO_ERROR;
    char cache_dir[PATH_MAX] = {0};

    ASSERT(terminal_hash);
    ASSERT(out);

    RETHROW(get_cache_dir(cache_dir, sizeof(cache_dir)));

    uint32_t written = snprintf(out, out_len, "%s/%s/%s", cache_dir, "workdirs", terminal_hash);
    ASSERT(written < out_len);

cleanup:
    return err;
}

static err_t get_terminal_workdir(char *terminal_workdir_path, char *out, uint32_t out_len) {
    err_t err = NO_ERROR;

    int fd = open(terminal_workdir_path, O_RDONLY);
    if (fd == -1)
        goto cleanup;

    ssize_t res = read(fd, out, out_len);
    out[res - 1] = '\0'; // override newline with null terminator
    if (res <= 0)
        goto cleanup;

cleanup:
    RETHROW_PRINT(safe_close_fd(&fd));
    return err;
}

err_t try_get_attached_terminal_workdir(char *session_id, char *out, uint32_t out_len, char *inotify_watch_path,
                                        uint32_t inotify_watch_path_len, bool *successful) {
    err_t err = NO_ERROR;
    char terminal_hash[TERMINAL_HASH_LEN + 1] = {0};
    char terminal_workdir_path[PATH_MAX] = {0};

    RETHROW(get_attached_terminal_hash(session_id, terminal_hash, sizeof(terminal_hash) - 1, successful));
    if (!*successful) {
        goto cleanup;
    }

    RETHROW(get_terminal_workdir_path(terminal_hash, terminal_workdir_path, sizeof(terminal_workdir_path) - 1));
    RETHROW(get_terminal_workdir(terminal_workdir_path, out, out_len));

    strncpy(inotify_watch_path, terminal_workdir_path, inotify_watch_path_len);

cleanup:
    return err;
}

err_t attach_terminal_to_session(char *session_id) {
    err_t err = NO_ERROR;
    char *terminal_hash = getenv("GIT_LIVE_TERMINAL_ID");

    ASSERT(terminal_hash);
    ASSERT(session_id);

    RETHROW(set_attached_terminal_hash(session_id, terminal_hash));
cleanup:
    return err;
}

err_t detach_terminal_to_session(char *session_id) {
    err_t err = NO_ERROR;

    ASSERT(session_id);

    RETHROW(clear_attached_terminal_hash(session_id));
cleanup:
    return err;
}
