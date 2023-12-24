#include "utils.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int32_t query(int fd, const char* text) {

    // 쓰기 작업
    uint32_t msg_len = (uint32_t)strlen(text);
    uint32_t msg_len_network = htonl(msg_len);
    if (msg_len > MAX_MSG_CHAR_LENGTH) {
        std::cout << "msg_len " << msg_len << "\n";
        std::cout << "exceed MAX_MSG_CHAR_LENGTH\n";
        return -1;
    }

    char wbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1];
    memcpy(wbuf, &msg_len_network, HEADER_SIZE); // 이렇게 되면, msg_len의 바이너리 값이 저장됨.
    memcpy(&wbuf[4], text, msg_len);

    if (int32_t err = write_all(fd, wbuf, HEADER_SIZE + msg_len)) {
        return err;
    }

    // 읽기 작업
    char rbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, HEADER_SIZE);
    if (err == -1) {
        if (errno == 0) {
            std::cout << "EOF\n";
        } else {
            std::cout << "read() error\n";
        }
        return err;
    }

    memcpy(&msg_len, rbuf, HEADER_SIZE);
    msg_len = ntohl(msg_len);
    std::cout << "msg_len " << msg_len << "\n";
    if (msg_len > MAX_MSG_CHAR_LENGTH) {
        std::cout << "exceed MAX_MSG_CHAR_LENGTH\n";
        return -1;
    }

    err = read_full(fd, &rbuf[HEADER_SIZE], msg_len);
    if (err) {
        std::cout << "read error\n";
        return err;
    }
    rbuf[4 + msg_len] = '\0';
    printf("server says: %s\n", &rbuf[HEADER_SIZE]);
    return 0;
}

int main() {
    int client_sock_fd;
    if ((client_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        std::cout << "socket error" << std::endl;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1
    serv_addr.sin_port = ntohs(1234);
    if ((connect(client_sock_fd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1) {
        std::cout << "connect error" << std::endl;
        return -1;
    }

    // client main logics
    int32_t err = query(client_sock_fd, "yo this is first query");
    if (err) {
        goto L_DONE;
    }
    err = query(client_sock_fd, "behold, next second query");
    if (err) {
        goto L_DONE;
    }
    err = query(client_sock_fd, "and last shot. boom.");
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(client_sock_fd);
    return 0;
}
