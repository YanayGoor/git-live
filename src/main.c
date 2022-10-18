#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <git2.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/queue.h>
#include <curses.h>

#define REFLOG_CO_PREFIX "checkout:"

#define CHECKOUT_MAX_LEN 100

#define MAX(x, y) ((x) > (y) ? (x) : (y))


struct ref {
  char *name;
  size_t index;
  LIST_ENTRY(ref) entry;
};

LIST_HEAD(refs, ref);


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

    if (!is_checkout_reflog(entry)) continue;

    const char *target = get_checkout_reflog_target(entry);
    if (refs_append_unique(out, create_ref(target, index))) {
     collected++;
    }

    if (collected == max) break;
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
  LIST_FOREACH(curr, refs, entry) {
    max = MAX(max, strlen(curr->name));
  }
  return max;
}

void get_co_command(char *out, size_t maxlen, size_t index) {
  if (index == 1) {
    strcpy(out, "git checkout -");
  } else {
    snprintf(out, maxlen, "git checkout @{-%ld}", index);
  }
}

void print_refs(WINDOW *win, struct refs *refs) {
  struct ref *curr;
  size_t line = 0;
  char buff[CHECKOUT_MAX_LEN];
  size_t max = get_refs_max_width(refs);

  LIST_FOREACH(curr, refs, entry) {
    wmove(win, line, 0);
    wclrtoeol(win);
    waddstr(win, curr->name);
    if (curr->index) {
      wmove(win, line, max + 1);
      wattr_on(win, WA_DIM, NULL);
      get_co_command(
          buff,
        CHECKOUT_MAX_LEN,
        curr->index
      );
      waddstr(win, buff);
      wattr_off(win, WA_DIM, NULL);
    }
    line++;
  }
  wrefresh(win);
}

void print_latest_commits(WINDOW *win, git_repository *repo, int max) {
  git_revwalk *walker;
  git_commit *commit;
  int line = 0;
  char hash[6];
  git_oid next;

  git_revwalk_new(&walker, repo);
  git_revwalk_push_ref(walker, "HEAD");

  while (git_revwalk_next(&next, walker) != GIT_ITEROVER) {
    if (git_commit_lookup(&commit, repo, &next)) continue;

    sprintf(hash, "%02x%02x ", next.id[0], next.id[1]);

    wclrtoeol(win);
    wmove(win, line, 0);

    wattr_on(win, WA_DIM, NULL);
    waddstr(win, hash);
    wattr_off(win, WA_DIM, NULL);

    waddnstr(win, git_commit_message(commit), getmaxx(win) - 15);

    wmove(win, line, getmaxx(win) - 15);
    wattr_on(win, WA_DIM, NULL);
    waddnstr(win, git_commit_committer(commit)->name, 15);
    wattr_off(win, WA_DIM, NULL);

    git_commit_free(commit);
    if (++line == max) break;
  }
  wrefresh(win);
}

void print_status_entry(const git_status_entry *entry, char *buff, size_t len) {
  const char *status = "";
  if (entry->status & (GIT_STATUS_INDEX_NEW | GIT_STATUS_WT_NEW)) {
    status = "new";
  }
  else if (entry->status & (GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED)) {
    status = "renamed";
  }
  else if (entry->status & (GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_MODIFIED)) {
    status = "modified";
  }
  else if (entry->status & (GIT_STATUS_INDEX_DELETED | GIT_STATUS_WT_DELETED)) {
    status = "deleted";
  }
  if (entry->head_to_index) {
    if (strcmp(entry->head_to_index->new_file.path, entry->head_to_index->old_file.path) != 0) {
      snprintf(buff, len, "  %s %s->%s", status, entry->head_to_index->old_file.path, entry->head_to_index->new_file.path);
    } else {
      snprintf(buff, len, "  %s %s", status, entry->head_to_index->new_file.path);
    }
  }
  if (entry->index_to_workdir) {
    if (strcmp(entry->index_to_workdir->new_file.path, entry->index_to_workdir->old_file.path) != 0) {
      snprintf(buff, len, "  %s %s->%s", status, entry->index_to_workdir->old_file.path, entry->index_to_workdir->new_file.path);
    } else {
      snprintf(buff, len, "  %s %s", status, entry->index_to_workdir->new_file.path);
    }
  }
}

void print_status(WINDOW *win, git_repository *repo) {
  char buff[200];
  git_status_list *status_list;
  git_status_options opts = {.version = GIT_STATUS_OPTIONS_VERSION, .flags=GIT_STATUS_OPT_INCLUDE_UNTRACKED, .show=GIT_STATUS_SHOW_INDEX_ONLY};

  git_status_list_new(&status_list, repo, &opts);

  int line = 0;

  wmove(win, line++, 0);
  wclrtoeol(win);
  waddstr(win, "staged:");
  for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
    const git_status_entry *entry = git_status_byindex(status_list, i);
    wmove(win, line++, 0);
    print_status_entry(entry, buff, 200);
    wclrtoeol(win);
    waddstr(win, buff);
  }
  git_status_list_free(status_list);

  opts = (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION, .flags=0, .show=GIT_STATUS_SHOW_WORKDIR_ONLY};

  git_status_list_new(&status_list, repo, &opts);

  wmove(win, line++, 0);
  wclrtoeol(win);
  waddstr(win, "not staged:");
  for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
    const git_status_entry *entry = git_status_byindex(status_list, i);
    wmove(win, line++, 0);
    print_status_entry(entry, buff, 200);
    wclrtoeol(win);
    waddstr(win, buff);
  }

  opts = (git_status_options){.version = GIT_STATUS_OPTIONS_VERSION, .flags=GIT_STATUS_OPT_INCLUDE_UNTRACKED, .show=GIT_STATUS_SHOW_WORKDIR_ONLY};

  git_status_list_free(status_list);
  git_status_list_new(&status_list, repo, &opts);

  wmove(win, line++, 0);
  wclrtoeol(win);
  waddstr(win, "untracked:");
  for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
    const git_status_entry *entry = git_status_byindex(status_list, i);
    if (entry->index_to_workdir && entry->index_to_workdir->status != GIT_DELTA_UNTRACKED) {
      continue;
    }
    wmove(win, line++, 0);
    print_status_entry(entry, buff, 200);
    wclrtoeol(win);
    waddstr(win, buff);
  }

  git_status_list_free(status_list);
  wrefresh(win);
}

int main() {
  char cwd[PATH_MAX];
  WINDOW * win;
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

  WINDOW *topwin = newwin(getmaxy(win) / 3, 0, 0, 0);
  WINDOW *middlewin = newwin(getmaxy(win) / 3, 0, getmaxy(win) / 3, 0);
  WINDOW *bottomwin = newwin(getmaxy(win) / 3, 0, getmaxy(win) * 2 / 3, 0);

  while (1) {
    print_status(topwin, repo);
    get_latest_refs(&refs, repo, getmaxy(middlewin) - 1);
    print_refs(middlewin, &refs);
    clear_refs(&refs);
    print_latest_commits(bottomwin, repo, getmaxy(bottomwin) - 1);
    usleep(10000);
  }

  delwin(win);
  endwin();
  refresh();
  return 0;
}