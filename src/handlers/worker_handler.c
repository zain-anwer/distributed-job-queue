#include "worker_handler.h"

void* worker_handler(void* arg)
{
    int idx;

    printf("Worker handler started\n");
    fflush(stdout);

    int fd = *((int*) arg);
    free(arg);

    sem_wait(&worker_mutex);
    
    sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Adding New Worker\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

	sem_post(&log_mutex);

    WorkerPool_Add(&worker_pool,createWorker(fd));
    struct Worker* new_worker = findWorkerByFd(&worker_pool,fd);
    int worker_id = new_worker->worker_id;

    sem_post(&worker_mutex);
    
    /* increase the number of workers available by 1 */

    sem_post(&workers_available);

    char buffer[1024];
 
    while (read(fd,buffer,sizeof(buffer)) > 0)
    {
        if (strncmp(buffer,"DONE ",5) == 0)
        {
            char result[1024];
            int job_id;
            int client_fd;
            
            sscanf(buffer + 5,"%d %[^\n]",&job_id,result);

            /* changing the job instance */

            sem_wait(&registry_mutex);

            struct Job* job = find_job_by_id(registry,job_id);
            job->status = JOB_COMPLETED;
            job->completed_at = time(NULL);
            strncpy(job->result,result,strlen(result));
            job->result[strlen(result)] = '\0';
            client_fd = job->client_fd;

            sem_post(&registry_mutex);

            /* notifying client (bourgoise!!) */

            char notification[2048];
            snprintf(notification, sizeof(notification), "JOB COMPLETED --- Result: %s\n", result);

            write(client_fd,notification,strlen(notification));

            /* logging the message to check on console as well */

            sem_wait(&log_mutex);

                idx = log_queue->head;

                snprintf(log_queue->log_messages[idx],1024,"-- JOB DONE -- \n");
                
                log_queue->head = (log_queue->head + 1) % 200;
                
                if (log_queue->count < 200)
                    log_queue->count++;

            sem_post(&log_mutex);

            /* update job status */

            sem_wait(&worker_mutex);

            new_worker = findWorkerByFd(&worker_pool,fd);
            new_worker->jobs_completed++;
            new_worker->status = WORKER_IDLE;

            sem_post(&worker_mutex);
        
            sem_post(&workers_available);
        }

        else if (strncmp(buffer,"FAILED ",7) == 0)
        {
            char result[1024];
            int job_id;
            int client_fd;
            
            sscanf(buffer + 7,"%d %[^\n]",&job_id,result);

            /* changing the job instance */

            sem_wait(&registry_mutex);

            struct Job* job = find_job_by_id(registry,job_id);
            job->status = JOB_FAILED;
            job->completed_at = time(NULL);
            strncpy(job->result,result,strlen(result));
            job->result[strlen(result)] = '\0';
            client_fd = job->client_fd;

            sem_post(&registry_mutex);

            /* notifying client (bourgoise!!) */

            char notification[2048];
            snprintf(notification, sizeof(notification), "JOB FAILED -- Result: %s\n", result);

            write(client_fd,notification,strlen(notification));

            /* logging the message to check on console as well */

            sem_wait(&log_mutex);

                idx = log_queue->head;

                snprintf(log_queue->log_messages[idx],1024,"-- JOB FAILED -- \n");
                
                log_queue->head = (log_queue->head + 1) % 200;
                
                if (log_queue->count < 200)
                    log_queue->count++;

            sem_post(&log_mutex);

            /* update job status */

            sem_wait(&worker_mutex);

            new_worker = findWorkerByFd(&worker_pool,fd);
            new_worker->jobs_failed++;
            new_worker->status = WORKER_IDLE;

            sem_post(&worker_mutex);
        
            sem_post(&workers_available);
        }

        else if (strncmp(buffer,"HEARTBEAT",9) == 0)
        {
            
            /* change the status if it has been marked offline for some god damn reason */

            sem_wait(&worker_mutex);        
            new_worker = findWorkerByFd(&worker_pool,fd);

            new_worker->last_heartbeat = time(NULL);

            if (new_worker->status == WORKER_OFFLINE)
               
                new_worker->status = WORKER_IDLE;    

            sem_post(&worker_mutex);
        }
        memset(buffer,0,sizeof(buffer));
    }

    if (registry == NULL)
    {
        
        sem_wait(&log_mutex);

            idx = log_queue->head;

            snprintf(log_queue->log_messages[idx],1024,"ERROR: Registry Uninitialized");
                
            log_queue->head = (log_queue->head + 1) % 200;
                
            if (log_queue->count < 200)
                log_queue->count++;

        sem_post(&log_mutex);

        return;
    }

    sem_wait(&registry_mutex);

    int i;
    
    for (i = 0 ; i < MAX_JOB_NUM ; i++)
    {
        if (registry[i] != NULL && registry[i]->worker_fd == fd && registry[i]->status == JOB_IN_PROGRESS)
        {
            sem_wait(&queue_mutex);
         
            registry[i]->status = JOB_PENDING;
            enqueue(&job_queue,registry[i]);
         
            sem_post(&queue_mutex);
            sem_post(&full); 

            sem_wait(&log_mutex);

            idx = log_queue->head;

            snprintf(log_queue->log_messages[idx],1024,"-- WORKER DOWN -- Job with Job ID: %d requeued\n",registry[i]->job_id);
                
            log_queue->head = (log_queue->head + 1) % 200;
                
            if (log_queue->count < 200)
                log_queue->count++;

            sem_post(&log_mutex);
        }
    }

    sem_post(&registry_mutex);

    sem_wait(&workers_available);

    sem_wait(&worker_mutex);
    WorkerPool_Remove(&worker_pool,fd);
    sem_post(&worker_mutex);


    sem_wait(&log_mutex);

        idx = log_queue->head;

        snprintf(log_queue->log_messages[idx],1024,"WORKER REMOVED -> (worker_id : %d) (worker_fd : %d)\n",worker_id,fd);
                
        log_queue->head = (log_queue->head + 1) % 200;
                
        if (log_queue->count < 200)
            log_queue->count++;

    sem_post(&log_mutex);

    close(fd);
}