#ifndef CLIENTS_MANAGE_H
#define CLIENTS_MANAGE_H

#include "server.h"
#include "file-packet.h"

class Client : public Server {
    private :
        FilePackManage* fpm = NULL;
    public :
        Client(Server*& server, char* a, unsigned short b, int c);
        FilePackManage* getPack() { return this->fpm; } 
        ~Client();
};

Client::Client(Server*& server, char* a, unsigned short b, int c) : Server(a, b, c) {
    // 0 set args
    this->setEpfd(server->getEpfd());
    // 1 accept
    int clifd = accept(server->getSockfd(), NULL, NULL);
    if (clifd == -1) {
        fprintf(stderr, "Err: accept failed.\n");
        throw;
    }
    if (makeSocketNonBlocking(clifd) == -1) {
        close(clifd);
        fprintf(stderr, "Err: make socket non-blocking failed.\n");
        throw;
    }
    // 2 add client socket to epoll
    this->ev.events = EPOLLIN | EPOLLET;
    this->ev.data.ptr = (void*)this;
    if (epoll_ctl(this->getEpfd(), EPOLL_CTL_ADD, clifd, &(this->ev)) == -1) {
        close(clifd);
        fprintf(stderr, "Err: add client_fd to epoll failed.\n");
        throw;
    }
    // 3 set sockfd
    this->setSockfd(clifd);
    // 4 new FilePackManage
    try {
        this->fpm = new FilePackManage(sizeof(FilePack), FILE_BLOCK_SIZE_H);
    } catch (...) {
        fprintf(stderr, "Err: new FilePackManage failed.\n");
        throw std::runtime_error("Err: new FilePackManage failed.\n");
    }
}

Client::~Client() {
    if (this->getSockfd() != 0 && this->getEpfd() != 0) {
        epoll_ctl(this->getEpfd(), EPOLL_CTL_DEL, this->getSockfd(), NULL);
        close(this->getSockfd());
    }
    delete this->fpm;
}

void* worker(void* arg) {
    Client* client = reinterpret_cast<Client*>(arg);
    if (client->getPack()->getPackbuf()->type == up) {
        if (client->getPack()->recvFile(client->getSockfd())) {
            fprintf(stderr, "recv file error.\n");
            delete client;
            pthread_exit((void*)-1);
        }
    } else if (client->getPack()->getPackbuf()->type == down) {
        if (client->getPack()->serverProcessDown(client->getSockfd())) {
            fprintf(stderr, "process down error.\n");
            delete client;
            pthread_exit((void*)-1);
        }
        pthread_exit((void*)-1);
    } else {
        fprintf(stderr, "Err: unknown type, may be attack.\n");
        delete client;
        pthread_exit((void*)-1);
    }
    delete client;   // 将来要改在线，就不能delete，这是长短连接的最大区别。
    pthread_exit((void*)0);
}

#endif

