#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void err(const char *msg, int code) {
    fprintf(stderr, "%s: %d\n", msg, code);
    /*va_list fmt_args;
    fprintf(stderr, "error code: ");
    va_start(fmt_args, msg);
    if (vfprintf(stderr, msg, fmt_args) < 0) {
        fprintf(stderr, " another ");
    }
    va_end(fmt_args);*/
    //exit(EXIT_FAILURE);
}

