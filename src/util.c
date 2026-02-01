#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// Robust write: write exactly n bytes unless error
ssize_t write_all(int fd, const void* buf, size_t n) {
    const char* p = (const char*)buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) return -1;
        p += w;
        left -= (size_t)w;
    }
    return (ssize_t)n;
}

// Robust read: read exactly n bytes unless EOF/error
ssize_t read_all(int fd, void* buf, size_t n) {
    char* p = (char*)buf;
    size_t left = n;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0; // EOF
        p += r;
        left -= (size_t)r;
    }
    return (ssize_t)n;
}

