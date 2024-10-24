#ifndef CLIENTS_MANAGE_H
#define CLIENTS_MANAGE_H

#include "server.h"
#include "file-packet.h"

class Client : public Server {
    private :
        FilePackManage* fpm = NULL;
    public :
        Client(Server*& server, const char* a, unsigned short b, int c);
        FilePackManage* getPack() const { return this->fpm; } 
        ~Client();
};

Client::Client(Server*& server, const char* a, unsigned short b, int c) : Server(a, b, c) {
    // 0 set args
    this->setEpfd(server->getEpfd());
    // 1 accept
    int clifd = accept(server->getSockfd(), NULL, NULL);
    if (clifd == -1) {
        perror("accept");
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
        // KEEP-CONN: if (epoll_ctl(this->getEpfd(), EPOLL_CTL_DEL, this->getSockfd(), NULL) == -1) { perror("epoll_ctl(del) in delete client"); }
        close(this->getSockfd());
    }
    delete this->fpm;
}

void* worker(void* arg) {
    Client* client = reinterpret_cast<Client*>(arg);
    if (client->getPack()->getPackbuf()->type == up) {
        if (client->getPack()->recvFile(client->getSockfd())) {
            fprintf(stderr, "Er: worker tell - recv file error.\n");
            delete client;
            pthread_exit((void*)-1);
        }
    } else if (client->getPack()->getPackbuf()->type == down) {
        if (client->getPack()->serverProcessDown(client->getSockfd())) {
            fprintf(stderr, "Err: worker tell - process down error.\n");
            delete client;
            pthread_exit((void*)-1);
        }
    } else {
        fprintf(stderr, "Err: worker tell - unknown type, may be attack.\n");
        delete client;
        pthread_exit((void*)-1);
    }
    delete client;   // KEEP-CONN: 将来要改在线，就不能delete，这是长短连接的最大区别。
    pthread_exit((void*)0);
}

void listFiles(const char* directory, std::string& filenames) {
    DIR* dir = opendir(directory);
    if (dir == NULL) {
        perror("opendir");
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (std::string(entry->d_name) != "." && std::string(entry->d_name) != "..") {
            filenames += entry->d_name;
            filenames += '\0';
        }
    }
    closedir(dir);
}

void* serverResponseFileList(void* arg) {
    // 0 get q_main2ls
    TaskQueue<Client*>* q_main2ls = (TaskQueue<Client*>*)arg;
    // 1 main-while
    ssize_t recvlen;
    Client* client;
    std::string filenames;
    while (1) {
        // (0) get fd from q_main2ls
        client = q_main2ls->dequeue();
        // (1) recv
        bzero((void*)(client->getPack()->getPackbuf()), client->getPack()->getPACKET_HEAD_SIZE());
        recvlen = readn(client->getSockfd(), (char*)(client->getPack()->getPackbuf()), client->getPack()->getPACKET_HEAD_SIZE());
        if (recvlen < 0) {
            fprintf(stderr, "Err: recv get-file-list-head error.\n");
            delete client;
            continue;
        } else if (recvlen == 0) {
            // printf("client closed.\n");
            delete client;
            continue;
        }
        // (2) pre file list
        filenames.clear();
        listFiles("./", filenames);
        // (3) send
        client->getPack()->getPackbuf()->filesize = filenames.length();
        if (writen(client->getSockfd(), (char*)(client->getPack()->getPackbuf()), client->getPack()->getPACKET_HEAD_SIZE()) < 0) {
            fprintf(stderr, "Err: send file list head error.\n");
            continue;
        }
        if (writen(client->getSockfd(), filenames.c_str(), filenames.length()) < 0) {
            fprintf(stderr, "Err: send file list error.\n");
            continue;
        }
    }
    pthread_exit((void*)0);
}

#endif

