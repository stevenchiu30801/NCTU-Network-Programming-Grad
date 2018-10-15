#include "header.h"

void err_sys(const char* x) {
    perror(x);
    exit(1);
}
