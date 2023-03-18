#define _GNU_SOURCE
#include "../lib/err.h"
#include "utils.h"
#include "../lib/layout/layout.h"
#include "ncurses_layout.h"
#include <curses.h>
#include <fcntl.h>
#include <git2.h>
#include <linux/limits.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/queue.h>
#include <unistd.h>
#include <sys/stat.h>

#define REFLOG_CO_PREFIX "checkout:"

#define CHECKOUT_MAX_LEN 100

#define COLOR_UNTRACKED 33
#define COLOR_NOT_STAGED 34
#define COLOR_STAGED 35
#define COLOR_TITLE 36

#define COLOR_COMMIT_HASH 37
#define COLOR_COMMIT_TITLE 38
#define COLOR_COMMIT_DATE 39
#define COLOR_COMMIT_USER 40

#define INOTIFY_INVALID (-1)
#define WATCH_INVALID (-1)
#define FD_INVALID (-1)

#define ASSERT_NCURSES(expr) ASSERT(expr != ERR)
#define ASSERT_NCURSES_PRINT(expr) ASSERT_PRINT(expr != ERR)

#define GIT_RETRY_COUNT (10)

struct ref {
    char *name;
    size_t index;
    LIST_ENTRY(ref) entry;
};

LIST_HEAD(refs, ref);

static volatile bool keep_running = TRUE;

bool is_checkout_reflog(const git_reflog_entry *entry) {
    const char *message = git_reflog_entry_message(entry);
    return !strncmp(message, REFLOG_CO_PREFIX, strlen(REFLOG_CO_PREFIX));
}

const char *get_checkout_reflog_target(const git_reflog_entry *entry) {
    const char *message = git_reflog_entry_message(entry);
    return strrchr(message, ' ') + 1;
}

struct ref *create_ref(const char *target, size_t index) {
    struct ref *ref = malloc(sizeof(struct ref));
    ref->index = index;
    ref->name = malloc(strlen(target) + 1);
    strcpy(ref->name, target);
    return ref;
}

err_t free_ref(struct ref *ref) {
    err_t err = NO_ERROR;

    ASSERT(ref);

    free(ref->name);
    free(ref);

cleanup:
    return err;
}

bool refs_append_unique(struct refs *list, struct ref *ref) {
    struct ref *curr = LIST_FIRST(list);
    struct ref *last = curr;

    LIST_FOREACH(curr, list, entry) {
        if (strcmp(curr->name, ref->name) == 0)
            return false;
        last = curr;
    }

    if (last == NULL) {
        LIST_INSERT_HEAD(list, ref, entry);
    } else {
        LIST_INSERT_AFTER(last, ref, entry);
    }
    return true;
}

err_t get_latest_refs(struct refs *out, git_repository *repo, size_t max) {
    err_t err = NO_ERROR;
    git_reflog *reflog;
    size_t collected = 0;

    ASSERT(out);
    ASSERT(repo);

    git_reflog_read(&reflog, repo, "HEAD");
    size_t count = git_reflog_entrycount(reflog);

    for (size_t index = 0; index < count; index++) {
        const git_reflog_entry *entry = git_reflog_entry_byindex(reflog, index);

        if (!is_checkout_reflog(entry))
            continue;

        const char *target = get_checkout_reflog_target(entry);
        struct ref *ref = create_ref(target, index);

        if (!refs_append_unique(out, ref)) {
            RETHROW(free_ref(ref));
            continue;
        }

        collected++;
        if (collected == max)
            break;
    }

cleanup:
    git_reflog_free(reflog);
    return err;
}

err_t clear_refs(struct refs *refs) {
    err_t err = NO_ERROR;

    ASSERT(refs);

    struct ref *first;
    while ((first = LIST_FIRST(refs)) != NULL) {
        LIST_REMOVE(first, entry);
        RETHROW(free_ref(first));
    }
cleanup:
    return err;
}

void get_co_command(char *out, size_t maxlen, size_t index) {
    if (index == 1) {
        strcpy(out, "git checkout -");
    } else {
        snprintf(out, maxlen, "git checkout @{-%ld}", index);
    }
}

