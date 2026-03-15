#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../util/socket.h"

pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
int known_jobs[100];
int num_jobs = 0;

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

    write(client_fd, "CLIENT", 6);

    pthread_t notif_handler;
    pthread_create(&notif_handler, NULL, notif_listener, (void*)&client_fd);

    char message[1000];
    char request[1024];
    char choice[5];

    while (1) {
        printf("== OPTIONS ==\n");
        printf("1. Submit Job\n");
        printf("2. Get Job Status\n");

        fgets(choice, sizeof(choice), stdin);
        choice[strcspn(choice, "\n")] = 0;    // strip newline

        if (atoi(choice) != 1 && atoi(choice) != 2) {
            printf("Invalid Choice!\n");
            continue;
        }

        if (atoi(choice) == 1) {
            printf("Enter job payload: ");
            fgets(message, sizeof(message), stdin);
            message[strcspn(message, "\n")] = 0;    // strip newline
            snprintf(request, sizeof(request), "SUBMIT %s\n", message);
        }
        else {
            char job_id[10];

            printf("[CLIENT] Your job IDs so far: ");
            pthread_mutex_lock(&job_mutex);
            for (int i = 0; i < num_jobs; i++)
                printf("%d ", known_jobs[i]);
            pthread_mutex_unlock(&job_mutex);
            printf("\n");

            printf("Enter job ID: ");
            fgets(job_id, sizeof(job_id), stdin);
            job_id[strcspn(job_id, "\n")] = 0;      // strip newline

            int id_found = 0;
            pthread_mutex_lock(&job_mutex);
            for (int i = 0; i < num_jobs; i++)
                if (known_jobs[i] == atoi(job_id))
                    id_found = 1;
            pthread_mutex_unlock(&job_mutex);

            if (!id_found) {
                printf("Invalid Entry\n");
                continue;                            // no need to memset, request unused
            }

            snprintf(request, sizeof(request), "STATUS %s\n", job_id);
        }

        write(client_fd, request, strlen(request));
        memset(request, 0, sizeof(request));
        memset(message, 0, sizeof(message));
    }

    return 0;
}

void* notif_listener(void* arg)
{
    int fd = *((int*)arg);
    char notification[1024];

    while (read(fd, notification, sizeof(notification)) > 0) {
        int job_id;
        printf("NOTIFICATION: %s", notification);

        if (strncmp(notification, "ACK", 3) == 0) {
            sscanf(notification, "ACK\nJOB SUBMITTED - JOB ID: %d\n", &job_id);
            pthread_mutex_lock(&job_mutex);
            known_jobs[num_jobs++] = job_id;
            pthread_mutex_unlock(&job_mutex);

            printf("[CLIENT] Your job IDs so far: ");
            pthread_mutex_lock(&job_mutex);
            for (int i = 0; i < num_jobs; i++)
                printf("%d ", known_jobs[i]);
            pthread_mutex_unlock(&job_mutex);
            printf("\n");
        }
        fflush(stdout);
        memset(notification, 0, sizeof(notification));
    }

    // Broker disconnected
    printf("[CLIENT] Connection to broker lost\n");
    exit(1);
}