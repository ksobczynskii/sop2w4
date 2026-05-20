#include "../common.h"
#include <pthread.h>
#include <semaphore.h>
#define MAX_MSG 128
#define BACKLOG 3
#define STACK_SIZE 16
#define THREADS 4
#define DIVISION_NAMES_SIZE 128
#define MAPLEN 100



typedef struct {
    char names[DIVISION_NAMES_SIZE][MAX_MSG];
    int no_of_names;
    int map[MAPLEN][MAPLEN];
    pthread_mutex_t map_mtxes[MAPLEN];
    pthread_mutex_t mtx;

} map;
typedef struct {
    char stack[STACK_SIZE][MAX_MSG];
    int stack_size;
    pthread_mutex_t mtx;
    sem_t sem;
    map m;
} stack;

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

int message_operate(char* buf, int32_t* X, int32_t* Y, int32_t *P, char odzial[MAX_MSG]) {
    char* schowek;
    char korektor[] = " \n";
    schowek = strtok(buf, korektor);
    int x_set= 0, y_set = 0, p_set = 0, msg_set= 0;
    while (schowek != NULL) {

        if (x_set==0) {
            // printf("X got schowek = %s\n", schowek);
            *X = atoi(schowek);
            if (*X < 0 || *X > 99)
                break;
            // printf("HERE\n");
            x_set++;
            schowek = strtok(NULL, korektor);
            // printf("X changed schowek to: %s\n", schowek);
        }
        else if (y_set==0) {
            // printf("Y got schowek = %s\n", schowek);

            *Y = atoi(schowek);
            if (*Y < 0 || *Y > 99)
                break;
            y_set++;
            schowek = strtok(NULL, korektor);
        }
        else if (p_set==0) {
            // printf("P got schowek = %s\n", schowek);

            *P = atoi(schowek);
            if (*P != 0 && *P != 1)
                break;
            p_set++;
            schowek = strtok(NULL, korektor);
        }
        else if (msg_set == 0) {
            // printf("MSG got schowek = %s\n", schowek);

            strncpy(odzial, schowek, strlen(schowek));
            odzial[strlen(schowek)] = 0;
            msg_set++;
            schowek = strtok(NULL, korektor);
        }
        else {
            break;
        }

    }

    if (x_set == y_set == p_set == msg_set == 1)
        return 1;
    return -1;



}

