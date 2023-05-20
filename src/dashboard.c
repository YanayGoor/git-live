#include "dashboard.h"

#include <curses.h>
#include <git2.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>
#include "../lib/err.h"
#include "../lib/layout/layout.h"
#include "attach.h"
#include "ncurses_layout.h"
#include "timing.h"
#include "utils.h"

#define REFLOG_CO_PREFIX ("checkout:")

#define CHECKOUT_MAX_LEN (100)

#define COLOR_UNTRACKED (33)
#define COLOR_NOT_STAGED (34)
#define COLOR_STAGED (35)
#define COLOR_TITLE (36)

#define COLOR_COMMIT_HASH (37)
#define COLOR_COMMIT_TITLE (38)
#define COLOR_COMMIT_DATE (39)
#define COLOR_COMMIT_USER (40)

#define ASSERT_NCURSES(expr) ASSERT(expr != ERR)
#define ASSERT_NCURSES_PRINT(expr) ASSERT_PRINT(expr != ERR)

#define GIT_RETRY_COUNT (10)

#define ERR_BUFF_LEN (4096)

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

static err_t format_path(const char *path, const char *git_dir, const char *attached_dir, char *out_buff,
                         unsigned long out_len) {
    err_t err = NO_ERROR;
    bool is_relative = FALSE;
    char abs_path[PATH_MAX] = {0};

    ASSERT(path);
    ASSERT(git_dir);
    ASSERT(attached_dir);
    ASSERT(out_buff);
    ASSERT(out_len > 1);

    RETHROW(is_relative_to(git_dir, attached_dir, &is_relative));
    if (is_relative) {
        strncpy(out_buff, path, out_len);
        goto cleanup;
    }

    RETHROW(join_paths(git_dir, path, abs_path, sizeof(abs_path)));
    RETHROW(relative_to(abs_path, attached_dir, out_buff, out_len));

cleanup:
    return err;
}

void print_status_entry(const char *git_dir, const char *attached_dir, const git_status_entry *entry, char *buff,
                        size_t len) {
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
            format_path(entry->head_to_index->old_file.path, git_dir, attached_dir, old_filename,
                        sizeof(old_filename) - 1);
            format_path(entry->head_to_index->new_file.path, git_dir, attached_dir, new_filename,
                        sizeof(new_filename) - 1);
            snprintf(buff, len, "   %s: %s->%s", status, old_filename, new_filename);
        } else {
            char filename[PATH_MAX] = {0};
            format_path(entry->head_to_index->new_file.path, git_dir, attached_dir, filename, sizeof(filename) - 1);
            snprintf(buff, len, "   %s: %s", status, filename);
        }
    }
    if (entry->index_to_workdir) {
        if (strcmp(entry->index_to_workdir->new_file.path, entry->index_to_workdir->old_file.path) != 0) {
            char old_filename[PATH_MAX] = {0};
            char new_filename[PATH_MAX] = {0};
            format_path(entry->index_to_workdir->old_file.path, git_dir, attached_dir, old_filename,
                        sizeof(old_filename) - 1);
            format_path(entry->index_to_workdir->new_file.path, git_dir, attached_dir, new_filename,
                        sizeof(new_filename) - 1);
            snprintf(buff, len, "   %s: %s->%s", status, old_filename, new_filename);
        } else {
            char filename[PATH_MAX] = {0};
            format_path(entry->index_to_workdir->new_file.path, git_dir, attached_dir, filename, sizeof(filename) - 1);
            snprintf(buff, len, "   %s: %s", status, filename);
        }
    }
}

err_t safe_git_status_list_new(git_status_list **status_list, git_repository *repo, git_status_options *opts) {
    err_t err = NO_ERROR;
    uint32_t retries = 0;
    int inner_err = 0;

    ASSERT(status_list);
    ASSERT(repo);
    ASSERT(opts);

    while (retries++ < GIT_RETRY_COUNT) {
        inner_err = git_status_list_new(status_list, repo, opts);
        if (!inner_err)
            goto cleanup;
    }
    ABORT();

cleanup:
    return err;
}

