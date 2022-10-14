#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <git2.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/queue.h>
#include <curses.h>

#define CHECKOUT ("checkout")
#define REFS_COUNT 5

struct ref {
  char *name;
  LIST_ENTRY(ref) entry;
};

LIST_HEAD(refs, ref);

int print_latest_refs(git_repository *repo, size_t max) {
  git_reflog *reflog;
  struct refs refs = LIST_HEAD_INITIALIZER();
  struct ref *curr;
  size_t printed = 0;

  git_reflog_read(&reflog, repo, "HEAD");
  size_t count = git_reflog_entrycount(reflog);
  for (size_t i = 0; i < count && printed < max; i++) {
    const git_reflog_entry *entry = git_reflog_entry_byindex(reflog, i);
    const char *message = git_reflog_entry_message(entry);

    if (strncmp(message, CHECKOUT, sizeof(CHECKOUT) - 1)) {
      continue;
    }

    const char *ref_name = strrchr(message, ' ') + 1;
    struct ref *ref = malloc(sizeof(struct ref));

    ref->name = malloc(strlen(ref_name) + 1);
    memset(ref->name, '\0', strlen(ref_name) + 1);
    strncpy(ref->name, ref_name, strlen(ref_name));

    if (LIST_EMPTY(&refs)) {
     LIST_INSERT_HEAD(&refs, ref, entry);
     printed++;
     continue;
    }
    LIST_FOREACH(curr, &refs, entry) {
      if (strcmp(curr->name, ref->name) == 0)
        break;
      if (LIST_NEXT(curr, entry) == NULL) {
        LIST_INSERT_AFTER(curr, ref, entry);
      }
    }
  }

  int line = 0;
  LIST_FOREACH(curr, &refs, entry) {
    move(line++, 0);
    clrtoeol();
    addstr(curr->name);
  }
  refresh();

  return 0;
}

int main() {
  char cwd[PATH_MAX];
  WINDOW * win;

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
    usleep(10000);
    print_latest_refs(repo, 5);
  }

  delwin(win);
  endwin();
  refresh();
  return 0;
}