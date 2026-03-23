// clients submit jobs that get assigned to workers by the central server
// clients can give two commands (submit and status) to get an equivalent response
// client handler thread

#include "client_handler.h"

/* the following program will be the producer (populate the queue) */

void* client_handler(void* arg) {
    
    int fd = (*(int*)arg);
    free(arg);
    char buffer[BUFFER_SIZE];

    ClientPool_Add(&client_pool,createClient(fd,true));

    // as long as the connection isn't closed

    while(read(fd,buffer,sizeof(buffer)) > 0)     
    {
        if (strncmp(buffer,"SUBMIT ",7) == 0) {
           
            struct Job* new_job = malloc(sizeof(struct Job));
            Job_init(new_job);
            new_job->client_fd = fd;
            strncpy(new_job->payload, buffer + 7, sizeof(new_job->payload) - 1);

            sem_wait(&registry_mutex);
            registry[jobs_registered++] = new_job;
            sem_post(&registry_mutex);
            
            sem_wait(&empty);
            sem_wait(&queue_mutex);
            
            enqueue(&job_queue, new_job);
            
            sem_post(&queue_mutex);
            sem_post(&full);

            /* sending an acknowledgment */

            char ack[ACK_SIZE];
            snprintf(ack,ACK_SIZE,"ACK: JOB SUBMITTED - JOB ID: %d\n",new_job->job_id);
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

    sem_wait(&client_mutex);
    ClientPool_Remove(&client_pool,fd);
    sem_post(&client_mutex);

    sem_wait(&log_mutex);

		int idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"REMOVED CLIENT -> (client_fd: %d)\n",fd);
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

	sem_post(&log_mutex);

    fflush(stdout);
    pthread_exit(0);
}


