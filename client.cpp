#include "utils.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int32_t send_req(int32_t fd, const std::vector<std::string>& cmd) {
    uint32_t len = 4; // cmd 개수를 나타내는 4바이트 추가
    for (const std::string& s : cmd) {
        len += 4 + s.size();
    }
    if (len > MAX_MSG_CHAR_LENGTH) {
        return -1;
    }

    char wbuf[4 + MAX_MSG_CHAR_LENGTH];
    uint32_t len_net = htonl(len);
    memcpy(&wbuf[0], &len_net, 4);

    uint32_t n = cmd.size();
    uint32_t n_net = htonl(n);
    memcpy(&wbuf[4], &n_net, 4);

    size_t cur = 8;
    for (const std::string& s : cmd) {
        uint32_t p = (uint32_t)s.size();
        uint32_t p_net = htonl(p);
        memcpy(&wbuf[cur], &p_net, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += 4 + s.size();
    }
    return write_all(fd, wbuf, 4 + len);
}

int32_t read_res(int32_t fd) {
    // 4 bytes header
    char rbuf[4 + MAX_MSG_CHAR_LENGTH + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            std::cout << "EOF" << std::endl;
        } else {
            std::cout << "read() error" << std::endl;
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    len = ntohl(len);
    if (len > MAX_MSG_CHAR_LENGTH) {
        std::cout << "too long" << std::endl;
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        std::cout << "read() error" << std::endl;
        return err;
    }

    uint32_t rescode = 0;
    if (len < 4) {
        std::cout << "bad response" << std::endl;
        return -1;
    }
    memcpy(&rescode, &rbuf[4], 4);
    rescode = ntohl(rescode); // Convert to host byte order
    printf("[rescode: %u] %.*s\n", rescode, len - 4, &rbuf[8]);
    return 0;
}

int main(int argc, char* argv[]) {
    int32_t client_sock_fd;
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

    std::vector<std::string> cmd;
    for (int32_t i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(client_sock_fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = read_res(client_sock_fd);
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(client_sock_fd);
    return 0;
}