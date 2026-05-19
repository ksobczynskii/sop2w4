#include "common.h"

#define FILEPATH "./pan_tadeusz"
#define read_chunk 50
#define MAXATT 5

volatile sig_atomic_t last_signal=0;
typedef struct {
    int32_t chunkNo;
    char* msg;
} message;
int make_socket()
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

void do_client(int client, struct sockaddr_in dest) {
    int file = open(FILEPATH, O_RDONLY);
    if (file < 0)
        ERR("open");
    char buf[read_chunk+1];
    char recv_buf[read_chunk+1];
    ssize_t bytes;
    int chunk = 1;
    while (1) {
        if ((bytes = TEMP_FAILURE_RETRY(read(file,buf,read_chunk))) < 0)
            ERR("read");
        if (bytes == 0)
            break;
        message m;
        m.chunkNo = chunk;
        m.msg = buf;

        int attempts = 0;
        while (attempts < MAXATT) {
            struct itimerval ts;
            if (TEMP_FAILURE_RETRY(sendto(client, (char*)&m, sizeof(m),0, (struct sockaddr*)&dest, sizeof(dest))) < 0)
                ERR("sendto");
            memset(&ts, 0, sizeof(struct itimerval));
            ts.it_value.tv_usec = 500000;
            setitimer(ITIMER_REAL, &ts, NULL);
            last_signal = 0;
            while (recv(client, recv_buf, size, 0) < 0)
            {
                if (EINTR != errno)
                    ERR("recv:");
                if (SIGALRM == last_signal)
                    break;
            }
        }
        chunk++;



    }
}

void set_flag(int sig) {
    last_signal = sig;
}
int main(int argc, const char** argv) {
    if (argc!=3)
        usage("NO of args");


    int client = make_socket();
    sethandler(set_flag, SIGALRM);

    do_client(client, make_address(argv[1], argv[2]));


}