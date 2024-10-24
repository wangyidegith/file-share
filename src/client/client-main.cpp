#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/m-net.h"
#include "../../include/file-packet.h"

int main(int argc, char* argv[]) {
    // 0 get args : ./client <server-ip> <server-port> ls | up <file1> <file2> ... | down <file1> <file2> ...
    // notice: down <file> as well all
    if (argc < 4) {
        printf("/client <server-ip> <server-port> ls | up <file1> <file2> ... | down <file1> <file2> ...\nnotice: down <file> as well all\n");
        exit(EXIT_FAILURE);
    }
    bool isall = 0;
    int i;
    int files_num;
    char** files;
    Type type;
    if (strncmp(argv[3], "ls", 2)) {
        if (strncmp(argv[3], "up", 2)) {
            if (strncmp(argv[3], "down", 4)) {
                printf("/client <server-ip> <server-port> ls | up <file1> <file2> ... | down <file1> <file2> ...\nnotice: down <file> as well all\n");
                exit(EXIT_FAILURE);
            } else {
                if (argc < 5) {
                    printf("/client <server-ip> <server-port> ls | up <file1> <file2> ... | down <file1> <file2> ...\nnotice: down <file> as well all\n");
                    exit(EXIT_FAILURE);
                } else {
                    type = down;
                    if (!strncmp(argv[4], "all", 3)) {
                        // down all
                        isall = 1;
                    }
                }
            }
        } else { type = up; }
        if (isall) {} else {
            if (argc < 5) {
                printf("/client <server-ip> <server-port> ls | up <file1> <file2> ... | down <file1> <file2> ...\nnotice: down <file> as well all\n");
                exit(EXIT_FAILURE);
            } else {
                files_num = argc - 4;
                files = (char**)calloc(files_num, sizeof(char*));
                for (i = 0; i < files_num; i++) {
                    files[i] = (char*)calloc(((pathconf("/", _PC_NAME_MAX)) * 2 + 1), sizeof(char));   // 文件路径一般不会很长的，我们这里使用两个文件名最长长度
                }
                // get filename-list
                for (i = 0; i < (argc - 4); i++) {
                    strncpy(files[i], argv[4 + i], strlen(argv[4 + i]));
                }
            }
        }
    } else { type = ls; }
    // 1 create connect socket
    char* server_ip = argv[1];
    unsigned short server_port = (unsigned short)atoi(argv[2]);
    int sockfd = createConnectSocket(server_ip, server_port);
    if (sockfd == -1) {
        fprintf(stderr, "Err: create connect socket falied.\n");
        exit(EXIT_FAILURE);
    }
    FilePackManage* fpm = NULL;
    try {
        fpm = new FilePackManage(sizeof(FilePack), FILE_BLOCK_SIZE_H);
    } catch (...) {
        fprintf(stderr, "create FilePackManage failed.\n");
        exit(EXIT_FAILURE);
    }
    // 2 process accoring to arg
    switch (type) {
        case ls :
            if (fpm->clientGetServerFileList(sockfd)) {
                fprintf(stderr, "Err: ls - send failed.\n");
            }
            break;
        case up :
            for (i = 0; i < files_num; i++) {
                if (fpm->sendFile(sockfd, files[i])) {
                    fprintf(stderr, "Err: up - send %s failed.\n", files[i]);
                }
            }
            break;
        case down :
            for (i = 0; i < files_num; i++) {
                if (fpm->downFile(sockfd, files[i])) {
                    fprintf(stderr, "Err: down - send %s failed.\n", files[i]);
                }
            }
            break;
        default :
            fprintf(stderr, "Err: unknown type.");
            break;
    }
    // 3 free
    delete fpm;
    for (i = 0; i < files_num; i++) {
        free(files[i]);
    }
    free(files);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

