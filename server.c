#include "common.h"
#define BACKLOG 3
#define MAX_CONNS 5
#define MAX_DGRAM 577

typedef struct {
    int free;
    int32_t chunkIdx;
    struct sockaddr_in addr;
} conn;



int make_socket(int domain, int type) {
    int sock = socket(domain, type, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type) {
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_socket(AF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (type==SOCK_STREAM) {
        if (listen(socketfd, BACKLOG) < 0)
            ERR("listen");
    }
    return socketfd;
}

int getIdx(struct sockaddr_in addr, conn conns[MAX_CONNS]) {
    // printf("HERE\n");
    int empty = -1, pos = -1;
    for (int i=0; i<MAX_CONNS; i++) {
        // printf("conns[%d].free = %d\n", i, conns[i].free);
        if (conns[i].free)
            empty = i;
        else if (0 == memcmp(&addr, &(conns[i].addr), sizeof(struct sockaddr_in))) {
            pos = i;
            break;
        }
    }
    // printf("AFTER CHECKING: empty = %d, and pos = %d\n", empty, pos);

    if (pos==-1 && empty != -1) {
        conns[empty].addr = addr;
        conns[empty].free = 0;
        conns[empty].chunkIdx = 0;
        pos = empty;
    }
    return pos;
}

void message_operate(ssize_t bytes, char* buf, conn* c, struct sockaddr_in sender, int server) {
    // printf("Operating msg...\n");
    int32_t recv_chunk = ntohl(*(int32_t*)buf);
    // printf("RECV_CHUNK = %d\n", recv_chunk);
    if (recv_chunk != 1 + c->chunkIdx)
        return;
    printf("CHUNK: %s\n", buf+sizeof(int32_t));
    c->chunkIdx = c->chunkIdx + 1;
    int32_t toSend = htonl(recv_chunk);
    if (TEMP_FAILURE_RETRY(sendto(server,(char*)&toSend,sizeof(int32_t), 0, (struct sockaddr*)&sender, sizeof(sender))) < 0)
        ERR("send to");
}

void do_server(int port) {
    int server = bind_inet_socket(port, SOCK_DGRAM);
    struct sockaddr_in addr;
    conn conns[MAX_CONNS];

    for (int i=0; i<MAX_CONNS; i++) {
        memset(&conns[i].addr,0,sizeof(struct sockaddr));
        conns[i].chunkIdx = -1;
        conns[i].free = 1;
    }

    char buf[MAX_DGRAM];

    while (1) {
        memset(&addr, 0, sizeof(struct sockaddr_in));
        socklen_t size = sizeof(addr);
        ssize_t bytes;
        if ((bytes = TEMP_FAILURE_RETRY(recvfrom(server,buf,sizeof(buf),0,(struct sockaddr*)&addr, &size))) < 0)
            ERR("recvfrom");
        // printf("Read something! Whats should be chunk size is: %d\n", ntohl(*(int32_t*)buf));
        // buf[bytes] = 0;
        int idx = getIdx(addr, conns);
        // printf("IDX = %d\n", idx);
        if (idx >= 0) {
            if (bytes > 0)
                message_operate(bytes, buf, &conns[idx], addr, server);
            else if (bytes==0) {
                printf("FREEING...\n");
                conns[idx].free = 1;
                memset(&(conns[idx].addr),0,sizeof(struct sockaddr_in));
                conns[idx].chunkIdx = 0;
            }
        }

    }



}

int main(int argc, const char** argv) {
    if (argc != 2)
        usage("wrong no. of arguments");

    int port = atoi(argv[1]);
    // if (sethandler(SIG_IGN, SIGPIPE))
    //     ERR("Seting SIGPIPE:");

    do_server(port);

    exit(EXIT_SUCCESS);
}