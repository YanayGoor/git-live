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

  while (1) {
    get_latest_refs(&refs, repo, getmaxx(win));
    print_refs(win, &refs);
    clear_refs(&refs);
    usleep(10000);
  }

  delwin(win);
  endwin();
  refresh();
  return 0;
}