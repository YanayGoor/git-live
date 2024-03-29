#ifndef GIT_LIVE_ERR_H
#define GIT_LIVE_ERR_H

#include <stdio.h>
#include <errno.h>

typedef int err_t;

#define NO_ERROR (0)

#define ABORT() do { \
    fprintf(stderr, "runtime error: aborted at %s:%d with errno %d\n", __FILE__, __LINE__, errno); \
    err = 1;      \
    goto cleanup; \
} while (0)

#define ASSERT(x) do { \
  if (!(x)) { \
    fprintf(stderr, "runtime error: %s assertion failed at %s:%d with errno %d\n", #x, __FILE__, __LINE__, errno); \
    err = 1;                   \
    goto cleanup; \
  } \
} while (0)

#define ASSERT_PRINT(x) do { \
  if (!(x)) { \
    fprintf(stderr, "runtime error: %s assertion failed at %s:%d with errno %d\n", #x, __FILE__, __LINE__, errno); \
    err = 1;                   \
  } \
} while (0)

#define RETHROW_PRINT(x) do { \
  err = (x);                            \
  if (err) { \
    fprintf(stderr, "rethrown from %s:%d (%d)\n", __FILE__, __LINE__, err); \
  } \
} while (0)

#define RETHROW(x) do { \
  err = (x);                            \
  if (err) { \
    fprintf(stderr, "rethrown from %s:%d (%d)\n", __FILE__, __LINE__, err); \
    goto cleanup; \
  } \
} while (0)

void init_stderr_buffering(char* buff, size_t sz);
void flush_stderr_buff();
void deinit_stderr_buff();

#endif //GIT_LIVE_ERR_H