err_t print_status(const char *pwd, const char *new_pwd, struct node *node, git_repository *repo) {
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

err_t get_root_repo_path(const char *path, uint32_t path_len, char *out, uint32_t out_len) {
    err_t err = NO_ERROR;
    bool found = FALSE;

    ASSERT(path);
    ASSERT(out);

    for (uint32_t i = 0; i < path_len; i++) {
        if (!strncmp(path + i, "/.git/modules/", MIN(path_len - i, strlen("/.git/modules/")))) {
            memcpy(out, path, MIN(out_len, i));
            out[i] = '\0';
            found = TRUE;
            break;
        }
    }

    if (!found) {
        memcpy(out, path, MIN(out_len, path_len));
    }

cleanup:
    return err;
}

void interrupt_handler() {
    keep_running = 0;
}

err_t run_dashboard() {
    err_t err = NO_ERROR;
    char err_buff[ERR_BUFF_LEN] = {0};
    char cwd[PATH_MAX] = {0};
    char new_pwd[PATH_MAX] = {0};
    char repo_root[PATH_MAX] = {0};
    char head_name[100] = {0};
    char session_id[SESSION_ID_LEN + 1] = {0};
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
    bool is_attached = false;
    struct timer *timer = NULL;
    struct attach_session* attach_session = NULL;
    int workdir_watch_id = INVALID_WATCH_ID;

    signal(SIGINT, interrupt_handler);
    init_stderr_buffering(err_buff, sizeof(err_buff));

    ASSERT(getcwd(cwd, PATH_MAX));
    ASSERT(getcwd(new_pwd, PATH_MAX));
    ASSERT(git_libgit2_init() > 0);
    ASSERT(!git_repository_open_ext(&repo, cwd, 0, "/"));

    RETHROW(get_root_repo_path(git_repository_path(repo), strlen(git_repository_path(repo)), repo_root, PATH_MAX));

    ASSERT(win = initscr());
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

    RETHROW(init_timer(&timer, (struct timer_config){
                                    .min_timeout = 200,
                                    .idle_cpu_percent_target = 10,
                                    .max_cpu_percent_target = 50,
                                }));
    RETHROW(init_attach_session(&attach_session, timer));

    RETHROW(timing_add_or_modify_watch(timer, &workdir_watch_id, git_repository_workdir(repo)));

    while (keep_running) {
        struct node *top_header_left = NULL;
        struct node *title = NULL;
        struct node *top_header_right = NULL;
        struct node *branch = NULL;
        struct node *padding = NULL;

        RETHROW(timing_wait(timer));

        RETHROW(get_attached_workdir(attach_session, new_pwd, sizeof(new_pwd) - 1, &is_attached));

        if (is_attached && strncmp(cwd, new_pwd, sizeof(cwd))) {
            strncpy(cwd, new_pwd, sizeof(cwd));

            bool is_relative = FALSE;
            RETHROW(is_relative_to(new_pwd, repo_root, &is_relative));
            if (is_relative) {
                git_repository_free(repo);
                ASSERT(!git_repository_open_ext(&repo, new_pwd, 0, "/"));
            }
        }

        RETHROW(print_status(git_repository_workdir(repo), new_pwd, top, repo));

        RETHROW(get_latest_refs(&refs, repo, getmaxy(win) - 2)); // we get more and some will be hidden
        RETHROW(print_refs(middle, &refs));
        RETHROW(clear_refs(&refs));

        RETHROW(print_latest_commits(bottom, repo, getmaxy(win) / 3));

        RETHROW(clear_children(top_header));
        RETHROW(clear_children(middle_header));
        RETHROW(clear_children(bottom_header));

        RETHROW(get_head_name(repo, head_name, sizeof(head_name)));

        RETHROW(append_child(top_header, &top_header_left));
        top_header_left->expand = 1;
        top_header_left->nodes_direction = nodes_direction_columns;

        RETHROW(get_attach_session_id(attach_session, session_id, sizeof(session_id)));

        RETHROW(append_child(top_header, &title));
        title->fit_content = true;
        title->padding_left = 1;
        title->padding_right = 1;
        RETHROW(append_text(title, "Git Live"));
        RETHROW(append_text(title, " (session "));
        RETHROW(append_text(title, session_id));
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

        RETHROW(append_text(top_header_right, git_repository_workdir(repo)));

        RETHROW(append_text(middle_header, "Latest Branches"));
        RETHROW(append_text(bottom_header, "Commits"));

        werase(win);
        RETHROW(draw_layout(layout, (struct rect){0, 0, getmaxx(win), getmaxy(win)}));
        wrefresh(win);
    }

cleanup:
    RETHROW_PRINT(free_attach_session(attach_session));
    RETHROW_PRINT(free_timer(timer));
    git_repository_free(repo);
    RETHROW_PRINT(clear_refs(&refs));
    RETHROW_PRINT(free_layout(layout));
    ASSERT_NCURSES_PRINT(delwin(win));
    ASSERT_NCURSES_PRINT(endwin());
    flush_stderr_buff();
    deinit_stderr_buff();
    return err;
}
