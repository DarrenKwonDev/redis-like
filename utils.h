#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

const uint32_t MAX_MSG_CHAR_LENGTH = 4096;
const uint32_t HEADER_SIZE = 4;

// fd로부터 n 바이트를 읽어 buf에 저장합니다.
// 정확히 n 바이트를 읽는 것을 목표로 합니다.
int32_t read_full(int fd, char* buf, size_t n);

// fd에게 buf의 n 바이트를 씁니다.
// 정확히 n 바이트를 쓰는 것을 목표로 합니다.
int32_t write_all(int fd, const char* buf, size_t n);