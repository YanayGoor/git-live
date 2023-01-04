#include "layout.h"
#include <curses.h>
#include <git2.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

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

void get_latest_refs(struct refs *out, git_repository *repo, size_t max) {
    git_reflog *reflog;
    size_t collected = 0;

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
}

void clear_refs(struct refs *refs) {
    struct ref *first;
    while ((first = LIST_FIRST(refs)) != NULL) {
        LIST_REMOVE(first, entry);
        free_ref(first);
    }
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

void print_refs(struct node *node, struct refs *refs) {
    struct ref *curr;
    char buff[CHECKOUT_MAX_LEN];

    clear_nodes(node);

    struct node *names = init_child(node);
    names->nodes_direction = nodes_direction_rows;
    names->fit_content = true;
    names->basis = 15;

    struct node *co_commands = init_child(node);
    co_commands->nodes_direction = nodes_direction_rows;
    co_commands->padding_left = 4;
    co_commands->expand = 1;

    LIST_FOREACH(curr, refs, entry) {
        if (curr->index == 0)
            continue;
        append_text(names, curr->name);
        if (curr->index) {
            get_co_command(buff, CHECKOUT_MAX_LEN, curr->index);
            append_styled_text(co_commands, buff, 0, WA_DIM);
        }
    }
}

void print_latest_commits(struct node *node, git_repository *repo, int max) {
    git_revwalk *walker;
    git_commit *commit;
    int line = 1;
    char hash[6];
    char time[16];
    git_oid next;

    git_revwalk_new(&walker, repo);
    git_revwalk_push_ref(walker, "HEAD");

    clear_nodes(node);

    struct node *hash_col = init_child(node);
    hash_col->fit_content = true;
    hash_col->nodes_direction = nodes_direction_rows;

    struct node *msg_col = init_child(node);
    msg_col->expand = 1;
    msg_col->nodes_direction = nodes_direction_rows;
    msg_col->padding_left = 1;

    struct node *user_col = init_child(node);
    user_col->fit_content = true;
    user_col->nodes_direction = nodes_direction_rows;
    user_col->padding_left = 1;

    struct node *time_col = init_child(node);
    time_col->fit_content = true;
    time_col->nodes_direction = nodes_direction_rows;
    time_col->padding_left = 1;
    time_col->padding_right = 1;

    while (git_revwalk_next(&next, walker) != GIT_ITEROVER) {
        if (git_commit_lookup(&commit, repo, &next))
            continue;

        sprintf(hash, "%02x%02x", next.id[0], next.id[1]);

        append_styled_text(hash_col, hash, COLOR_COMMIT_HASH, WA_DIM);

//        first_line_end = (int)(strchr(git_commit_message(commit), '\n') - git_commit_message(commit));
//        waddnstr(win, git_commit_message(commit), MIN(first_line_end, getmaxx(win) - 15 - 15));
//        append_text(node, git_commit_message(commit), MIN(first_line_end, getmaxx(win) - 15 - 15));
        append_styled_text(msg_col, git_commit_message(commit), COLOR_COMMIT_TITLE, 0);

        append_styled_text(user_col, git_commit_committer(commit)->name, COLOR_COMMIT_USER, WA_DIM);

        get_human_readable_time(git_commit_time(commit), time, 15);
        append_styled_text(time_col, time, COLOR_COMMIT_DATE, 0);

        git_commit_free(commit);
        if (++line == max)
            break;
    }
}

int get_head_name(git_repository *repo, char *buff, size_t len) {
    int error = 0;
    git_reference *head = NULL;
    error = git_repository_head(&head, repo);
    if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND)
        return -1;
    else if (!error) {
        strncpy(buff, git_reference_shorthand(head), len);
        git_reference_free(head);
        return 0;
    }
    return -1;
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

void print_status(struct node *node, git_repository *repo) {
    char buff[200] = {0};
    git_status_list *status_list;
    git_status_options opts = {.version = GIT_STATUS_OPTIONS_VERSION,
                               .flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED,
                               .show = GIT_STATUS_SHOW_INDEX_ONLY};

    if (git_status_list_new(&status_list, repo, &opts)) {
        printf("cannot get status list\n");
        exit(1);
    }

    clear_nodes(node);

    append_text(node, "staged:");
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        print_status_entry(entry, buff, 200);
        append_styled_text(node, buff, COLOR_STAGED, 0);
    }
    git_status_list_free(status_list);
    opts =
        (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION, .flags = 0, .show = GIT_STATUS_SHOW_WORKDIR_ONLY};

    if (git_status_list_new(&status_list, repo, &opts)) {
        printf("cannot get status list\n");
        exit(1);
    }

    append_text(node, "changed:");
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        print_status_entry(entry, buff, 200);
        append_styled_text(node, buff, COLOR_NOT_STAGED, 0);
    }
    git_status_list_free(status_list);

    opts = (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION,
                                .flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED,
                                .show = GIT_STATUS_SHOW_WORKDIR_ONLY};

    if (git_status_list_new(&status_list, repo, &opts)) {
        printf("cannot get status list\n");
        exit(1);
    }
    append_text(node, "untracked:");
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (entry->index_to_workdir && entry->index_to_workdir->status != GIT_DELTA_UNTRACKED) {
            continue;
        }
        print_status_entry(entry, buff, 200);
        append_styled_text(node, buff, COLOR_UNTRACKED, 0);
    }
    git_status_list_free(status_list);
}