void print_map(map* m) {
    if (pthread_mutex_lock(&m->mtx) < 0)
        ERR("lock mtx");
    for (int i=0; i<m->no_of_names;i++) {
        printf("[%d]: %s\n", i, m->names[i]);
    }
    if (pthread_mutex_unlock(&m->mtx) < 0)
        ERR("unlock");
    for (int i=0; i<MAPLEN; i++) {
        if (pthread_mutex_lock(&m->map_mtxes[i]) < 0)
            ERR("lock mtx");
        for (int j=0; j<MAPLEN; j++) {
            printf(" %d", m->map[i][j]);
        }
        if (pthread_mutex_unlock(&m->map_mtxes[i]) < 0)
            ERR("lock mtx");
        printf("\n");
    }
}
void update_pos(map* m, char oddzial[MAX_MSG],int32_t X, int32_t Y) {
    if (pthread_mutex_lock(&m->mtx) < 0)
        ERR("lock mtx");
    int pos = -1;
    int changed = -1;
    for (int i=0; i<m->no_of_names; i++) {
        if (strcmp(oddzial, m->names[i]) == 0) {
            pos = i;
            changed = 1;
            break;
        }
    }
    if (pos ==-1) {
        memcpy(m->names[m->no_of_names], oddzial, strlen(oddzial));
        m->no_of_names++;
        pos = m->no_of_names;

    }
    if (pthread_mutex_unlock(&m->mtx) < 0)
        ERR("unlock");

    if (changed == 1) {
       for (int i=0; i<MAPLEN; i++) {
           if (pthread_mutex_lock(&m->map_mtxes[i]) < 0)
               ERR("lock mtx");
           for (int j=0; j<MAPLEN; j++) {
               if (m->map[i][j] == pos) {
                   m->map[i][j] = -1;
               }
           }
           if (pthread_mutex_unlock(&m->map_mtxes[i]) < 0)
               ERR("lock mtx");
       }
    }

    if (pthread_mutex_lock(&m->map_mtxes[X]) < 0)
        ERR("lock mtx");
    m->map[X][Y] = pos;
    if (pthread_mutex_unlock(&m->map_mtxes[X]) < 0)
        ERR("lock mtx");

}
void* worker_work(void* args) {
    stack* s = args;
    char buf[MAX_MSG];
    ssize_t bytes;
    int32_t X, Y, P;
    char oddzial[MAX_MSG];
    while (1) {
        if (sem_wait(&s->sem) < 0)
            ERR("sem_wait");
        if (pthread_mutex_lock(&s->mtx) < 0)
            ERR("lock mtx");
        if (s->stack_size > 0) {
            printf("[%d]: Poppin from stack \n", gettid());
            printf("working with: %s\n", s->stack[s->stack_size-1]);
            memcpy(buf, s->stack[s->stack_size-1], MAX_MSG);
            memset(s->stack[s->stack_size-1],0,MAX_MSG);
            s->stack_size--;
            if (pthread_mutex_unlock(&s->mtx) < 0)
                ERR("unlock mutex");
            int res = message_operate(buf,&X,&Y,&P, oddzial);
            if (res == 1) {

                update_pos(&s->m, oddzial, X, Y);

                if (P==0) {
                    printf("Wrogi oddzial %s byl widziany na pozycji %d:%d\n", oddzial, X,Y);
                }
                else if (P==1)
                    printf("Nasz oddzial %s byl widziany na pozycji %d:%d\n", oddzial, X,Y);
                print_map(&s->m);
            }
            else {
                printf("Bledny format wiadomosci!\n");
            }

        }
        else {
            if (pthread_mutex_unlock(&s->mtx) < 0)
                ERR("unlock mutex");
        }

    }


    return NULL;
}
void do_server(int server_fd) {

    stack s;
    if (pthread_mutex_init(&s.mtx, 0) < 0)
        ERR("mutex_init");

    s.stack_size = 0;
    for (int i=0; i<STACK_SIZE; i++) {
        memset(s.stack[i], 0, MAX_MSG);
    }

    if (sem_init(&s.sem, 0, 0) < 0)
        ERR("sem init");


    s.m.no_of_names = 0;

    if (pthread_mutex_init(&s.m.mtx, 0) < 0)
        ERR("mutex_init");

    for (int i=0; i<MAPLEN; i++) {
        if (pthread_mutex_init(&s.m.map_mtxes[i], 0) < 0)
            ERR("mutex_init");
    }

    for (int i=0; i<MAPLEN; i++) {
        for (int j=0; j<MAPLEN; j++) {
            s.m.map[i][j] = -1;
        }
    }

    for (int i=0; i<DIVISION_NAMES_SIZE; i++) {
        memset(s.m.names[i],0,MAX_MSG);
    }

    pthread_t workers[THREADS] = {0};

    for (int i=0; i<THREADS; i++) {
        if (pthread_create(&(workers[i]), NULL, worker_work, &s) < 0)
            ERR("pthread create");
    }

    char buf[MAX_MSG];
    ssize_t bytes;
    // int32_t X, Y, P;
    // char oddzial[MAX_MSG];
    while (1) {
        if ((bytes = recvfrom(server_fd, buf, MAX_MSG, 0, NULL, 0)) < 0)
            ERR("recv from");
        if (bytes == 0)
            continue;
        if (pthread_mutex_lock(&s.mtx) < 0)
            ERR("lock");
        if (s.stack_size == 16)
        {
            if (pthread_mutex_unlock(&s.mtx) < 0)
                ERR("unlock mutex");
            continue;
        }
        // printf("Pushed to stack\n");
        buf[bytes-1] = 0;
        memcpy(s.stack[s.stack_size], buf, bytes);
        printf("[%s] Pushed to stack\n", s.stack[s.stack_size]);
        s.stack_size++;
        printf("Pushed to stack\n");
        if (sem_post(&s.sem) < 0)
            ERR("sem post");
        if (pthread_mutex_unlock(&s.mtx) < 0)
            ERR("unlock mutex");
    }
}

int main(int argc, const char** argv) {
    if (argc != 2)
        usage("wrong no. of arguments");

    int server_fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM);
    do_server(server_fd);

    exit(EXIT_SUCCESS);
}