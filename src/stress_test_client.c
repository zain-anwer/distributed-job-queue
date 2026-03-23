#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../util/socket.h"

void* notif_listener(void* arg);

int main(int argc, char** argv)

{
    setvbuf(stdout, NULL, _IONBF, 0); // disable buffering

    char ip[20] = "127.0.0.1";
    int port_num = 2000;

    if (argc >= 3) {
        strcpy(ip, argv[1]);
        port_num = atoi(argv[2]);
    }

    int client_fd = createTCPIpv4Socket();
    struct sockaddr* address = createTCPIpv4SocketAddress(ip, port_num);

    if (connect(client_fd, address, sizeof(*address)) == 0)
        printf("Connection Successful\n");
    else {
        fprintf(stderr, "[CLIENT] Failed to connect to broker\n");
        exit(1);
    }

    free(address);

    pthread_t notif_thread;
    pthread_create(&notif_thread,NULL,notif_listener,(void*)&client_fd);
    

    write(client_fd, "CLIENT", 6);

    char request[1024];
    char message[] = "STRESS TEST RANDOM JOB";

    while(1)
    {
        snprintf(request, sizeof(request), "SUBMIT %s\n", message);
        write(client_fd, request, strlen(request));
        memset(request, 0, sizeof(request));
        usleep(500000);
    }


    return 0;
}

void* notif_listener(void* arg)
{
    int fd = *((int*)arg);
    char notification[1024];

    while (read(fd, notification, sizeof(notification)) > 0) {
        
        printf("NOTIFICATION: %s", notification);
        fflush(stdout);
        memset(notification, 0, sizeof(notification));
    }

    // Broker disconnected
    printf("[CLIENT] Connection to broker lost\n");
    exit(1);
}