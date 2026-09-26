#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include "../util/socket.h"

void* notif_listener(void* arg);

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

int main(int argc, char** argv)

{
    setvbuf(stdout, NULL, _IONBF, 0); // disable buffering

    char ip[20] = "127.0.0.1";
    int port_num = 2000;

    if (argc >= 3) {
        snprintf(ip, sizeof(ip), "%s", argv[1]);
        port_num = atoi(argv[2]);
    }

    int client_fd = createTCPIpv4Socket();
    struct sockaddr* address = createTCPIpv4SocketAddress(ip, port_num);

    if (client_fd >= 0 && address != NULL &&
        connect(client_fd, address, sizeof(*address)) == 0)
        printf("Connection Successful\n");
    else {
        fprintf(stderr, "[CLIENT] Failed to connect to broker\n");
        if (client_fd >= 0)
            close(client_fd);
        free(address);
        return EXIT_FAILURE;
    }

    free(address);

    pthread_t notif_thread;
    if (pthread_create(&notif_thread, NULL, notif_listener, &client_fd) != 0) {
        close(client_fd);
        return EXIT_FAILURE;
    }

    if (send_all(client_fd, "CLIENT", 6) == -1) {
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
        pthread_join(notif_thread, NULL);
        return EXIT_FAILURE;
    }

    char request[1024];
    char message[] = "STRESS TEST RANDOM JOB";

    for (int i = 0; i < 200; i++)
    {
        snprintf(request, sizeof(request), "SUBMIT %s\n", message);
        if (send_all(client_fd, request, strlen(request)) == -1)
            break;
        memset(request, 0, sizeof(request));
        usleep(500000);
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    pthread_join(notif_thread, NULL);
    return EXIT_SUCCESS;
}

void* notif_listener(void* arg)
{
    int fd = *((int*)arg);
    char input[1024];
    char line[2048];
    size_t line_length = 0;
    int line_too_long = 0;
    for (;;) {
        ssize_t bytes_read = read(fd, input, sizeof(input));
        if (bytes_read < 0 && errno == EINTR)
            continue;
        if (bytes_read <= 0)
            break;

        for (ssize_t i = 0; i < bytes_read; i++) {
            char ch = input[i];
            if (ch == '\n') {
                if (!line_too_long) {
                    if (line_length > 0 && line[line_length - 1] == '\r')
                        line_length--;
                    line[line_length] = '\0';
                    printf("NOTIFICATION: %s\n", line);
                }
                line_length = 0;
                line_too_long = 0;
            } else if (!line_too_long) {
                if (line_length + 1 < sizeof(line))
                    line[line_length++] = ch;
                else
                    line_too_long = 1;
            }
        }
    }
    return NULL;
}