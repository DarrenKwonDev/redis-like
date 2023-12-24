#include "utils.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <unistd.h>

int32_t read_full(int fd, char* buf, size_t n) {
    while (n > 0) {
        ssize_t read_sz = read(fd, buf, n);
        if (read_sz <= 0) { // EOF or error
            return -1;
        }
        assert((size_t)read_sz <= n);
        n -= (size_t)read_sz;
        buf += read_sz; // buf ptr move
    }
    return 0;
}

int32_t write_all(int fd, const char* buf, size_t n) {
    printf("write_all: %s\n", buf);

    while (n > 0) {
        ssize_t write_sz = write(fd, buf, n);
        if (write_sz <= 0) { // EOF or error
            return -1;
        }
        assert((size_t)write_sz <= n);
        n -= (size_t)write_sz;
        buf += write_sz; // buf ptr move
    }
    return 0;
}
