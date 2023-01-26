#include <curses.h>
#include <git2.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>
#include "../lib/err.h"

#define REFLOG_CO_PREFIX "checkout:"

#define CHECKOUT_MAX_LEN 100

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define COLOR_UNTRACKED 33
#define COLOR_NOT_STAGED 34
#define COLOR_STAGED 35
#define COLOR_TITLE 36

#define COLOR_COMMIT_HASH 37
#define COLOR_COMMIT_TITLE 38
#define COLOR_COMMIT_DATE 39
#define COLOR_COMMIT_USER 40

#define ASSERT_LIBGIT2(expr) ASSERT(expr != ERR)

struct ref {
    char *name;
    size_t index;
    LIST_ENTRY(ref) entry;
};

LIST_HEAD(refs, ref);

void get_human_readable_time(git_time_t t, char *buff, size_t len) {
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
}

bool is_checkout_reflog(const git_reflog_entry *entry) {
    const char *message = git_reflog_entry_message(entry);
    return !strncmp(message, REFLOG_CO_PREFIX, strlen(REFLOG_CO_PREFIX));
}

const char *get_checkout_reflog_target(const git_reflog_entry *entry) {
    const char *message = git_reflog_entry_message(entry);
    return strrchr(message, ' ') + 1;
}

struct ref *create_ref(const char *target, int index) {
    struct ref *ref = malloc(sizeof(struct ref));
    ref->index = index;
    ref->name = malloc(strlen(target) + 1);
    strcpy(ref->name, target);
    return ref;
}

void free_ref(struct ref *ref) {
    free(ref->name);
    free(ref);
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
        if (refs_append_unique(out, create_ref(target, index))) {
            collected++;
        }

        if (collected == max)
            break;
    }

    git_reflog_free(reflog);
cleanup:
    return err;
}

err_t clear_refs(struct refs *refs) {
    err_t err = NO_ERROR;

    ASSERT(refs);

    struct ref *first;
    while ((first = LIST_FIRST(refs)) != NULL) {
        LIST_REMOVE(first, entry);
        free_ref(first);
    }
cleanup:
    return err;
}

size_t get_refs_max_width(struct refs *refs) {
    struct ref *curr;
    size_t max = 0;
    LIST_FOREACH(curr, refs, entry) { max = MAX(max, strlen(curr->name)); }
    return max;
}

void get_co_command(char *out, size_t maxlen, size_t index) {
    if (index == 1) {
        strcpy(out, "git checkout -");
    } else {
        snprintf(out, maxlen, "git checkout @{-%ld}", index);
    }
}

err_t print_refs(WINDOW *win, struct refs *refs) {
    err_t err = NO_ERROR;
    struct ref *curr;
    int line = 1;
    char buff[CHECKOUT_MAX_LEN];
    size_t max = get_refs_max_width(refs);

    ASSERT(win);
    ASSERT(refs);

    LIST_FOREACH(curr, refs, entry) {
        if (curr->index == 0)
            continue;
        wmove(win, line, 1);
        wclrtoeol(win);
        waddstr(win, curr->name);
        wattr_off(win, WA_DIM, NULL);
        if (curr->index) {
            wmove(win, line, max + 2);
            wattr_on(win, WA_DIM, NULL);
            get_co_command(buff, CHECKOUT_MAX_LEN, curr->index);
            waddstr(win, buff);
            wattr_off(win, WA_DIM, NULL);
        }
        line++;
    }
cleanup:
    return err;
}

