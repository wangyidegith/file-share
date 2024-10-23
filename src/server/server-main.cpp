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
    // 2 main-while
    while (1) {
        // (1) epoll_wait
        int event_count = epoll_wait(server->getEpfd(), server->getEvs(), server->getClientnum() + 1, -1);
        if (event_count <= 0) {
            fprintf(stderr, "Err: epoll wait failed.\n");
            continue;
        }
        // (2) process-while
        for (int i = 0; i < event_count; i++) {
            if ((reinterpret_cast<Server*>(server->getEvs()[i].data.ptr))->getSockfd() == server->getSockfd()) {
                try {
                    client = new Client(server, NULL, 0, 0);
                } catch (...) {
                    fprintf(stderr, "Err: new client falied..\n");
                    delete client;
                    continue;
                }
            } else {
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
                        printf("TODO: send client to epoll2ls_queue\n"); delete client; continue;
                    } else {
                        workers.enqueue(worker, client);
                    }
                }
            }
        }
    }
    delete server;
    exit(EXIT_SUCCESS);
}

