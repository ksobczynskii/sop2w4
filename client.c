#include "common.h"

#define FILEPATH "./pan_tadeusz.txt"
#define read_chunk 50
#define MAXATT 5

volatile sig_atomic_t last_signal=0;
typedef struct {
    int32_t chunkNo;
    char msg[read_chunk+1];
} message;
int make_socket()
{
    int sock;
    sock = socket(PF_INET, SOCK_DGRAM, 0);
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
    printf("OPENED FILE. WILL BE WRITING...\n");
    int chunk = 1;
    while (1) {
        if ((bytes = TEMP_FAILURE_RETRY(read(file,buf,read_chunk))) < 0)
            ERR("read");
        printf("CHUNK %d, READ %ld\n", chunk, bytes);
        if (bytes == 0)
            break;
        buf[bytes] = '\0';

        // printf("HERE\n");
        message m;
        m.chunkNo = htonl(chunk);
        memcpy(&m.msg, buf, bytes+1);
        ssize_t sent_bytes;
        int attempts = 0;
        while (attempts < MAXATT) {
            printf("ATTEMPT NO. %d\n", attempts);
            struct itimerval ts;
            printf("HERE\n");
            if ((sent_bytes = TEMP_FAILURE_RETRY(sendto(client, &m, sizeof(m),0, (struct sockaddr*)&dest, sizeof(dest)))) < 0)
                ERR("sendto");
            printf("SENT: %ld bytes\n", sent_bytes);
            // fflush(stdout);
            memset(&ts, 0, sizeof(struct itimerval));
            ts.it_value.tv_usec = 500000;
            setitimer(ITIMER_REAL, &ts, NULL);
            last_signal = 0;
            ssize_t read_bytes;
            while ((read_bytes = recv(client, recv_buf, read_chunk, 0)) < 0)
            {
                if (EINTR != errno)
                    ERR("recv:");
                if (SIGALRM == last_signal)
                    break;
            }

            printf("READ %d\n", read_bytes);
            if (read_bytes >= sizeof(int32_t))
            {
                int32_t received = ntohl(*(int32_t*)recv_buf);
                if (received == chunk)
                    break;
            }
            attempts++;
        }
        if (attempts >= MAXATT)
        {
            printf("Nobody is receiving my dgrams. Leaving...\n");
            break;
        }
        chunk++;
    }
    if (TEMP_FAILURE_RETRY(sendto(client,0,0,0, (struct sockaddr*)&dest, sizeof(dest))) < 0)
        ERR("sendto");

}

void set_flag(int sig) {
    last_signal = sig;
}
int main(int argc, const char** argv) {
    if (argc!=3)
        usage("NO of args");


    int client = make_socket();
    sethandler(set_flag, SIGALRM);
    // if (sethandler(SIG_IGN, SIGPIPE))
    //     ERR("Seting SIGPIPE:");

    do_client(client, make_address(argv[1], argv[2]));


}