err_t print_latest_commits(WINDOW *win, git_repository *repo, int max) {
    err_t err = NO_ERROR;
    git_revwalk *walker;
    git_commit *commit;
    int first_line_end;
    int line = 1;
    char hash[6];
    char time[15];
    git_oid next;

    ASSERT(win);
    ASSERT(repo);

    git_revwalk_new(&walker, repo);
    git_revwalk_push_ref(walker, "HEAD");

    while (git_revwalk_next(&next, walker) != GIT_ITEROVER) {
        if (git_commit_lookup(&commit, repo, &next))
            continue;

        sprintf(hash, "%02x%02x ", next.id[0], next.id[1]);

        wclrtoeol(win);
        wmove(win, line, 1);

        wattr_on(win, WA_DIM, NULL);
        wcolor_set(win, COLOR_COMMIT_HASH, NULL);
        waddstr(win, hash);
        wattr_off(win, WA_DIM, NULL);
        wcolor_set(win, 0, NULL);

        wcolor_set(win, COLOR_COMMIT_TITLE, NULL);
        first_line_end = (int)(strchr(git_commit_message(commit), '\n') - git_commit_message(commit));
        waddnstr(win, git_commit_message(commit), MIN(first_line_end, getmaxx(win) - 15 - 15));
        wcolor_set(win, 0, NULL);

        wcolor_set(win, COLOR_COMMIT_USER, NULL);
        wmove(win, line, getmaxx(win) - 15 - 15);
        wattr_on(win, WA_DIM, NULL);
        waddnstr(win, git_commit_committer(commit)->name, 15);
        wattr_off(win, WA_DIM, NULL);
        wcolor_set(win, 0, NULL);

        wcolor_set(win, COLOR_COMMIT_DATE, NULL);
        wmove(win, line, getmaxx(win) - 15);
        get_human_readable_time(git_commit_time(commit), time, 15);
        waddnstr(win, time, 15);
        wcolor_set(win, 0, NULL);

        git_commit_free(commit);
        if (++line == max)
            break;
    }

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

void print_status_entry(const git_status_entry *entry, char *buff, size_t len) {
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
            snprintf(buff, len, "  %s: %s->%s", status, entry->head_to_index->old_file.path,
                     entry->head_to_index->new_file.path);
        } else {
            snprintf(buff, len, "  %s: %s", status, entry->head_to_index->new_file.path);
        }
    }
    if (entry->index_to_workdir) {
        if (strcmp(entry->index_to_workdir->new_file.path, entry->index_to_workdir->old_file.path) != 0) {
            snprintf(buff, len, "  %s: %s->%s", status, entry->index_to_workdir->old_file.path,
                     entry->index_to_workdir->new_file.path);
        } else {
            snprintf(buff, len, "  %s: %s", status, entry->index_to_workdir->new_file.path);
        }
    }
}

err_t print_status(WINDOW *win, git_repository *repo) {
    err_t err = NO_ERROR;
    char buff[200];
    git_status_list *status_list;
    git_status_options opts = {.version = GIT_STATUS_OPTIONS_VERSION,
                               .flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED,
                               .show = GIT_STATUS_SHOW_INDEX_ONLY};

    ASSERT(win);
    ASSERT(repo);

    ASSERT_LIBGIT2(git_status_list_new(&status_list, repo, &opts));

    int line = 1;

    wmove(win, line++, 1);
    wclrtoeol(win);
    waddstr(win, "staged:");
    wcolor_set(win, COLOR_STAGED, NULL);
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        wmove(win, line++, 1);
        print_status_entry(entry, buff, 200);
        wclrtoeol(win);
        waddstr(win, buff);
    }
    wcolor_set(win, 0, NULL);
    git_status_list_free(status_list);
    wmove(win, line++, 0);
    wclrtoeol(win);

    opts =
        (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION, .flags = 0, .show = GIT_STATUS_SHOW_WORKDIR_ONLY};

    git_status_list_new(&status_list, repo, &opts);

    wmove(win, line++, 1);
    wclrtoeol(win);
    waddstr(win, "changed:");
    wcolor_set(win, COLOR_NOT_STAGED, NULL);
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        wmove(win, line++, 1);
        print_status_entry(entry, buff, 200);
        wclrtoeol(win);
        waddstr(win, buff);
    }
    wcolor_set(win, 0, NULL);
    wmove(win, line++, 0);
    wclrtoeol(win);
    git_status_list_free(status_list);

    opts = (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION,
                                .flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED,
                                .show = GIT_STATUS_SHOW_WORKDIR_ONLY};

    git_status_list_new(&status_list, repo, &opts);

    wmove(win, line++, 1);
    wclrtoeol(win);
    waddstr(win, "untracked:");
    wcolor_set(win, COLOR_UNTRACKED, NULL);
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (entry->index_to_workdir && entry->index_to_workdir->status != GIT_DELTA_UNTRACKED) {
            continue;
        }
        wmove(win, line++, 1);
        print_status_entry(entry, buff, 200);
        wclrtoeol(win);
        waddstr(win, buff);
    }
    wcolor_set(win, 0, NULL);
    git_status_list_free(status_list);

cleanup:
    return err;
}

