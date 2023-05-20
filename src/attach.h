#ifndef GIT_LIVE_ATTACH_H
#define GIT_LIVE_ATTACH_H

#include <stdint.h>
#include <stdbool.h>
#include "../lib/err.h"
#include "timing.h"

#define SESSION_ID_LEN (4)

struct attach_session;

err_t attach_terminal_to_session(char *session_id);

err_t detach_terminal_to_session(char *session_id);

err_t init_attach_session(struct attach_session**, struct timer*);
err_t free_attach_session(struct attach_session*);

err_t get_attached_workdir(struct attach_session* session, char *out, uint32_t out_len, bool *is_attached);
err_t get_attach_session_id(struct attach_session* session, char *out, uint32_t out_len);

#endif //GIT_LIVE_ATTACH_H
