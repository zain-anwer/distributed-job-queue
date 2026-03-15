#include "worker_handler.h"

void* worker_handler(void* arg)
{

    printf("Worker handler started\n");
    fflush(stdout);

    int fd = *((int*) arg);
    free(arg);

    int v1, v2, v3, v4, v5;
    sem_getvalue(&worker_mutex,     &v1);
    sem_getvalue(&registry_mutex,   &v2);
    sem_getvalue(&queue_mutex,      &v3);
    sem_getvalue(&workers_available,&v4);
    sem_getvalue(&full,             &v5);

    printf("worker_mutex=%d registry_mutex=%d queue_mutex=%d workers_available=%d full=%d\n",
            v1, v2, v3, v4, v5);
    fflush(stdout);

    sem_wait(&worker_mutex);
    
    printf("Adding Worker\n");
    fflush(stdout);

    WorkerPool_Add(&worker_pool,createWorker(fd));
    struct Worker* new_worker = findWorkerByFd(&worker_pool,fd);
    int worker_id = new_worker->worker_id;
    
    printf("Added Worker\n");
    fflush(stdout);

    sem_post(&worker_mutex);
    
    /* increase the number of workers available by 1 */

    sem_post(&workers_available);

    printf("Worker added\n");
    fflush(stdout);

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
            snprintf(notification, sizeof(notification), "JOB COMPLETED\nResult: %s\n", result);

            write(client_fd,notification,strlen(notification));

            /* logging the message to check on console as well */

            printf("== JOB DONE ==\n");
            printf("worker id: %d\n",worker_id);
            printf("job id: %d\n",job_id);
            printf("result: %s\n",result);

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
            snprintf(notification, sizeof(notification), "JOB FAILED\nResult: %s\n", result);

            write(client_fd,notification,strlen(notification));

            /* logging the message to check on console as well */

            printf("== JOB FAILED ==\n");
            printf("worker id: %d\n",worker_id);
            printf("job id: %d\n",job_id);
            printf("result: %s\n",result);

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
        printf("Registry data structure uninitialized\n");
        fflush(stdout);
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
    
            printf("== WORKER DOWN ==");
            printf("job with job id = %d readded to queue\n",registry[i]->job_id);
        }
    }

    sem_post(&registry_mutex);

    sem_wait(&workers_available);

    sem_wait(&worker_mutex);
    WorkerPool_Remove(&worker_pool,fd);
    sem_post(&worker_mutex);

    printf("WORKER REMOVED -> (worker_id : %d) (worker_fd : %d)\n",worker_id,fd);
    fflush(stdout);

    close(fd);
}