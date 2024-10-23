#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>
#include <mqueue.h>

#define LISTEN_QUEUE 24

int createListenSocket(const char* ip, unsigned short port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket(tcp)");
        return -1;
    }
    struct sockaddr_in addr;
    memset((void*)&addr, 0x00, sizeof(struct sockaddr));
    addr.sin_family = AF_INET;
    if (ip == NULL) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, ip, &(addr.sin_addr)) <= 0) {
            perror("inet_pton");
            close(listen_fd);
            return -1;
        }
    }
    addr.sin_port = htons(port);
    // option :
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt addr");
        close(listen_fd);
        return -1;
    }
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt port");
        close(listen_fd);
        return -1;
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
        perror("bind(tcp)");
        close(listen_fd);
        return -1;
    }
    if (listen(listen_fd, LISTEN_QUEUE) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

int createListenUsocket(const char* SOCKET_PATH) {
    int usockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (usockfd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0x00, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    unlink(SOCKET_PATH);
    if (bind(usockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(usockfd);
        return -1;
    }
    listen(usockfd, LISTEN_QUEUE);
    return usockfd;
}

int createConnectSocket(const char* server_ip, int server_port) {
    int conn_fd;
    struct sockaddr_in server_addr;
    if ((conn_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &(server_addr.sin_addr)) <= 0) {
        perror("inet_pton");
        close(conn_fd);
        return -1;
    }
    if (connect(conn_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(conn_fd);
        return -1;
    }
    return conn_fd;
}

int createConnectUsocket(const char* SOCKET_PATH) {
    int usockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (usockfd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    if (connect(usockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(usockfd);
        return -1;
    }
    return usockfd;
}

int makeSocketNonBlocking(int sockfd) {
    int flags, s;
    flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        return -1;
    }
    flags |= O_NONBLOCK;
    s = fcntl(sockfd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl(F_SETFL)");
        return -1;
    }
    return 0;
}

ssize_t writen(const int fd, const char* buf, const size_t n) {   // for no epoll
    int nleft, nwrite, offset;
    nleft = n;
    offset = 0;
    while(nleft > 0) {
        if ((nwrite = write(fd, buf + offset, nleft)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;   // 如果目标缓冲区满则continue
            } else {
                perror("write");
                return nwrite;
            }
        }
        offset += nwrite;
        nleft -= nwrite;
    }
    return n;
}

ssize_t readn(const int fd, char* buf, const size_t n) {   // for LT and non-block
    int nleft = n, nread, offset = 0;
    while (nleft > 0) {
        nread = read(fd, buf + offset, nleft);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;   // 在LT的情况下不可能出现无数据可读的情况，如果出现，那是网络问题接着读，也可以使用unrecv_len做状态记录，状态记录的方法也可以防御攻击者构造小包，但是这需要缓冲区独立
            }
            return nread;
        } else if (nread == 0) {
            perror("read");
            return 0;
        }
        offset += nread;
        nleft -= nread;
    }
    return n;
}

ssize_t readnn(const int fd, char* buf, const size_t n) {   // for LT and non-block
    int nleft = n, nread, offset = 0;
    while (nleft > 0) {
        nread = read(fd, buf + offset, nleft);
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return -2;   // 在外头将返回值给到unrecv_len并修改cur，注意这种情况缓冲区必须独立
            }
            perror("read");
            return nread;
        } else if (nread == 0) {
            return 0;
        }
        offset += nread;
        nleft -= nread;
    }
    return n;
}

ssize_t mq_receive_n(const mqd_t fd, char* buf, const size_t n) {   // for block
    int nleft = n, nread, offset = 0;
    while (nleft > 0) {
        nread = mq_receive(fd, buf + offset, nleft, 0);
        if (nread <= 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("mq_receive");
            return nread;
        }
        offset += nread;
        nleft -= nread;
    }
    return n;
}

ssize_t mq_send_n(const mqd_t fd, const char* buf, const size_t n) {   // for block
    int nleft, nwrite, offset;
    nleft = n;
    offset = 0;
    while(nleft > 0) {
        nwrite = mq_send(fd, buf + offset, nleft, 0);
        if (nwrite < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("mq_send");
            return nwrite;
        }
        offset += nwrite;
        nleft -= nwrite;
    }
    return n;
}

ssize_t readn_b(const int fd, char* buf, const size_t n) {   // for block
    int nleft = n, nread, offset = 0;
    while (nleft > 0) {
        nread = read(fd, buf + offset, nleft);
        if (nread <= 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return nread;
        }
        offset += nread;
        nleft -= nread;
    }
    return n;
}

ssize_t recvn_b(const int fd, char* buf, const size_t n, int flags) {   // for block
    int nleft = n, nread, offset = 0;
    while (nleft > 0) {
        nread = recv(fd, buf + offset, nleft, flags);
        if (nread <= 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return nread;
        }
        offset += nread;
        nleft -= nread;
    }
    return n;
}

ssize_t recvn(const int fd, char* buf, const size_t n, int flags) {   // for nonblock
    int nleft = n, nread, offset = 0;
    while (nleft > 0) {
        nread = recv(fd, buf + offset, nleft, flags);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            return nread;
        } else if (nread == 0) {
            perror("read");
            return 0;
        }
        offset += nread;
        nleft -= nread;
    }
    return n;
}