err_t print_refs(struct node *node, struct refs *refs) {
    err_t err = NO_ERROR;
    struct ref *curr;
    char buff[CHECKOUT_MAX_LEN];
    struct node *names = NULL;
    struct node *co_commands = NULL;

    clear_children(node);

    append_child(node, &names);
    names->nodes_direction = nodes_direction_rows;
    names->fit_content = true;
    //    names->basis = 15;

    append_child(node, &co_commands);
    co_commands->nodes_direction = nodes_direction_rows;
    co_commands->padding_left = 4;
    co_commands->expand = 1;

    ASSERT(node);
    ASSERT(refs);

    LIST_FOREACH(curr, refs, entry) {
        if (curr->index == 0)
            continue;
        append_text(names, curr->name);
        if (curr->index) {
            get_co_command(buff, CHECKOUT_MAX_LEN, curr->index);
            append_styled_text(co_commands, buff, 0, WA_DIM);
        }
    }
cleanup:
    return err;
}

err_t print_latest_commits(struct node *node, git_repository *repo, int max) {
    err_t err = NO_ERROR;
    git_revwalk *walker;
    git_commit *commit;
    int line = 1;
    char hash[6];
    char time[16];
    char msg[512];
    git_oid next;
    struct node *hash_col = NULL;
    struct node *msg_col = NULL;
    struct node *user_col = NULL;
    struct node *time_col = NULL;

    ASSERT(node);
    ASSERT(repo);

    git_revwalk_new(&walker, repo);
    git_revwalk_push_ref(walker, "HEAD");

    clear_children(node);

    append_child(node, &hash_col);
    hash_col->fit_content = true;
    hash_col->nodes_direction = nodes_direction_rows;

    append_child(node, &msg_col);
    msg_col->expand = 1;
    msg_col->nodes_direction = nodes_direction_rows;
    msg_col->padding_left = 1;

    append_child(node, &user_col);
    user_col->fit_content = true;
    user_col->nodes_direction = nodes_direction_rows;
    user_col->padding_left = 1;

    append_child(node, &time_col);
    time_col->fit_content = true;
    time_col->nodes_direction = nodes_direction_rows;
    time_col->padding_left = 1;
    time_col->padding_right = 1;

    while (git_revwalk_next(&next, walker) != GIT_ITEROVER) {
        if (git_commit_lookup(&commit, repo, &next))
            continue;

        sprintf(hash, "%02x%02x", next.id[0], next.id[1]);

        append_styled_text(hash_col, hash, COLOR_COMMIT_HASH, WA_DIM);

        strncpy(msg, git_commit_message(commit), sizeof(msg) - 1);
        strtok(msg, "\n");
        append_styled_text(msg_col, msg, COLOR_COMMIT_TITLE, 0);

        append_styled_text(user_col, git_commit_committer(commit)->name, COLOR_COMMIT_USER, WA_DIM);

        RETHROW(get_human_readable_time(git_commit_time(commit), time, 15));
        append_styled_text(time_col, time, COLOR_COMMIT_DATE, 0);

        git_commit_free(commit);
        if (++line == max)
            break;
    }

    git_revwalk_free(walker);

cleanup:
    return err;
}

err_t get_head_name(git_repository *repo, char *buff, size_t len) {
    err_t err = NO_ERROR;
    git_reference *head = NULL;

    err = git_repository_head(&head, repo);
    ASSERT(err != GIT_EUNBORNBRANCH && err != GIT_ENOTFOUND);

    strncpy(buff, git_reference_shorthand(head), len);
    git_reference_free(head);

cleanup:
    return err;
}

