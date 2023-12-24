#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

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

    char msg[] = "hello from client";
    msg[sizeof(msg) - 1] = '\0';
    ssize_t write_sz = write(client_sock_fd, msg, strlen(msg));
    if (write_sz < 0) {
        std::cout << "write error" << std::endl;
        return -1;
    }

    char rbuf[64];
    ssize_t read_sz = read(client_sock_fd, rbuf, sizeof(rbuf) - 1);
    if (read_sz < 0) {
        std::cout << "read error" << std::endl;
        return -1;
    }
    printf("server said: %s\n", rbuf);

    close(client_sock_fd);

    return 0;
}
