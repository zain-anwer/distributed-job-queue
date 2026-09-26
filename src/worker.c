#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../util/socket.h"

static int heartbeat_fd = -1;
static volatile sig_atomic_t heartbeat_stop = 0;
static int failure_rate_percent = 0;

static int read_line(int fd, char* line, size_t capacity)
{
    size_t length = 0;
    int too_long = 0;
    for (;;) {
        char ch;
        ssize_t result = read(fd, &ch, 1);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return 0;
        if (ch == '\n') {
            if (too_long)
                return -1;
            if (length > 0 && line[length - 1] == '\r')
                length--;
            line[length] = '\0';
            return 1;
        }
        if (!too_long) {
            if (length + 1 < capacity)
                line[length++] = ch;
            else
                too_long = 1;
        }
    }
}

static int write_all(int fd, const char* data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t result = write(fd, data + sent, length - sent);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return -1;
        sent += (size_t)result;
    }
    return 0;
}

static int send_all(int fd, const char* data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t result = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return -1;
        sent += (size_t)result;
    }
    return 0;
}

static void* heartbeat_handler(void* arg)
{
    (void)arg;
    while (!heartbeat_stop) {
        sleep(1);
        if (heartbeat_stop)
            break;
        static const char heartbeat[] = "HEARTBEAT\n";
        if (send_all(heartbeat_fd, heartbeat, sizeof(heartbeat) - 1) == -1)
            break;
    }
    return NULL;
}

static void execute_job(const char* payload, int job_id, int fd)
{
    char result[1100];
    bool job_failed = failure_rate_percent > 0 &&
                      rand() % 100 < failure_rate_percent;
    sleep(rand() % 4 + 1);
    if (job_failed)
        snprintf(result, sizeof(result), "FAILED %d Random Failure\n", job_id);
    else
        snprintf(result, sizeof(result), "DONE %d - %s\n", job_id, payload);
    send_all(fd, result, strlen(result));
}

int main(int argc, char** argv)
{
    const char* failure_rate = getenv("WORKER_FAILURE_RATE");
    if (failure_rate != NULL) {
        char* end = NULL;
        long parsed_rate = strtol(failure_rate, &end, 10);
        if (end != failure_rate && *end == '\0' && parsed_rate >= 0 && parsed_rate <= 100)
            failure_rate_percent = (int)parsed_rate;
        else
            fprintf(stderr, "Ignoring invalid WORKER_FAILURE_RATE (expected 0-100)\n");
    }

    char ip[20] = "127.0.0.1";
    int port_num = 2000;
    if (argc >= 3) {
        snprintf(ip, sizeof(ip), "%s", argv[1]);
        port_num = atoi(argv[2]);
    }

    int worker_fd = createTCPIpv4Socket();
    struct sockaddr* address = createTCPIpv4SocketAddress(ip, port_num);
    if (worker_fd == -1 || address == NULL ||
        connect(worker_fd, address, sizeof(*address)) == -1) {
        fprintf(stderr, "[WORKER] Failed to connect\n");
        if (worker_fd >= 0)
            close(worker_fd);
        free(address);
        return EXIT_FAILURE;
    }
    free(address);

    static const char handshake[] = "WORKER";
    if (send_all(worker_fd, handshake, sizeof(handshake) - 1) == -1) {
        close(worker_fd);
        return EXIT_FAILURE;
    }

    int job_pipe[2];
    if (pipe(job_pipe) == -1) {
        perror("pipe");
        close(worker_fd);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(job_pipe[0]);
        close(job_pipe[1]);
        close(worker_fd);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        close(job_pipe[1]);
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        char line[2048];
        int line_status;
        while ((line_status = read_line(job_pipe[0], line, sizeof(line))) != 0) {
            if (line_status == -1)
                continue;
            char payload[1024];
            int job_id;
            if (sscanf(line, "JOB_ID: %d PAYLOAD: %1023[^\r\n]", &job_id, payload) != 2)
                continue;
            execute_job(payload, job_id, worker_fd);
        }
        close(job_pipe[0]);
        close(worker_fd);
        _exit(EXIT_SUCCESS);
    }

    close(job_pipe[0]);
    srand((unsigned int)time(NULL));
    heartbeat_fd = worker_fd;
    heartbeat_stop = 0;
    pthread_t heartbeat_thread;
    if (pthread_create(&heartbeat_thread, NULL, heartbeat_handler, NULL) != 0) {
        close(job_pipe[1]);
        shutdown(worker_fd, SHUT_RDWR);
        close(worker_fd);
        waitpid(pid, NULL, 0);
        return EXIT_FAILURE;
    }

    char line[2048];
    int line_status;
    while ((line_status = read_line(worker_fd, line, sizeof(line))) != 0) {
        if (line_status == -1 || strncmp(line, "JOB ", 4) != 0)
            continue;
        int job_id;
        char payload[1024];
        if (sscanf(line + 4, "%d %1023[^\r\n]", &job_id, payload) != 2)
            continue;
        char pipe_message[1100];
        snprintf(pipe_message, sizeof(pipe_message), "JOB_ID: %d PAYLOAD: %s\n", job_id, payload);
        if (write_all(job_pipe[1], pipe_message, strlen(pipe_message)) == -1)
            break;
    }

    close(job_pipe[1]);
    heartbeat_stop = 1;
    shutdown(worker_fd, SHUT_RDWR);
    pthread_join(heartbeat_thread, NULL);
    close(worker_fd);
    waitpid(pid, NULL, 0);
    return EXIT_SUCCESS;
}