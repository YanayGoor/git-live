#ifndef GIT_LIVE_ERR_H
#define GIT_LIVE_ERR_H

#include "errno.h"

typedef int err_t;

#define NO_ERROR (0)

#define ASSERT(x) do { \
  errno = 0;           \
  if (!(x)) { \
    fprintf(stderr, "runtime error: %s assertion failed at %s:%d with errno %d\n", #x, __FILE__, __LINE__, errno); \
    err = 1;                   \
    goto cleanup; \
  } \
} while (0)

#define ASSERT_PRINT(x) do { \
  errno = 0;           \
  if (!(x)) { \
    fprintf(stderr, "runtime error: %s assertion failed at %s:%d with errno %d\n", #x, __FILE__, __LINE__, errno); \
    err = 1;                   \
  } \
} while (0)

#define RETHROW_PRINT(x) do { \
  if (x) { \
    fprintf(stderr, "rethrown from %s:%d (%d)\n", __FILE__, __LINE__, (x)); \
    err = 1;                   \
  } \
} while (0)

#define RETHROW(x) do { \
  if (x) { \
    fprintf(stderr, "rethrown from %s:%d (%d)\n", __FILE__, __LINE__, (x)); \
    err = 1;                   \
    goto cleanup; \
  } \
} while (0)

#endif //GIT_LIVE_ERR_H
