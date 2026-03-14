// clients submit jobs that get assigned to workers by the central server
// clients can give two commands (submit and status) to get an equivalent response
// client handler thread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/job_queue.c"
#include "../util/sync.c"

#define BUFFER_SIZE 1024
#define RESPONSE_SIZE 1024
#define ACK_SIZE 64

extern job_queue;
extern registry;
extern jobs_registered;


extern sem_t queue_mutex;
extern sem_t empty;
extern sem_t full;


/* the following program will be the producer (populate the queue) */

void* client_handler(void* arg) {
    
    int fd = (*(int*)arg);
    char buffer[BUFFER_SIZE];

    // as long as the connection isn't closed

    while(read(fd,buffer,sizeof(buffer)) > 0)     
    {
        if (strncmp(buffer,"SUBMIT ",7) == 0) {
           
            struct Job* new_job = malloc(sizeof(struct Job));
            Job_init(new_job);
            new_job->client_fd = fd;
            strncpy(new_job->payload, buffer + 7, sizeof(new_job->payload) - 1);
            registry[jobs_registered++] = new_job;
            
            sem_wait(&empty);
            sem_wait(&queue_mutex);
            
            enqueue(&job_queue, new_job);
            
            sem_post(&queue_mutex);
            sem_post(&full);

            /* sending an acknowledgment */

            char ack[ACK_SIZE];
            snprintf(ack,ACK_SIZE,"JOB SUBMITTED - JOB ID: %d\n",new_job->job_id);
            write(fd,ack,strlen(ack));
        }
        
        else if (strncmp(buffer,"STATUS ",7) == 0) {

            char response[RESPONSE_SIZE];
            
            int job_id = atoi(buffer + 7);
            struct Job* rel_job = find_job_by_id(registry,job_id);
            
            if (rel_job == NULL)
                snprintf(response,sizeof(response),"ERROR: JOB NOT FOUND\n");
            
            else {
                if (rel_job->status == JOB_PENDING)
                    snprintf(response,sizeof(response),"STATUS: JOB_PENDING\n");
                else if (rel_job->status == JOB_IN_PROGRESS)
                    snprintf(response,sizeof(response),"STATUS: JOB_IN_PROGRESS\n");
                else if (rel_job->status == JOB_COMPLETED)
                    snprintf(response,sizeof(response),"STATUS: JOB_COMPLETED\n");
                else if (rel_job->status == JOB_FAILED)
                    snprintf(response,sizeof(response),"STATUS: JOB_FAILED\n");
            }
            
            write(fd,response,strlen(response));

        }
        // flushing the buffer to prevent stale reads
        memset(buffer, 0, sizeof(buffer));
    }
}


