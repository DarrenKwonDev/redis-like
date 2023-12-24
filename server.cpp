#include <arpa/inet.h>
#include <cassert>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void something(int conn_fd) {
    assert(conn_fd > 0);

    char rbuf[64];
    ssize_t read_sz = read(conn_fd, rbuf, sizeof(rbuf) - 1);
    if (read_sz < 0) {
        std::cout << "read error" << std::endl;
        return;
    }
    rbuf[read_sz] = '\0';
    printf("client said: %s\n", rbuf);

    char wbuf[] = "server sent this";
    ssize_t write_sz = write(conn_fd, wbuf, strlen(wbuf));
    if (write_sz < 0) {
        std::cout << "write error" << std::endl;
        return;
    }
}

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

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int conn_sock_fd = accept(serv_sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (conn_sock_fd == -1) {
            std::cout << "accept error" << std::endl;
            return -1;
        }

        something(conn_sock_fd);

        close(conn_sock_fd);
    }

    close(serv_sock_fd);

    return 0;
}