int main() {
    char cwd[PATH_MAX];
    char head_name[100];
    WINDOW *win;
    struct refs refs = LIST_HEAD_INITIALIZER();

    if ((win = initscr()) == NULL) {
        exit(1);
    }

    if (getcwd(cwd, PATH_MAX) == NULL) {
        exit(1);
    }

    git_libgit2_init();
    git_repository *repo = NULL;
    git_repository_init(&repo, cwd, false);

    curs_set(0);
    start_color();
    use_default_colors();

    init_pair(COLOR_STAGED, COLOR_GREEN, -1);
    init_pair(COLOR_NOT_STAGED, COLOR_RED, -1);
    init_pair(COLOR_UNTRACKED, COLOR_RED, -1);
    init_pair(COLOR_TITLE, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_COMMIT_HASH, COLOR_BLUE, -1);
    init_pair(COLOR_COMMIT_TITLE, -1, -1);
    init_pair(COLOR_COMMIT_DATE, -1, -1);
    init_pair(COLOR_COMMIT_USER, -1, -1);

    struct node *root = init_node();
    root->expand = 1;
    root->fit_content = true;
    root->nodes_direction = nodes_direction_rows;

    struct node *top_header = init_child(root);
    top_header->basis = 1;
    top_header->padding_left = 1;
    top_header->nodes_direction = nodes_direction_columns;
    top_header->color = COLOR_TITLE;

    struct node *top = init_child(root);
    top->expand = 1;
    top->nodes_direction = nodes_direction_rows;
    top->wrap = node_wrap_wrap;
    top->padding_left = 1;
    top->fit_content = true;

    struct node *middle_header = init_child(root);
    middle_header->basis = 1;
    middle_header->padding_left = 1;
    middle_header->color = COLOR_TITLE;

    struct node *middle = init_child(root);
    middle->expand = 1;
    middle->nodes_direction = nodes_direction_columns;
    middle->padding_left = 1;

    struct node *bottom_header = init_child(root);
    bottom_header->basis = 1;
    bottom_header->padding_left = 1;
    bottom_header->color = COLOR_TITLE;

    struct node *bottom = init_child(root);
    bottom->expand = 1;
    bottom->nodes_direction = nodes_direction_columns;
    bottom->padding_left = 1;

    while (1) {
        print_status(top, repo);

        get_latest_refs(&refs, repo, getmaxy(win) - 2); // we get more and some will be hidden
        print_refs(middle, &refs);
        clear_refs(&refs);

        print_latest_commits(bottom, repo, getmaxy(win) / 3);

        clear_nodes(top_header);
        clear_nodes(middle_header);
        clear_nodes(bottom_header);

        get_head_name(repo, head_name, 100);

        struct node* top_header_left = init_child(top_header);
        top_header_left->expand = 1;
        top_header_left->nodes_direction = nodes_direction_columns;

        struct node* title = init_child(top_header);
        title->fit_content = true;
        title->padding_left = 1;
        title->padding_right = 1;
        append_text(title, "Git Live");

        struct node* top_header_right = init_child(top_header);
        top_header_right->expand = 1;
        top_header_right->nodes_direction = nodes_direction_columns;

        struct node* status = init_child(top_header_left);
        status->fit_content = true;
        append_text(top_header_left, "Status");

        struct node* branch = init_child(top_header_left);
        branch->expand = 1;
        branch->padding_left = 1;
        branch->nodes_direction = nodes_direction_columns;
        append_text(branch, "(");
        append_text(branch, head_name);
        append_text(branch, ")");


        struct node* padding = init_child(top_header_right);
        padding->expand = 1;

        append_text(top_header_right, cwd);

        append_text(middle_header, "Latest Branches");
        append_text(bottom_header, "Commits");

        wclear(win);
        print_layout(win, root);
        wrefresh(win);
        usleep(50000);
    }

    delwin(win);
    endwin();
    refresh();
    return 0;
}

