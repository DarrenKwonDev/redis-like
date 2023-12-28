#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
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

enum RES_CODE {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};

#pragma pack(push, 1) // for memory padding
struct Conn {         // connection 마다 읽기, 쓰기 버퍼가 생성되어야 함.
    int32_t fd = -1;
    CONN_STATE state = CONN_STATE::STATE_REQ;
    size_t rbuf_size = 0;                             // 현재까지 읽은 데이터의 크기(read)
    size_t wbuf_size = 0;                             // 현재까지 쓴 데이터의 크기(echoing을 위해서)
    size_t wbuf_sent = 0;                             // 현재까지 보낸 데이터의 크기(write)
    char rbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1]; // 읽기 버퍼
    char wbuf[HEADER_SIZE + MAX_MSG_CHAR_LENGTH + 1]; // 쓰기 버퍼
};
#pragma pack(pop)

// 임시적 map
// 정렬을 위해 O(logN)이 소요됨.
static std::map<std::string, std::string> g_map;

void set_fd_non_blocking(int32_t fd);
void conn_state_req_handler(Conn* conn);
void conn_state_res_handler(Conn* conn);
void conn_state_end_handler(Conn* conn);
int32_t accept_new_conn_malloc(std::vector<Conn*>& fd_to_conn, int32_t fd);

