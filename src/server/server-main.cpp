#include "../../include/server.h"
#include "../../include/clients-manage.h"
#include "../../include/workers.h"

int main(int argc, char* argv[]) {
    // 0 get args
    char* serverip = NULL;
    unsigned short serverport;
    if (argc < 2 || argc > 3) {
        printf("Usage: ./server [ip] <port>\n");
        exit(EXIT_FAILURE);
    } else if (argc == 2) {
        serverip = NULL;
        serverport = (unsigned short)atoi(argv[1]);
    } else {
        serverip = argv[1];
        serverport = (unsigned short)atoi(argv[2]);
    }
    // 1 pre
    Server* server = NULL;
    try {
        server = new Server(serverip, serverport, CLIENTS_H);
    } catch (...) {
        fprintf(stderr, "Err: new server failed.\n");
        exit(EXIT_FAILURE);
    }
    Workers workers;
    Client* client = NULL;
    ssize_t recvlen;
    const char* child_construct_arg = "from child";
    TaskQueue<Client*> q_main2ls;
    pthread_t ls_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&ls_tid, &attr, serverResponseFileList, (void*)&q_main2ls)) {
        fprintf(stderr, "Err: create response file list worker failed.\n");
        exit(EXIT_FAILURE);
    }
    pthread_attr_destroy(&attr);
    // 2 main-while
    while (1) {
        // (1) epoll_wait
        int event_count = epoll_wait(server->getEpfd(), server->getEvs(), server->getClientnum() + 1, -1);
        if (event_count <= 0) {
            perror("epoll_wait");
            fprintf(stderr, "Err: epoll wait error.\n");
            continue;
        }
        // (2) process-while
        for (int i = 0; i < event_count; i++) {
            if ((reinterpret_cast<Server*>(server->getEvs()[i].data.ptr))->getSockfd() == server->getSockfd()) {
                try {
                    client = new Client(server, child_construct_arg, 0, CLIENTS_H);
                } catch (...) {
                    fprintf(stderr, "Err: new client falied.\n");
                    delete client;
                    continue;
                }
            } else {
                epoll_ctl(client->getEpfd(), EPOLL_CTL_DEL, client->getSockfd(), NULL);
                client = reinterpret_cast<Client*>(server->getEvs()[i].data.ptr);
                recvlen = recvn(client->getSockfd(), (char*)(client->getPack()->getPackbuf()), client->getPack()->getPACKET_HEAD_SIZE(), MSG_PEEK);
                if (recvlen < 0) {
                    fprintf(stderr, "Err: recv msg peek error.\n");
                    delete client;
                    continue;
                } else if (recvlen == 0) {
                    printf("client closed.\n");
                    delete client;
                    continue;
                } else {
                    if (client->getPack()->getPackbuf()->type == ls) {
                        q_main2ls.enqueue(client);
                    } else {
                        workers.enqueue(worker, client);
                    }
                }
            }
        }
    }
    // 由于错误的使用继承，导致不得不弃用Server的析构函数，但是其实就不用写，因为并没有用共享资源，server和主进程同在，server不需要delete。
    exit(EXIT_SUCCESS);
}

