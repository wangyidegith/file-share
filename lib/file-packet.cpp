#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdexcept>

#include "../include/m-net.h"
#include "../include/file-packet.h"

static char* getFileName(char* file_path) {
    char* fn = strrchr(file_path, '/');
    if (fn == NULL) {
        return file_path;
    } else if (strlen(fn) == 1) {
        file_path[(strlen(file_path) - 1) * sizeof(char)] = '\0';
        return getFileName(file_path);
    } else {
        return ++fn;
    }
}

static bool judgeFileExisted(const char* filepath) {
    if (access(filepath, F_OK) == 0) {
        return true;
    } else {
        return false;
    }
}

static int encodeFilePackInit(const char* filepath, FilePack* pack) {
    // 0 filefd
    int filefd = open(filepath, O_RDONLY, 0664);
    if (filefd == -1) {
        fprintf(stderr, "Err: open file failed.\n");
        return -1;
    }
    // 1 type
    pack->type = up;
    // 2 filesize
    struct stat filestat;
    if (stat(filepath, &filestat)) {
        fprintf(stderr, "Err: get file's stat failed.\n");
        return -1;
    }
    pack->filesize = filestat.st_size;
    // 3 filename
    size_t filepath_size = (strlen(filepath) + 1) * sizeof(char);
    char filepath_tmp[filepath_size];
    bzero((void*)filepath_tmp, filepath_size);
    strncpy(filepath_tmp, filepath, strlen(filepath));
    char* filename = getFileName(filepath_tmp);
    strncpy(pack->filename, filename, strlen(filename));
    return filefd;
}

FilePackManage::FilePackManage(int PACKET_HEAD_SIZE, int FILE_BLOCK_SIZE) : PACKET_HEAD_SIZE(PACKET_HEAD_SIZE), FILE_BLOCK_SIZE(FILE_BLOCK_SIZE) {
    this->packsize = (this->getPACKET_HEAD_SIZE() + this->getFILE_BLOCK_SIZE() + 1) * sizeof(char);
    this->pack = (FilePack*)calloc(this->packsize, 1);
    if (this->pack == NULL) {
        throw std::runtime_error("Err: create buf failed.\n");
    }
}

FilePackManage::~FilePackManage() {
    if (this->pack != NULL) {
        free((void*)(this->pack));
    }
}

int FilePackManage::sendFile(int sockfd, const char* filepath) {
    // 0 check args
    if (filepath == NULL) {
        fprintf(stderr, "Err: file path is null.\n");
        return -1;
    }
    // 1 judge file exist
    if (!judgeFileExisted(filepath)) {
        fprintf(stderr, "Err: file is not existed.\n");
        return -1;   
    }
    // 3 fill pack
    bzero((void*)(this->pack), this->packsize);
    int filefd = encodeFilePackInit(filepath, this->pack);
    if (filefd < 0) {
        fprintf(stderr, "Err: encodeFilePackInit failed.\n");
        return -1;
    }
    // 4 send file
    // (1) send head
    if (writen(sockfd, (char*)pack, this->getPACKET_HEAD_SIZE()) < 0) {
        close(filefd);
        return -1;
    }
    // (2) send file
    ssize_t recvlen;
    while ((recvlen = read(filefd, this->pack->data, this->getFILE_BLOCK_SIZE())) > 0) {
        if (writen(sockfd, this->pack->data, recvlen) < 0) {
            fprintf(stderr, "Err: send file packet failed.\n");
            close(filefd);
            return -1;
        }
        // bzero
        bzero((void*)(this->pack->data), this->getFILE_BLOCK_SIZE());
    }
    // clean
    close(filefd);
    return 0;
}

static int openFileToWrite(const char* filename, const int filename_len) {
    int fd;
    static int count = 0;
    if (judgeFileExisted(filename)) {
        // true : file existed
        size_t actual_len = (strlen(filename) + 10 + 1) * sizeof(char);   // 10是int最大数字2147483647的长度
        char tmp[actual_len];
        bzero((void*)tmp, actual_len);
        snprintf(tmp, actual_len, "%.*s%d", filename_len, filename, ++count);
        fd = openFileToWrite(tmp, filename_len);
    } else {
        // false : file not existed
        fd = open(filename, O_CREAT | O_WRONLY, 0600);
    }
    if (fd == -1) {
        return -1;
    }
    return fd;
}

int FilePackManage::recvFile(int sockfd) {
    int filefd;
    bzero((void*)(this->pack), this->packsize);
    ssize_t recvlen = readn(sockfd, (char*)(this->pack), this->getPACKET_HEAD_SIZE());
    if (recvlen < 0) {
        fprintf(stderr, "Err: recv file head error.\n");
        return -1;
    } else if (recvlen == 0) {
        printf("peer closed.\n");
        return -1;
    } else {
        if ((filefd = openFileToWrite(this->pack->filename, strlen(this->pack->filename))) == -1) {
            fprintf(stderr, "Err: open file error.\n");
            return -1;
        }
        for (int i = 0; i < (int)(this->pack->filesize / this->getFILE_BLOCK_SIZE()); i++) {
            recvlen = readn(sockfd, this->pack->data, this->getFILE_BLOCK_SIZE());
            if (recvlen < 0) {
                fprintf(stderr, "Err: recv file error.\n");
                close(filefd);
                return -1;
            } else if (recvlen == 0) {
                printf("peer closed.\n");
                close(filefd);
                return -1;
            } else {
                if (writen(filefd, this->pack->data, recvlen) < 0) {
                    fprintf(stderr, "write data to file error.\n");
                    close(filefd);
                    return -1;
                }
            }
        }
        recvlen = readn(sockfd, this->pack->data, this->pack->filesize % this->getFILE_BLOCK_SIZE());
        if (recvlen < 0) {
            fprintf(stderr, "Err: recv file error.\n");
            close(filefd);
            return -1;
        } else if (recvlen == 0) {
            printf("peer closed.\n");
            close(filefd);
            return -1;
        } else {
            if (writen(filefd, this->pack->data, recvlen) < 0) {
                fprintf(stderr, "write data to file error.\n");
                close(filefd);
                return -1;
            }
        }
    }
    // clean
    close(filefd);
    return 0;
}

int FilePackManage::sendDown(const int sockfd, const char* filename) {
    // 1 fill pack
    bzero((void*)(this->pack), this->packsize);
    this->pack->type = down;
    strncpy(this->pack->filename, filename, strlen(filename));
    // 2 send head
    if (writen(sockfd, (char*)pack, this->getPACKET_HEAD_SIZE()) < 0) {
        return -1;
    }
    // 3 recv file
    if (this->recvFile(sockfd)) {
        fprintf(stderr, "Err: recv downfile failed.\n");
        return -1;
    }
    return 0;
}

int FilePackManage::serverProcessDown(const int sockfd) {
    char filename_tmp[256] = {0};
    while (1) {
        bzero((void*)(this->pack), this->packsize);
        ssize_t recvlen = readn_b(sockfd, (char*)(this->pack), this->getPACKET_HEAD_SIZE());
        if (recvlen == -1) {
            fprintf(stderr, "recv dwon info failed.\n");
            return -1;
        } else if (recvlen == 0) {
            printf("client closed.\n");
            return 0;
        } else {
            strncpy(filename_tmp, this->pack->filename, strlen(this->pack->filename));
            if (this->sendFile(sockfd, filename_tmp)) {
                fprintf(stderr, "Err: send down file error.\n");
                return -1;
            }
        }
    }
    return 0;
}

