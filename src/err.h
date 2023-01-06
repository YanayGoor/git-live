#ifndef GIT_LIVE_ERR_H
#define GIT_LIVE_ERR_H

typedef int err_t;

#define NO_ERROR (0)

#define ASSERT(x) do { \
  if (!(x)) { \
    fprintf(stderr, "runtime error: %s assertion failed at %s:%d\n", #x, __FILE__, __LINE__); \
    err = 1;                   \
    goto cleanup; \
  } \
} while (0)

#define RETHROW(x) do { \
  if (x) { \
    fprintf(stderr, "rethrown from %s:%d\n", __FILE__, __LINE__); \
    err = 1;                   \
    goto cleanup; \
  } \
} while (0)

#endif //GIT_LIVE_ERR_H