void print_status_entry(char *pwd, char *new_pwd, const git_status_entry *entry, char *buff, size_t len) {
    const char *status = "";
    if (entry->status & (GIT_STATUS_INDEX_NEW | GIT_STATUS_WT_NEW)) {
        status = "new";
    } else if (entry->status & (GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED)) {
        status = "renamed";
    } else if (entry->status & (GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_MODIFIED)) {
        status = "modified";
    } else if (entry->status & (GIT_STATUS_INDEX_DELETED | GIT_STATUS_WT_DELETED)) {
        status = "deleted";
    }
    if (entry->head_to_index) {
        if (strcmp(entry->head_to_index->new_file.path, entry->head_to_index->old_file.path) != 0) {
            char old_filename[PATH_MAX] = {0};
            char new_filename[PATH_MAX] = {0};
            relative_to(pwd, entry->head_to_index->old_file.path, new_pwd, old_filename, sizeof(old_filename) - 1);
            relative_to(pwd, entry->head_to_index->new_file.path, new_pwd, new_filename, sizeof(new_filename) - 1);
            snprintf(buff, len, "   %s: %s->%s", status, old_filename, new_filename);
        } else {
            char filename[PATH_MAX] = {0};
            relative_to(pwd, entry->head_to_index->new_file.path, new_pwd, filename, sizeof(filename) - 1);
            snprintf(buff, len, "   %s: %s", status, filename);
        }
    }
    if (entry->index_to_workdir) {
        if (strcmp(entry->index_to_workdir->new_file.path, entry->index_to_workdir->old_file.path) != 0) {
            char old_filename[PATH_MAX] = {0};
            char new_filename[PATH_MAX] = {0};
            relative_to(pwd, entry->index_to_workdir->old_file.path, new_pwd, old_filename, sizeof(old_filename) - 1);
            relative_to(pwd, entry->index_to_workdir->new_file.path, new_pwd, new_filename, sizeof(new_filename) - 1);
            snprintf(buff, len, "   %s: %s->%s", status, old_filename, new_filename);
        } else {
            char filename[PATH_MAX] = {0};
            relative_to(pwd, entry->index_to_workdir->new_file.path, new_pwd, filename, sizeof(filename) - 1);
            snprintf(buff, len, "   %s: %s", status, filename);
        }
    }
}

err_t safe_git_status_list_new(git_status_list** status_list, git_repository* repo, git_status_options* opts) {
    err_t err = NO_ERROR;
    uint32_t retries = 0;
    int inner_err = 0;

    ASSERT(status_list);
    ASSERT(repo);
    ASSERT(opts);

    while (retries++ < GIT_RETRY_COUNT) {
        inner_err = git_status_list_new(status_list, repo, opts);
        if (!inner_err) goto cleanup;
    }
    ABORT();

cleanup:
    return err;
}

err_t print_status(char *pwd, char *new_pwd, struct node *node, git_repository *repo) {
    err_t err = NO_ERROR;
    char buff[PATH_MAX] = {0};
    git_status_list *status_list;
    git_status_options opts = {.version = GIT_STATUS_OPTIONS_VERSION,
                               .flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED,
                               .show = GIT_STATUS_SHOW_INDEX_ONLY};

    ASSERT(node);
    ASSERT(repo);

    RETHROW(safe_git_status_list_new(&status_list, repo, &opts));

    clear_children(node);

    append_text(node, " staged:");
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        print_status_entry(pwd, new_pwd, entry, buff, sizeof(buff));
        append_styled_text(node, buff, COLOR_STAGED, 0);
    }
    git_status_list_free(status_list);
    opts =
        (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION, .flags = 0, .show = GIT_STATUS_SHOW_WORKDIR_ONLY};

    RETHROW(safe_git_status_list_new(&status_list, repo, &opts));

    append_text(node, " changed:");
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        print_status_entry(pwd, new_pwd, entry, buff, sizeof(buff));
        append_styled_text(node, buff, COLOR_NOT_STAGED, 0);
    }
    git_status_list_free(status_list);

    opts = (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION,
                                .flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED,
                                .show = GIT_STATUS_SHOW_WORKDIR_ONLY};

    RETHROW(safe_git_status_list_new(&status_list, repo, &opts));
    append_text(node, " untracked:");
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (entry->index_to_workdir && entry->index_to_workdir->status != GIT_DELTA_UNTRACKED) {
            continue;
        }
        print_status_entry(pwd, new_pwd, entry, buff, sizeof(buff));
        append_styled_text(node, buff, COLOR_UNTRACKED, 0);
    }
    git_status_list_free(status_list);

