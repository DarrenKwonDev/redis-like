#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/_types/_socklen_t.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "utils.h"

enum class CONN_STATE {
    STATE_REQ = 0, // 요청을 읽기 위한 상태
    STATE_RES = 1, // 응답을 보내기 위한 상태
    STATE_END = 2, // 연결을 끊기 위한 상태
};

#pragma pack(push, 1) // for memory padding
struct Conn {
    int fd = -1;
    CONN_STATE state = CONN_STATE::STATE_REQ;
    size_t rbuf_size = 0;
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    char rbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1];
    char wbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1];
};
#pragma pack(pop)

void set_fd_non_blocking(int fd);
void conn_state_req_handler(Conn* conn);
void conn_state_res_handler(Conn* conn);
void conn_state_end_handler(Conn* conn);
int32_t accept_new_conn(std::vector<Conn*>& fd_to_conn, int fd);

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

    set_fd_non_blocking(serv_sock_fd);

    std::vector<Conn*> fd_to_conn; // idx if fd, value is pointer to Conn
    std::vector<struct pollfd> poll_args;

    while (true) {
        poll_args.clear();

        struct pollfd pfd = {serv_sock_fd, POLLIN, 0};
        poll_args.push_back(pfd); // poll_args[0] is serv_sock

        for (Conn* conn : fd_to_conn) {
            if (!conn) { // conn can be NULL
                continue;
            }
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == CONN_STATE::STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        // https://pubs.opengroup.org/onlinepubs/7908799/xsh/poll.html
        int active_fds_cnt = poll(poll_args.data(), (nfds_t)poll_args.size(), -1); // -1 :: infinite timeout
        if (active_fds_cnt < 0) {
            std::cout << "poll error\n";
            continue;
        }

        for (int i = 1; i < poll_args.size(); i++) {
            assert(i > 0); // 0은 server socket이므로

            // kernel에서 발생한 이벤트에 따른 처리
            if (poll_args[i].revents) {
                Conn* conn = fd_to_conn[poll_args[i].fd];

                switch (conn->state) {
                case CONN_STATE::STATE_REQ:
                    conn_state_req_handler(conn);
                    break;
                case CONN_STATE::STATE_RES:
                    conn_state_res_handler(conn);
                    break;
                case CONN_STATE::STATE_END:
                    conn_state_end_handler(conn);
                    break;
                default:
                    std::cout << "invalid state\n";
                    break;
                }

                if (conn->state == CONN_STATE::STATE_END) {
                    fd_to_conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn);
                }
            }
        }

        // 서버 소켓에 이벤트 발생. 새로운 연결을 수락
        if (poll_args[0].revents) {
            accept_new_conn(fd_to_conn, serv_sock_fd);
        }
    }

    close(serv_sock_fd);

    return 0;
}

void set_fd_non_blocking(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0); // file control
    if (errno) {
        std::cout << "fcntl error\n";
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    int s = fcntl(fd, F_SETFL, flags);
    if (s == -1 || errno) {
        std::cout << "fcntl error" << std::endl;
        return;
    }
}

bool try_one_request(Conn* conn) {
    if (conn->rbuf_size < HEADER_SIZE) {
        return false;
    }

    uint32_t msg_len = 0;
    memcpy(&msg_len, &conn->rbuf[0], HEADER_SIZE);
    msg_len = ntohl(msg_len);
    std::cout << "msg_len " << msg_len << "\n";

    if (msg_len > MAX_MSG_CHAR_LENGTH) {
        std::cout << "exceed MAX_MSG_CHAR_LENGTH\n";
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    if (conn->rbuf_size < HEADER_SIZE + msg_len) {
        return false;
    }

    printf("client said: %.*s\n", msg_len, &conn->rbuf[4]);

    // echoing
    uint32_t msg_len_network = htonl(msg_len);
    memcpy(&conn->wbuf[0], &msg_len_network, HEADER_SIZE);
    memcpy(&conn->wbuf[4], &conn->rbuf[4], msg_len);
    conn->wbuf_size = HEADER_SIZE + msg_len;

    size_t remain = conn->rbuf_size - (HEADER_SIZE + msg_len);
    if (remain > 0) {
        memmove(&conn->rbuf[0], &conn->rbuf[HEADER_SIZE + msg_len], remain);
    }
    conn->rbuf_size = remain;

    conn->state = CONN_STATE::STATE_RES;
    conn_state_res_handler(conn);

    return (conn->state == CONN_STATE::STATE_REQ);
}

bool try_fill_buffer(Conn* conn) {
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t read_sz = 0;
    do {
        size_t remain = sizeof(conn->rbuf) - conn->rbuf_size;
        read_sz = read(conn->fd, conn->rbuf + conn->rbuf_size, remain);
    } while (read_sz < 0 && errno == EINTR);

    if (read_sz < 0 && errno == EAGAIN) {
        return false;
    }
    if (read_sz < 0) {
        std::cout << "read() error\n";
        conn->state = CONN_STATE::STATE_END;
        return false;
    }
    if (read_sz == 0) {
        if (conn->rbuf_size > 0) {
            std::cout << "unexpected EOF\n";
        } else {
            std::cout << "EOF\n";
        }
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)read_sz;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    while (try_one_request(conn)) {
    }
    return (conn->state == CONN_STATE::STATE_REQ);
}

void conn_state_req_handler(Conn* conn) {
    while (try_fill_buffer(conn)) {
    }
}

bool try_flush_buffer(Conn* conn) {
    ssize_t write_sz = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        write_sz = write(conn->fd, conn->wbuf + conn->wbuf_sent, remain);
    } while (write_sz < 0 && errno == EINTR);

    if (write_sz < 0 && errno == EAGAIN) {
        return false;
    }
    if (write_sz < 0) {
        std::cout << "write() error\n";
        conn->state = CONN_STATE::STATE_END;
        return false;
    }
    conn->wbuf_sent += (size_t)write_sz;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if (conn->wbuf_sent == conn->wbuf_size) {
        conn->state = CONN_STATE::STATE_REQ;
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        return false;
    }
    return true; // wbuf에 남은 데이터가 있을 수 있으므로, true를 리턴
}
void conn_state_res_handler(Conn* conn) {
    while (try_flush_buffer(conn)) {
    }
}

void conn_state_end_handler(Conn* conn) {
    assert(0); // should not reach here
}

int32_t accept_new_conn(std::vector<Conn*>& fd_to_conn, int serv_sock_fd) {

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int conn_sock_fd;
    conn_sock_fd = accept(serv_sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (conn_sock_fd == -1) {
        std::cout << "accept error\n";
        return -1;
    };

    set_fd_non_blocking(conn_sock_fd);

    struct Conn* conn = (struct Conn*)malloc(sizeof(struct Conn)); // conn should be free
    if (!conn) {
        std::cout << "malloc error\n";
        close(conn_sock_fd);
        return -1;
    }

    conn->fd = conn_sock_fd;
    conn->state = CONN_STATE::STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;

    if (fd_to_conn.size() <= (size_t)conn_sock_fd) {
        fd_to_conn.resize(conn_sock_fd + 1);
    }

    fd_to_conn[conn_sock_fd] = conn;

    return 0;
}