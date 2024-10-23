#ifndef SERVER_H
#define SERVER_H

#include <unistd.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/socket.h>

#include "m-net.h"

#define CLIENTS_H 1024

class Server {
    private :
        int sockfd = 0;
        int epfd = 0;
        struct epoll_event evs[CLIENTS_H + 1] = {};
        const int CLIENTNUM;
    protected :
        struct epoll_event ev;
    public :
        Server(char* serverip, unsigned short serverport, int clientnum);
        void setSockfd(int sockfd) { this->sockfd = sockfd; }
        int getSockfd() { return this->sockfd; }
        void setEpfd(int epfd) { this->epfd = epfd; }
        int getEpfd() { return this->epfd; }
        struct epoll_event* getEvs() { return this->evs; }
        int getClientnum() { return this->CLIENTNUM; }
        virtual ~Server();
};

Server::Server(char* serverip, unsigned short serverport, int clientnum) : CLIENTNUM(clientnum) {
    // 1 create listen socket
    this->sockfd = createListenSocket(serverip, serverport);
    if (this->sockfd == -1) {
        throw std::runtime_error("Err: create listen socket failed.\n");
    }
    if (makeSocketNonBlocking(this->sockfd) == -1) {
        throw std::runtime_error("Err: make socket non-blocking failed.\n");
    }
    // 2 create epoll
    this->epfd = epoll_create1(0);
    if (this->epfd == -1) {
        throw std::runtime_error("Err: create epoll handler failed.\n");
    }
    // 3 add listen socket to epoll
    this->ev.events = EPOLLIN;
    this->ev.data.ptr = (void*)this;
    if (epoll_ctl(this->epfd, EPOLL_CTL_ADD, this->sockfd, &(this->ev)) == -1) {
        throw std::runtime_error("Err: add listen_fd to epoll failed.\n");
    }
}

Server::~Server() {
    if (this->sockfd != 0 && this->epfd != 0) {
        epoll_ctl(this->epfd, EPOLL_CTL_DEL, this->sockfd, NULL);
        close(this->epfd);
        close(this->sockfd);
    }
}

#endif