// int main() {
//     WINDOW *win;
//
//     if ((win = initscr()) == NULL) {
//         exit(1);
//     }
//
//     curs_set(0);
//     start_color();
//     use_default_colors();
//
//     init_pair(COLOR_STAGED, COLOR_GREEN, -1);
//     init_pair(COLOR_NOT_STAGED, COLOR_RED, -1);
//     init_pair(COLOR_UNTRACKED, COLOR_RED, -1);
//     init_pair(COLOR_TITLE, COLOR_BLACK, COLOR_WHITE);
//     init_pair(COLOR_COMMIT_HASH, COLOR_BLUE, -1);
//     init_pair(COLOR_COMMIT_TITLE, -1, -1);
//     init_pair(COLOR_COMMIT_DATE, -1, -1);
//     init_pair(COLOR_COMMIT_USER, -1, -1);
//
//     struct node *root = init_node();
//     root->expand = 1;
//     root->fit_content = true;
//     root->nodes_direction = nodes_direction_rows;
//
//     struct node *top = init_node();
//     top->expand = 1;
//     top->nodes_direction = nodes_direction_columns;
//     top->color = COLOR_STAGED;
//     top->attr = 0;
//
//     struct node *top_left = init_node();
//     top_left->content =
//     "saaa\naaaa\naaaaaa\naaaaa\naaa\naa\naaaa\naaaaaa\naaaaa\naaa\naa\naaaa\naaaaaa\naaaaa\naaa\naa\naaa"
//                     "a\naaaaaa\naaaaa\naaa\naa";
//     top_left->fit_content = true;
//     top_left->padding_right = 1;
//     top_left->attr = WA_BOLD;
//
//     struct node *top_middle = init_node();
//     top_middle->content = "saaa\naaa\naaaa\naaaaaaaaaaaa\n";
//     top_middle->expand = 1;
//     top_middle->fit_content = true;
//
//     struct node *top_right = init_node();
//     top_right->content = "saaa\naaa\naaaa\n";
//     top_right->expand = 1;
//     top_right->fit_content = true;
//
//     append_node(top, top_left);
//     append_node(top, top_middle);
//     append_node(top, top_right);
//
//     struct node *middle = init_node();
//     middle->expand = 3;
//     middle->fit_content = true;
//     middle->nodes_direction = nodes_direction_rows;
//     middle->padding_top = 1;
//     middle->wrap = node_wrap_wrap;
//
//     struct node *middle_top = init_node();
//     middle_top->content = "bbbb\nbbbb\nbbbb\nbbbb\nbbbb\nbbbb\nbbbb\nbbbb\n";
//     middle_top->fit_content = true;
//     middle_top->padding_left = 1;
//
////    struct node *middle_top = init_node();
////    middle_top->nodes_direction = nodes_direction_rows;
////    middle_top->fit_content = true;
////    middle_top->padding_left = 1;
//
////    struct node *middle_top_1 = init_node();
////    middle_top_1->content = "bbbb\n";
////    middle_top_1->fit_content = true;
////    struct node *middle_top_2 = init_node();
////    middle_top_2->content = "bbbb\n";
////    middle_top_2->fit_content = true;
////    struct node *middle_top_3 = init_node();
////    middle_top_3->content = "bbbb\n";
////    middle_top_3->fit_content = true;
////    struct node *middle_top_4 = init_node();
////    middle_top_4->content = "bbbb\n";
////    middle_top_4->fit_content = true;
////    struct node *middle_top_5 = init_node();
////    middle_top_5->content = "bbbb\n";
////    middle_top_5->fit_content = true;
////    struct node *middle_top_6 = init_node();
////    middle_top_6->content = "bbbb\n";
////    middle_top_6->fit_content = true;
////    struct node *middle_top_7 = init_node();
////    middle_top_7->content = "bbbb\n";
////    middle_top_7->fit_content = true;
////    struct node *middle_top_8 = init_node();
////    middle_top_8->content = "bbbb\n";
////    middle_top_8->fit_content = true;
////
////    append_node(middle_top, middle_top_1);
////    append_node(middle_top, middle_top_2);
////    append_node(middle_top, middle_top_3);
////    append_node(middle_top, middle_top_4);
////    append_node(middle_top, middle_top_5);
////    append_node(middle_top, middle_top_6);
////    append_node(middle_top, middle_top_7);
////    append_node(middle_top, middle_top_8);
//
//    struct node *middle_middle = init_node();
////    middle_middle->content = "cccc\ncccc\ncccc\ncccc\ncccc\ncccc\ncccc\ncccc\n";
//    middle_middle->content = NULL;
//    middle_middle->nodes_direction = nodes_direction_rows;
//    middle_middle->fit_content = true;
//    middle_middle->padding_left = 1;
//
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//    append_text(middle_middle, "cccc");
//
//    struct node *middle_bottom = init_node();
//    middle_bottom->content = "dddd\ndddd\ndddd\ndddd\ndddd\ndddd\ndddd\ndddd\n";
//    middle_bottom->fit_content = true;
//    middle_bottom->padding_left = 1;
//
//    append_node(middle, middle_top);
//    append_node(middle, middle_middle);
//    append_node(middle, middle_bottom);
//
//    struct node *bottom = init_node();
//    bottom->content = "ccccccccccccc";
//    bottom->basis = 4;
//    bottom->padding_right = 1;
//    bottom->padding_left = 1;
//    bottom->padding_bottom = 1;
//    bottom->padding_top = 1;
//
//    append_node(root, top);
//    append_node(root, middle);
//    append_node(root, bottom);
//
//    while (1) {
//        print_layout(win, root);
//        wrefresh(win);
//        usleep(10000);
//    }
//
//    delwin(win);
//    endwin();
//    refresh();
//    return 0;
//}