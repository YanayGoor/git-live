#ifndef GIT_LIVE_ATTACH_H
#define GIT_LIVE_ATTACH_H

#include <stdint.h>
#include <stdbool.h>
#include "../lib/err.h"

err_t set_attached_terminal_hash(char *session_id, char *terminal_hash);

err_t attach_terminal_to_session(char *session_id);

err_t detach_terminal_to_session(char *session_id);

err_t try_get_attached_terminal_workdir(char *session_id, char *out, uint32_t out_len, char *inotify_watch_path,
                                        uint32_t inotify_watch_path_len, bool *successful);

#endif //GIT_LIVE_ATTACH_H
