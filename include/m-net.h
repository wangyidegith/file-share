#ifndef M_NET_H
#define M_NET_H

#include <sys/types.h>
#include <mqueue.h>

int createListenSocket(const char* ip, unsigned short port);
int createConnectSocket(const char* server_ip, int server_port);
int makeSocketNonBlocking(int sockfd);
int createListenUsocket(const char* SOCKET_PATH);
int createConnectUsocket(const char* SOCKET_PATH);
ssize_t writen(const int fd, const char* buf, const size_t n);
ssize_t readn(const int fd, char* buf, const size_t n);
ssize_t mq_receive_n_b(const mqd_t fd, char* buf, const size_t n);
ssize_t mq_receive_n(const mqd_t fd, char* buf, const size_t n);
ssize_t mq_send_n_b(const mqd_t fd, const char* buf, const size_t n);
ssize_t readn_b(const int fd, char* buf, const size_t n);
ssize_t recvn_b(const int fd, char* buf, const size_t n, int flags);
ssize_t recvn(const int fd, char* buf, const size_t n, int flags);

#endif