cleanup:
    return err;
}

err_t try_initialize_inotify(char *path, int *out) {
    err_t err = NO_ERROR;

    ASSERT(path);

    *out = INOTIFY_INVALID;

    int inotify = inotify_init();
    if (inotify == INOTIFY_INVALID)
        goto cleanup;

    int watch = inotify_add_watch(inotify, path, IN_MODIFY);
    if (watch == WATCH_INVALID)
        goto cleanup;

    *out = inotify;

cleanup:
    return err;
}

err_t clear_inotify_message(int inotify) {
    err_t err = NO_ERROR;
    char buff[sizeof(struct inotify_event) + NAME_MAX + 1] = {0};

    ASSERT(inotify != INOTIFY_INVALID);
    read(inotify, buff, sizeof(buff));

cleanup:
    return err;
}

err_t wait_for_inotify_message(int inotify, int timeout) {
    err_t err = NO_ERROR;
    struct pollfd pollfds[1] = {{.fd = inotify, .events = POLLIN, .revents = 0}};

    ASSERT(inotify != INOTIFY_INVALID);

    int changed = poll(pollfds, 1, timeout);
    if (changed == 1) {
        RETHROW(clear_inotify_message(inotify));
    }

cleanup:
    return err;
}

err_t get_cache_dir(char *buff, uint32_t maxlen) {
    err_t err = NO_ERROR;

    ASSERT(buff);

    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    snprintf(buff, maxlen, "%s/%s", homedir, ".cache/git-live");

cleanup:
    return err;
}

