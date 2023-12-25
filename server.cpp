#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

int32_t handle_one_req(int conn_fd);
void serverLoop(int conn_fd);

int main(int argc, char* argv[]) {

    int serv_sock_fd;
    serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock_fd == -1) {
        std::cout << "socket error" << std::endl;
        return -1;
    }

    // time-wait 상태 제거. 곧바로 재사용 가능하게.
    int reuse_addr = 1;
    socklen_t reuse_addr_len = sizeof(reuse_addr);
    setsockopt(serv_sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, reuse_addr_len);

    // set sock address and binding
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ntohl(INADDR_ANY); // 어떠한 인터페이스로도 들어오는 연결을 수락
    serv_addr.sin_port = htons(1234);

    if ((bind(serv_sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1) {
        std::cout << "bind error" << std::endl;
        return -1;
    }

    // 일단 기본값으론 1024개까지 connection을 받을 수 있다.
    if ((listen(serv_sock_fd, SOMAXCONN)) == -1) {
        std::cout << "listen error" << std::endl;
        return -1;
    }

    // iterative tcp server
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int conn_sock_fd = accept(serv_sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (conn_sock_fd == -1) {
            std::cout << "accept error" << std::endl;
            return -1;
        }

        serverLoop(conn_sock_fd);

        close(conn_sock_fd);
    }

    close(serv_sock_fd);

    return 0;
}

int32_t handle_one_req(int conn_fd) {
    char rbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1]; // +1 for null-terminator

    errno = 0; // errno 초기화

    // 4byte를 읽어 header를 보기 위함.
    int32_t err = read_full(conn_fd, rbuf, HEADER_SIZE);
    if (err == -1) {
        if (errno == 0) {
            std::cout << "EOF\n";
        } else {
            std::cout << "read() error\n";
        }
        return err;
    }

    uint32_t msg_len;
    memcpy(&msg_len, rbuf, HEADER_SIZE);
    msg_len = ntohl(msg_len);
    std::cout << "msg_len " << msg_len << "\n";

    // msg_len을 넘어서는 메시지는 받지 않습니다.
    if (msg_len > MAX_MSG_CHAR_LENGTH) {
        std::cout << "exceed MAX_MSG_CHAR_LENGTH\n";
        return -1;
    }

    // msg_len만큼의 메시지를 읽습니다.
    err = read_full(conn_fd, &rbuf[HEADER_SIZE], msg_len);
    if (err) {
        std::cout << "read error\n";
        return err;
    }

    rbuf[HEADER_SIZE + msg_len] = '\0';
    printf("client said: %s\n", &rbuf[HEADER_SIZE]);

    // send client something
    const char* reply = "server sent this";
    uint32_t reply_len = strlen(reply);
    uint32_t reply_len_network = htonl(reply_len);
    char wbuf[HEADER_SIZE + strlen(reply)];

    memcpy(wbuf, &reply_len_network, HEADER_SIZE); // reply_len의 바이너리 값이 저장됨.
    memcpy(&wbuf[HEADER_SIZE], reply, reply_len);

    return write_all(conn_fd, wbuf, HEADER_SIZE + reply_len);
};

void serverLoop(int conn_fd) {
    assert(conn_fd > 0);

    while (true) {
        int32_t err = handle_one_req(conn_fd);
        if (err) {
            break;
        }
    }
}