int main() {
    err_t err = NO_ERROR;
    char cwd[PATH_MAX] = { 0 };
    char head_name[100] = { 0 };
    struct refs refs = LIST_HEAD_INITIALIZER();
    git_repository *repo = NULL;
    WINDOW *win = NULL;

    ASSERT(win = initscr());
    ASSERT(getcwd(cwd, PATH_MAX));

    ASSERT(git_libgit2_init() > 0);
    ASSERT(git_repository_init(&repo, cwd, false) == 0);

    ASSERT_LIBGIT2(curs_set(0));
    ASSERT_LIBGIT2(start_color());
    ASSERT_LIBGIT2(use_default_colors());

    ASSERT_LIBGIT2(init_pair(COLOR_STAGED, COLOR_GREEN, -1));
    ASSERT_LIBGIT2(init_pair(COLOR_NOT_STAGED, COLOR_RED, -1));
    ASSERT_LIBGIT2(init_pair(COLOR_UNTRACKED, COLOR_RED, -1));
    ASSERT_LIBGIT2(init_pair(COLOR_TITLE, COLOR_BLACK, COLOR_WHITE));
    ASSERT_LIBGIT2(init_pair(COLOR_COMMIT_HASH, COLOR_BLUE, -1));
    ASSERT_LIBGIT2(init_pair(COLOR_COMMIT_TITLE, -1, -1));
    ASSERT_LIBGIT2(init_pair(COLOR_COMMIT_DATE, -1, -1));
    ASSERT_LIBGIT2(init_pair(COLOR_COMMIT_USER, -1, -1));

    int third = getmaxy(win) / 3;
    int leftover = getmaxy(win) - third * 3;

    WINDOW *topwin = newwin(getmaxy(win) / 3 + (leftover ? 1 : 0), 0, 0, 0);
    WINDOW *middlewin = newwin(getmaxy(win) / 3 + (leftover == 2 ? 1 : 0), 0, getmaxy(win) / 3 + (leftover ? 1 : 0), 0);
    WINDOW *bottomwin = newwin(getmaxy(win) / 3, 0, getmaxy(win) / 3 * 2 + leftover, 0);
    ASSERT(topwin);
    ASSERT(middlewin);
    ASSERT(bottomwin);

    while (1) {
        RETHROW(print_status(topwin, repo));
        RETHROW(get_latest_refs(&refs, repo, getmaxy(middlewin) - 2));
        RETHROW(print_refs(middlewin, &refs));
        RETHROW(clear_refs(&refs));
        RETHROW(print_latest_commits(bottomwin, repo, getmaxy(win) / 3));

        wcolor_set(topwin, COLOR_TITLE, NULL);
        for (int i = 0; i < getmaxx(topwin); i++) {
            wmove(topwin, 0, i);
            waddstr(topwin, " ");
        }
        wmove(topwin, 0, 0);
        waddstr(topwin, " Status (");
        get_head_name(repo, head_name, 100);
        waddstr(topwin, head_name);
        waddstr(topwin, ")");
        wmove(topwin, 0, getmaxx(topwin) / 2 - strlen("Git Live") / 2);
        waddstr(topwin, "Git Live");
        wmove(topwin, 0, getmaxx(topwin) - MIN(strlen(cwd), 60) - 1);
        waddnstr(topwin, cwd, 60);
        wcolor_set(topwin, 0, NULL);

        wcolor_set(middlewin, COLOR_TITLE, NULL);
        for (int i = 0; i < getmaxx(middlewin); i++) {
            wmove(middlewin, 0, i);
            waddstr(middlewin, " ");
        }
        wmove(middlewin, 0, 0);
        waddstr(middlewin, " Latest Branches");
        wcolor_set(middlewin, 0, NULL);

        wcolor_set(bottomwin, COLOR_TITLE, NULL);
        for (int i = 0; i < getmaxx(bottomwin); i++) {
            wmove(bottomwin, 0, i);
            waddstr(bottomwin, " ");
        }
        wmove(bottomwin, 0, 0);
        waddstr(bottomwin, " Commits");
        wcolor_set(bottomwin, 0, NULL);

        wrefresh(topwin);
        wrefresh(middlewin);
        wrefresh(bottomwin);
        usleep(10000);
    }

cleanup:
    delwin(win);
    endwin();
    refresh();
    return err;
}