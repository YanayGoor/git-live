#include <stddef.h>
#include <stdio.h>

void init_stderr_buffering(char* buff, size_t sz) {
    setvbuf(stderr, buff, _IOFBF, sz);
}

void flush_stderr_buff() {
    fflush(stderr);
}

void deinit_stderr_buff() {
    setlinebuf(stderr);
}