#ifndef STRNDUP_H
#define STRNDUP_H

#include <string.h>
#include <stdlib.h>

// Use a project prefix because newer Windows runtimes also declare strndup.
static inline char* gs2_strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}
#endif // STRNDUP_H
