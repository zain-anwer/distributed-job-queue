#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "../lib/socket.h"

// thread handler declarations
void* heartbeat_handler(void* arg);
bool execute_job(char* payload, int job_id, int fd);
bool weighted_bit();

int main(int argc, char** argv)
{

    setvbuf(stdout, NULL, _IONBF, 0); // disable buffering

    srand(time(NULL));
    char ip[20] = "127.0.0.1";
    int port_num = 2000;

    if (argc >= 3) {
        strcpy(ip, argv[1]);
        port_num = atoi(argv[2]);
    }

    int worker_fd = createTCPIpv4Socket();
    struct sockaddr* address = createTCPIpv4SocketAddress(ip, port_num);

    if (connect(worker_fd, address, sizeof(*address)) == 0)
        printf("Connection Successful\n");
    
    // worker handshake
    write(worker_fd,"WORKER",6);

    /* heartbeat thread creation */
    pthread_t heartbeat_thread;
    int* fd = (int*) malloc(sizeof(int));
    *fd = worker_fd;
    pthread_create(&heartbeat_thread,NULL,heartbeat_handler,(void*)fd);

    /* main loop */

    char buffer[1100];
    while (read(worker_fd, buffer, sizeof(buffer)) > 0) {

        if (strncmp(buffer, "JOB ", 4) == 0) {
            int job_id;
            char payload[1024];
            sscanf(buffer + 4, "%d %[^\n]", &job_id, payload);
            execute_job(payload,job_id,worker_fd);
        }
        memset(buffer,0,sizeof(buffer));
    }

   
    // exits loop on disconnect
    printf("[WORKER] Broker disconnected\n");
    exit(1);
}

/* -------------------------------- FUNCTION DEFINITIONS --------------------------------------*/

bool weighted_bit()
{
    int num = rand() % 10;
    if (num == 0)
        return false;
    return true;
}

void* heartbeat_handler(void* arg)
{
    int fd = *((int*) arg);
    while(1){
        sleep(5);
        write(fd,"HEARTBEAT",9); 
    }
}

bool execute_job(char* payload, int job_id, int fd)
{
 
    char result[1024];
    bool job_failed = !weighted_bit();

    /* simulate variable execution */
    sleep(rand() % 4 + 1);
    
    if (job_failed)
    {
        char reason[] = "Random Failure";
        snprintf(result,sizeof(result),"FAILED %d %s\n",job_id,reason);
    }
    else
    {
        char result_value[] = "Random Value";
        snprintf(result,sizeof(result),"DONE %d %s\n",job_id,result_value);
    }
    write(fd,result,strlen(result));
    printf("Job result submitted\n");
    return job_failed;
}