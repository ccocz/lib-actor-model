#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void err(const char *msg, int code) {
    fprintf(stderr, "%s: %d\n", msg, code);
}

void fatal(int ret) {
    if (ret == 0) {
        return;
    }
    fprintf(stderr, "\nfatal error detected\n");
    fprintf(stderr, "error: %s\n", strerror(ret));
    if (errno) {
        fprintf(stderr, "errno: %s\n", strerror(errno));
    }
    abort();
}

