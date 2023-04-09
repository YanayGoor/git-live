#include <stdio.h>
#include <string.h>
#include "../lib/err.h"
#include "attach.h"
#include "dashboard.h"

void print_usage() {
    fprintf(stderr, "Usage: git live <command>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  <none>       Run a new git-live dashboard.\n");
    fprintf(stderr, "  attach       Attach a running dashboard to the current terminal so that the paths are relative "
                    "to its cwd.\n");
    fprintf(stderr, "  detach       Detach a running dashboard from the terminal it is attached to.\n");
}

void print_attach_usage() {
    fprintf(stderr, "Usage: git live attach <session_id>\n");
}

void print_detach_usage() {
    fprintf(stderr, "Usage: git live detach <session_id>\n");
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        return run_dashboard();
    } else if (!strcmp(argv[1], "attach")) {
        if (argc == 3 && strcmp(argv[2], "--help")) {
            return attach_terminal_to_session(argv[2]);
        } else if (argc > 3) {
            fprintf(stderr, "Too many arguments.\n");
            print_attach_usage();
        } else {
            print_attach_usage();
        }
    } else if (!strcmp(argv[1], "detach")) {
        if (argc == 3 && strcmp(argv[2], "--help")) {
            return detach_terminal_to_session(argv[2]);
        } else if (argc > 3) {
            fprintf(stderr, "Too many arguments.\n");
            print_detach_usage();
        } else {
            print_detach_usage();
        }
    } else if (!strcmp(argv[1], "--help")) {
        print_usage();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
    }
    return 1;
}