err_t get_attached_terminal_hash(char *session_id, char *out, uint32_t out_len, bool* successful) {
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

err_t get_terminal_workdir_path(char *terminal_hash, char *out, uint32_t out_len) {
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

err_t get_terminal_workdir(char *terminal_workdir_path, char *out, uint32_t out_len) {
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
                                        uint32_t inotify_watch_path_len, bool* successful) {
    err_t err = NO_ERROR;
    char terminal_hash[17] = {0};
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

err_t gen_session_id(char *out, uint32_t out_len) {
    err_t err = NO_ERROR;

    ASSERT(out);

    srand(time(NULL));
    snprintf(out, out_len, "%02x", rand());

cleanup:
    return err;
}

void interrupt_handler() { keep_running = 0; }

int _main() {
    err_t err = NO_ERROR;
    char cwd[PATH_MAX] = {0};
    char new_pwd[PATH_MAX] = {0};
    char prev_inotify_new_pwd_path[PATH_MAX] = {0};
    char inotify_new_pwd_path[PATH_MAX] = {0};
    char head_name[100] = {0};
    char hex[5] = {0};
    git_buf buf = {NULL, 0, 0};
    struct refs refs = LIST_HEAD_INITIALIZER();
    struct layout *layout = NULL;
    struct node *top_header = NULL;
    struct node *top = NULL;
    struct node *middle_header = NULL;
    struct node *middle = NULL;
    struct node *bottom_header = NULL;
    struct node *bottom = NULL;
    WINDOW *win = NULL;
    git_repository *repo = NULL;
    int inotify = INOTIFY_INVALID;
    int inotify_pwd_path = INOTIFY_INVALID;
    bool is_attached = false;

    signal(SIGINT, interrupt_handler);

    ASSERT(win = initscr());
    ASSERT(getcwd(cwd, PATH_MAX));
    ASSERT(getcwd(new_pwd, PATH_MAX));
    ASSERT(git_libgit2_init() > 0);
    ASSERT(!git_repository_discover(&buf, cwd, 0, NULL));
    ASSERT(!git_repository_open(&repo, cwd));

    RETHROW(gen_session_id(hex, sizeof(hex)));

    ASSERT_NCURSES(curs_set(0));
    ASSERT_NCURSES(start_color());
    ASSERT_NCURSES(use_default_colors());

    ASSERT_NCURSES(init_pair(COLOR_STAGED, COLOR_GREEN, -1));
    ASSERT_NCURSES(init_pair(COLOR_NOT_STAGED, COLOR_RED, -1));
    ASSERT_NCURSES(init_pair(COLOR_UNTRACKED, COLOR_RED, -1));
    ASSERT_NCURSES(init_pair(COLOR_TITLE, COLOR_BLACK, COLOR_WHITE));
    ASSERT_NCURSES(init_pair(COLOR_COMMIT_HASH, COLOR_BLUE, -1));
    ASSERT_NCURSES(init_pair(COLOR_COMMIT_TITLE, -1, -1));
    ASSERT_NCURSES(init_pair(COLOR_COMMIT_DATE, -1, -1));
    ASSERT_NCURSES(init_pair(COLOR_COMMIT_USER, -1, -1));

    RETHROW(init_ncurses_layout(&layout, win));
    layout->root.expand = 1;
    layout->root.fit_content = true;
    layout->root.nodes_direction = nodes_direction_rows;

    RETHROW(append_child(&layout->root, &top_header));
    top_header->basis = 1;
    top_header->padding_left = 1;
    top_header->nodes_direction = nodes_direction_columns;
    top_header->color = COLOR_TITLE;

    RETHROW(append_child(&layout->root, &top));
    top->expand = 1;
    top->nodes_direction = nodes_direction_rows;
    top->wrap = node_wrap_wrap;
    // top->padding_left = 1;
    top->fit_content = true;

    RETHROW(append_child(&layout->root, &middle_header));
    middle_header->basis = 1;
    middle_header->padding_left = 1;
    middle_header->color = COLOR_TITLE;

    RETHROW(append_child(&layout->root, &middle));
    middle->expand = 1;
    middle->nodes_direction = nodes_direction_columns;
    middle->padding_left = 1;

    RETHROW(append_child(&layout->root, &bottom_header));
    bottom_header->basis = 1;
    bottom_header->padding_left = 1;
    bottom_header->color = COLOR_TITLE;

    RETHROW(append_child(&layout->root, &bottom));
    bottom->expand = 1;
    bottom->nodes_direction = nodes_direction_columns;
    bottom->padding_left = 1;

    RETHROW(try_initialize_inotify(buf.ptr, &inotify));

    while (keep_running) {
        if (inotify == INOTIFY_INVALID) {
            RETHROW(wait_for_ms(50));
        } else {
            RETHROW(wait_for_inotify_message(inotify, 100));
        }

        memcpy(new_pwd, cwd, sizeof(new_pwd) - 1);
        try_get_attached_terminal_workdir(hex, new_pwd, sizeof(new_pwd) - 1, inotify_new_pwd_path,
                                                  sizeof(inotify_new_pwd_path) - 1, &is_attached);

        if (inotify != -1 &&
            memcmp(prev_inotify_new_pwd_path, inotify_new_pwd_path, sizeof(prev_inotify_new_pwd_path))) {
            if (inotify_pwd_path != INOTIFY_INVALID) {
                inotify_rm_watch(inotify, inotify_pwd_path);
            }
            inotify_pwd_path = inotify_add_watch(inotify, inotify_new_pwd_path, IN_MODIFY);
        }
        memcpy(prev_inotify_new_pwd_path, inotify_new_pwd_path, sizeof(prev_inotify_new_pwd_path));

        struct node *top_header_left = NULL;
        struct node *title = NULL;
        struct node *top_header_right = NULL;
        struct node *branch = NULL;
        struct node *padding = NULL;

        RETHROW(print_status(cwd, new_pwd, top, repo));

        RETHROW(get_latest_refs(&refs, repo, getmaxy(win) - 2)); // we get more and some will be hidden
        RETHROW(print_refs(middle, &refs));
        RETHROW(clear_refs(&refs));

        RETHROW(print_latest_commits(bottom, repo, getmaxy(win) / 3));

        RETHROW(clear_children(top_header));
        RETHROW(clear_children(middle_header));
        RETHROW(clear_children(bottom_header));

        RETHROW(get_head_name(repo, head_name, 100));

        RETHROW(append_child(top_header, &top_header_left));
        top_header_left->expand = 1;
        top_header_left->nodes_direction = nodes_direction_columns;

        RETHROW(append_child(top_header, &title));
        title->fit_content = true;
        title->padding_left = 1;
        title->padding_right = 1;
        RETHROW(append_text(title, "Git Live"));
        RETHROW(append_text(title, " (session "));
        RETHROW(append_text(title, hex));
        if (is_attached) {
            RETHROW(append_text(title, " <attached>"));
        }
        RETHROW(append_text(title, ")"));

        RETHROW(append_child(top_header, &top_header_right));
        top_header_right->expand = 1;
        top_header_right->nodes_direction = nodes_direction_columns;

        RETHROW(append_text(top_header_left, "Status"));

        RETHROW(append_child(top_header_left, &branch));
        branch->expand = 1;
        branch->padding_left = 1;
        branch->nodes_direction = nodes_direction_columns;
        RETHROW(append_text(branch, "("));
        RETHROW(append_text(branch, head_name));
        RETHROW(append_text(branch, ")"));

        RETHROW(append_child(top_header_right, &padding));
        padding->expand = 1;

        RETHROW(append_text(top_header_right, cwd));

        RETHROW(append_text(middle_header, "Latest Branches"));
        RETHROW(append_text(bottom_header, "Commits"));

        werase(win);
        RETHROW(draw_layout(layout, (struct rect){0, 0, getmaxx(win), getmaxy(win)}));
        wrefresh(win);
    }

cleanup:
    git_buf_free(&buf);
    git_repository_free(repo);
    RETHROW_PRINT(clear_refs(&refs));
    RETHROW_PRINT(free_layout(layout));
    ASSERT_NCURSES_PRINT(delwin(win));
    ASSERT_NCURSES_PRINT(endwin());
    return err;
}

err_t print_usage() {
    fprintf(stderr, "Usage: git live <command>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  <none>       Run a new git-live dashboard.\n");
    fprintf(stderr, "  attach       Attach a running dashboard to the current terminal so that the paths are relative "
                    "to its cwd.\n");
    fprintf(stderr, "  detach       Detach a running dashboard from the terminal it is attached to.\n");
    return 0;
}

err_t attach_terminal_session(char *session_id) {
    err_t err = NO_ERROR;
    char *terminal_hash = getenv("GIT_LIVE_TERMINAL_ID");

    ASSERT(terminal_hash);
    ASSERT(session_id);

    RETHROW(set_attached_terminal_hash(session_id, terminal_hash));
cleanup:
    return err;
}

err_t detach_terminal_session(char *session_id) {
    err_t err = NO_ERROR;

    ASSERT(session_id);

    RETHROW(clear_attached_terminal_hash(session_id));
cleanup:
    return err;
}

err_t print_attach_usage() {
    fprintf(stderr, "Usage: git live attach <session_id>\n");
    return 0;
}

err_t print_detach_usage() {
    fprintf(stderr, "Usage: git live detach <session_id>\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        return _main();
    } else if (!strcmp(argv[1], "attach")) {
        if (argc == 3 && strcmp(argv[2], "--help")) {
            return attach_terminal_session(argv[2]);
        } else if (argc > 3) {
            fprintf(stderr, "Too many arguments.\n");
            print_attach_usage();
        } else {
            print_attach_usage();
        }
    } else if (!strcmp(argv[1], "detach")) {
        if (argc == 3 && strcmp(argv[2], "--help")) {
            return detach_terminal_session(argv[2]);
        } else if (argc > 3) {
            fprintf(stderr, "Too many arguments.\n");
            print_detach_usage();
        } else {
            print_detach_usage();
        }
    } else if (!strcmp(argv[1], "--help")) {
        print_usage();
        return 1;
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }
    return 0;
}