int main(int argc, char* argv[]) {

    int32_t serv_sock_fd;
    serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock_fd == -1) {
        std::cout << "socket error" << std::endl;
        return -1;
    }

    // time-wait 상태 제거. 곧바로 재사용 가능하게.
    int32_t reuse_addr = 1;
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

    std::vector<Conn*> fd_to_conn;        // idx if fd, value is pointer to Conn
    std::vector<struct pollfd> poll_args; // 관찰할 fd들의 목록 (0번째에는 server sock 등록 예정)

    while (true) {
        poll_args.clear(); // 매 iteration마다 clear

        // serv_sock_fd의 입력 버퍼에 데이터가 있으면 revent에 POLLIN 이벤트 발생
        struct pollfd pfd = {serv_sock_fd, POLLIN, 0};

        poll_args.push_back(pfd); // poll_args[0] is serv_sock

        // 기 존재하는 connection에 대한 처리
        for (Conn* conn_or_null : fd_to_conn) {
            if (!conn_or_null) { // conn can be NULL
                continue;
            }
            struct pollfd pfd = {};
            pfd.fd = conn_or_null->fd;
            pfd.events = (conn_or_null->state == CONN_STATE::STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        // https://pubs.opengroup.org/onlinepubs/7908799/xsh/poll.html
        // poll을 통과하면서 poll_args의 fd들의 revents에 실제 발생한 이벤트가 등록됨
        int32_t active_fds_cnt = poll(poll_args.data(), (nfds_t)poll_args.size(), -1); // -1 :: infinite timeout
        if (active_fds_cnt < 0) {
            std::cout << "poll error\n";
            continue;
        }

        // serv_sock을 제외한 나머지 fd들에 대한 처리
        for (int32_t i = 1; i < poll_args.size(); i++) {
            assert(i > 0); // 0은 server socket이므로

            // kernel에서 발생한 이벤트에 따른 처리
            if (poll_args[i].revents) {
                Conn* conn = fd_to_conn[poll_args[i].fd];

                // simple state machine
                switch (conn->state) {
                case CONN_STATE::STATE_REQ: // POLLIN. 읽을 데이터가 있으니 처리해주세요
                    conn_state_req_handler(conn);
                    break;
                case CONN_STATE::STATE_RES:
                    conn_state_res_handler(conn); // POLLOUT. 쓸 데이터가 있으니 처리해주세요
                    break;
                case CONN_STATE::STATE_END:
                    conn_state_end_handler(conn);
                    break;
                default:
                    std::cout << "invalid state\n";
                    break;
                }

                if (conn->state == CONN_STATE::STATE_END) {
                    fd_to_conn[conn->fd] = NULL; // fd_to_conn에서 해당 fd를 NULL로 설정
                    (void)close(conn->fd);       // socket 닫기
                    free(conn);                  // conn 메모리 해제
                }
            }
        }

        // 서버 소켓에 이벤트 발생. 새로운 연결을 수락
        if (poll_args[0].revents) {
            accept_new_conn_malloc(fd_to_conn, serv_sock_fd);
        }
    }

    close(serv_sock_fd);

    return 0;
}

// fd를 활용한 read, write는 non blocking으로 작동하게 됩니다.
// 데이터가 없으면 -1이 반환되고 errno에는 EAGAIN, EWOULDBLOCK 이 설정됩니다.
void set_fd_non_blocking(int32_t fd) {
    errno = 0;
    int32_t flags = fcntl(fd, F_GETFL, 0); // file control
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

static uint32_t do_get(const std::vector<std::string>& cmd, char* res, uint32_t* reslen) {
    if (!g_map.count(cmd[1])) {
        return RES_NX;
    }
    std::string& val = g_map[cmd[1]];
    assert(val.size() <= MAX_MSG_CHAR_LENGTH);
    memcpy(res, val.data(), val.size());
    *reslen = (uint32_t)val.size();
    std::cout << "get " << cmd[1] << " = " << val << std::endl;
    return RES_OK;
}

static uint32_t do_set(const std::vector<std::string>& cmd, char* res, uint32_t* reslen) {
    (void)res;
    (void)reslen;
    g_map[cmd[1]] = cmd[2]; // key value mapping
    std::cout << "set " << cmd[1] << " " << cmd[2] << "\n";
    return RES_OK;
}

static uint32_t do_del(const std::vector<std::string>& cmd, char* res, uint32_t* reslen) {
    (void)res;
    (void)reslen;
    g_map.erase(cmd[1]);
    std::cout << "del " << cmd[1] << std::endl;
    return RES_OK;
}

// stream_byte : (stream_byte_size, 2, [len, cmd], [len, cmd])
// payload : (2, [len, cmd], [len, cmd])
int32_t parse_req(const char* payload, size_t stream_byte_len, std::vector<std::string>& out) {
    if (stream_byte_len < 4) {
        return -1;
    }

    uint32_t word_cnt = 0; // '몇 어절?'
    memcpy(&word_cnt, &payload[0], 4);
    word_cnt = ntohl(word_cnt);

    std::cout << "word_cnt " << word_cnt << "\n";

    if (word_cnt > MAX_MSG_CHAR_LENGTH) {
        return -1;
    }

    size_t pos = 4;

    // 한 어절 (len, cmd) 를 처리합니다.
    while (word_cnt--) {
        if (pos + 4 > stream_byte_len) {
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &payload[pos], 4);
        sz = ntohl(sz);
        if (pos + 4 + sz > stream_byte_len) {
            return -1;
        }
        out.push_back(std::string((char*)&payload[pos + 4], sz));
        pos += 4 + sz; // 현재 위치 갱신
    }

    if (pos != stream_byte_len) {
        return -1; // 처리된 길이와 전체 스트림 길이가 일치하는지 검사
    }

    return 0;
}

bool cmd_is(const std::string& word, const char* cmd) {
    return 0 == strcasecmp(word.c_str(), cmd);
}

int32_t
do_request(const char* payload, uint32_t stream_byte_len, uint32_t* rescode, char* resbuffer, uint32_t* reslen) {
    std::vector<std::string> cmd;

    if (0 != parse_req(payload, stream_byte_len, cmd)) {
        std::cout << "parse_req error\n";
        return -1;
    }

    if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        *rescode = do_get(cmd, resbuffer, reslen);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        *rescode = do_set(cmd, resbuffer, reslen);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        *rescode = do_del(cmd, resbuffer, reslen);
    } else {
        // cmd is not recognized
        *rescode = RES_ERR;
        const char* msg = "Unknown cmd";
        strcpy((char*)resbuffer, msg);
        *reslen = strlen(msg);
        return 0;
    }

    return 0;
}

bool try_one_request(Conn* conn) {
    if (conn->rbuf_size < HEADER_SIZE) {
        return false;
    }

    uint32_t stream_byte_len = 0;
    memcpy(&stream_byte_len, &conn->rbuf[0], HEADER_SIZE);
    stream_byte_len = ntohl(stream_byte_len);
    std::cout << "stream_byte_len " << stream_byte_len << "\n";

    if (stream_byte_len > MAX_MSG_CHAR_LENGTH) {
        std::cout << "exceed MAX_MSG_CHAR_LENGTH\n";
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    if (conn->rbuf_size < HEADER_SIZE + stream_byte_len) {
        return false;
    }

    uint32_t wlen = 0;
    uint32_t rescode = 0;
    int32_t err = do_request(&conn->rbuf[4], stream_byte_len, &rescode, &conn->wbuf[4 + 4], &wlen);
    if (err) {
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    // 주의: wlen은 호스트 바이트 오더로 유지해야 함
    uint32_t wlen_network = htonl(wlen + 4); // Include the size of the rescode
    rescode = htonl(rescode);

    memcpy(&conn->wbuf[0], &wlen_network, HEADER_SIZE);
    memcpy(&conn->wbuf[4], &rescode, HEADER_SIZE);
    conn->wbuf_size = wlen + HEADER_SIZE * 2;

    size_t remain = conn->rbuf_size - 4 - stream_byte_len;
    if (remain) {
        memmove(conn->rbuf, &conn->rbuf[4 + stream_byte_len], remain);
    }
    conn->rbuf_size = remain;

    // change state
    conn->state = CONN_STATE::STATE_RES;
    conn_state_res_handler(conn);

    return (conn->state == CONN_STATE::STATE_REQ);
}

// false를 반환하면 while loop를 빠져나옴
bool try_fill_buffer(Conn* conn) {

    assert(conn->rbuf_size < sizeof(conn->rbuf));

    ssize_t read_sz = 0;
    do {
        size_t remain = sizeof(conn->rbuf) - conn->rbuf_size; // 남은 rbuf의 크기
        read_sz = read(conn->fd, &conn->rbuf[conn->rbuf_size], remain);
    } while (read_sz < 0 && errno == EINTR); // signal에 의해 read()가 중단된 경우

    // conn->fd는 non blocking으로 작동함.
    // 즉시 처리할 데이터가 없다면 -1을 반환하고, errno가 EAGAIN 또는 EWOULDBLOCK가 반환함.

    // 즉시 처리할 데이터가 없는 경우
    if (read_sz < 0 && errno == EAGAIN) {
        return false;
    }

    // read()가 실패한 경우
    if (read_sz < 0) {
        std::cout << "read() error\n";
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    // 클라이언트의 연결 종료. EOF
    if (read_sz == 0) {
        if (conn->rbuf_size > 0) {
            std::cout << "unexpected EOF\n";
        } else {
            std::cout << "EOF\n";
        }
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)read_sz; // 읽은 만큼 처리된 사이즈 증가.
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    while (try_one_request(conn)) {
    }

    return (conn->state == CONN_STATE::STATE_REQ);
}

void conn_state_req_handler(Conn* conn) {
    // POLLIN 이벤트가 발생했을 때, rbuf에 데이터를 채워넣는다.
    while (try_fill_buffer(conn)) {
    }
}

// false를 반환하면 while loop를 빠져나옴
bool try_flush_buffer(Conn* conn) {
    ssize_t write_sz = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;

        std::cout << "wbuf_size: " << conn->wbuf_size << std::endl;
        std::cout << "remain: " << remain << std::endl;

        write_sz = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (write_sz < 0 && errno == EINTR); // signal에 의해 write()가 중단된 경우

    // 즉시 처리할 데이터가 없는 경우
    if (write_sz < 0 && errno == EAGAIN) {
        return false;
    }

    // write()가 실패한 경우
    if (write_sz < 0) {
        std::cout << "write() error\n";
        conn->state = CONN_STATE::STATE_END;
        return false;
    }

    conn->wbuf_sent += (size_t)write_sz;
    assert(conn->wbuf_sent <= conn->wbuf_size);

    // 다 썼으므로 종료.
    if (conn->wbuf_sent == conn->wbuf_size) {
        conn->state = CONN_STATE::STATE_REQ;
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        return false;
    }

    return true; // wbuf에 남은 데이터가 있을 수 있으므로, true를 리턴하여 while loop를 계속 돌린다.
}

void conn_state_res_handler(Conn* conn) {
    while (try_flush_buffer(conn)) {
    }
}

void conn_state_end_handler(Conn* conn) {
    assert(0); // should not reach here
}

int32_t accept_new_conn_malloc(std::vector<Conn*>& fd_to_conn, int32_t serv_sock_fd) {

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int32_t conn_sock_fd;
    conn_sock_fd = accept(serv_sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (conn_sock_fd == -1) {
        std::cout << "accept error\n";
        return -1;
    };

    set_fd_non_blocking(conn_sock_fd);

    struct Conn* pa_conn = (struct Conn*)malloc(sizeof(struct Conn));

    if (!pa_conn) {
        // malloc이 실패하면 pa_conn은 NULL이므로 굳이 해제할 필요는 없다.
        std::cout << "malloc error\n";
        close(conn_sock_fd);
        return -1;
    }
    assert(pa_conn != NULL);

    pa_conn->fd = conn_sock_fd;
    pa_conn->state = CONN_STATE::STATE_REQ;
    pa_conn->rbuf_size = 0;
    pa_conn->wbuf_size = 0;
    pa_conn->wbuf_sent = 0;

    if (fd_to_conn.size() <= (size_t)conn_sock_fd) {
        fd_to_conn.resize(conn_sock_fd + 1);
    }

    fd_to_conn[conn_sock_fd] = pa_conn;

    return 0;
}