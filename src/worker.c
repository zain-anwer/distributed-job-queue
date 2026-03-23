#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../util/socket.h"

void* heartbeat_handler(void* arg);
void execute_job(char* payload, int job_id, int fd);
bool weighted_bit();

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* parse arguments before anything else */
    char ip[20] = "127.0.0.1";
    int port_num = 2000;
    if (argc >= 3) {
        strcpy(ip, argv[1]);
        port_num = atoi(argv[2]);
    }

    /* connect before fork so child inherits live socket */
    int worker_fd = createTCPIpv4Socket();
    struct sockaddr* address = createTCPIpv4SocketAddress(ip, port_num);
    if (connect(worker_fd, address, sizeof(*address)) == 0)
        printf("Connection Successful\n");
    else {
        fprintf(stderr, "[WORKER] Failed to connect\n");
        exit(1);
    }

    /* pipe before fork so both processes have it */
    int job_pipe[2];
    if (pipe(job_pipe) == -1) {
        perror("PIPE FAILURE");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    /* PARENT — handles broker communication */
    if (pid > 0) {
        close(job_pipe[0]);     // parent only writes to job pipe
        srand(time(NULL));

        write(worker_fd, "WORKER", 6);

        int* fd_ptr = malloc(sizeof(int));
        *fd_ptr = worker_fd;
        pthread_t heartbeat_thread;
        pthread_create(&heartbeat_thread, NULL, heartbeat_handler, fd_ptr);

        char buffer[1100];
        while (read(worker_fd, buffer, sizeof(buffer)) > 0) {
            if (strncmp(buffer, "JOB ", 4) == 0) {
                int job_id;
                char payload[1024];
                sscanf(buffer + 4, "%d %[^\n]", &job_id, payload);

                char pipe_msg[1100];
                snprintf(pipe_msg, sizeof(pipe_msg),
                         "JOB_ID: %d PAYLOAD: %s\n", job_id, payload);
                write(job_pipe[1], pipe_msg, strlen(pipe_msg));
                printf("[WORKER] Job %d dispatched to child\n", job_id);
            }
            memset(buffer, 0, sizeof(buffer));
        }

        close(job_pipe[1]);     // signals EOF to child
        wait(NULL);             // reap child cleanly
        printf("[WORKER] Broker disconnected\n");
        exit(1);
    }

    /* CHILD — handles job execution */
    else if (pid == 0) {
        close(job_pipe[1]);     // child only reads from job pipe
        srand(time(NULL) ^ getpid());

        char buffer[1100];
        char payload[1024];
        int job_id;

        while (read(job_pipe[0], buffer, sizeof(buffer)) > 0) {
            sscanf(buffer, "JOB_ID: %d PAYLOAD: %[^\n]\n", &job_id, payload);
            execute_job(payload, job_id, worker_fd);  // worker_fd inherited from fork
            memset(buffer, 0, sizeof(buffer));
            memset(payload, 0, sizeof(payload));
        }

        close(job_pipe[0]);
        exit(0);
    }

    return 0;
}

bool weighted_bit() {
    return (rand() % 10) != 0;
}

void* heartbeat_handler(void* arg) {
    int fd = *((int*)arg);
    while (1) {
        sleep(5);
        write(fd, "HEARTBEAT", 9);
    }
    return NULL;
}

void execute_job(char* payload, int job_id, int fd) {
    char result[1100];
    bool job_failed = !weighted_bit();

    sleep(rand() % 4 + 1);

    if (job_failed)
        snprintf(result, sizeof(result), "FAILED %d Random Failure\n", job_id);
    else
        snprintf(result, sizeof(result), "DONE %d - %s\n", job_id, payload);

    printf("Job result formulated\n");
    write(fd, result, strlen(result));
}