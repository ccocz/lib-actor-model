#include <stdio.h>
#include <stdlib.h>

void err(const char *msg) {
    fprintf(stderr, "%s", msg);
    exit(EXIT_FAILURE);
}
