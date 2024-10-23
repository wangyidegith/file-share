#ifndef FILE_PACKET_H
#define FILE_PACKET_H

#define FILE_BLOCK_SIZE_H 4096
#define MAX_FILENAME_LEN_H 255

typedef enum {
    ls,
    up,
    down
} Type;

typedef struct {
    Type type;
    size_t filesize;
    char filename[MAX_FILENAME_LEN_H + 1];

    char data[];
} FilePack;

class FilePackManage {
    private :
        const int PACKET_HEAD_SIZE;
        const int FILE_BLOCK_SIZE;
        size_t packsize = 0;
        FilePack* pack = NULL;
        
    public :
        FilePackManage(int PACKET_HEAD_SIZE, int FILE_BLOCK_SIZE);
        FilePack* getPackbuf() { return this->pack; }
        int getPACKET_HEAD_SIZE() { return PACKET_HEAD_SIZE; }
        int getFILE_BLOCK_SIZE() { return FILE_BLOCK_SIZE; }
        int sendDown(const int sockfd, const char* filename);
        int serverProcessDown(const int sockfd);
        int sendFile(int sockfd, const char* filepath);
        int recvFile(int sockfd);
        ~FilePackManage();
};

#endif

