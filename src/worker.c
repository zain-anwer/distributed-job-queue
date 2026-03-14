#include <string.h>
#include "../lib/worker_pool.c"
#include "../lib/job_queue.c"
#include "../util/sync.c"

extern sem_t registry_mutex;
extern sem_t worker_mutex;
extern sem_t workers_available;
extern struct Job* registry[MAX_JOB_NUM];
extern struct WorkerPool worker_pool;

void* worker_handler(void* arg)
{
    int fd = *((int*) arg);
    struct Worker new_worker = createWorker(fd);

    sem_wait(&worker_mutex);
    WorkerPool_Add(&worker_pool,new_worker);
    sem_post(&worker_mutex);
    
    /* increase the number of workers available by 1 */

    sem_post(&workers_available);

    char buffer[1024];
 
    while (read(fd,buffer,sizeof(buffer)) == 0)
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
            strncpy(notification,"JOB COMPLETED\n",14);
            strncpy(notification + 14,"Result: ",8);
            strncpy(notification + 14 + 8,result,strlen(result));

            write(client_fd,notification,strlen(notification));

            /* logging the message to check on console as well */

            printf("== JOB DONE ==\n");
            printf("worker id: %d",new_worker.worker_id);
            printf("job id: %d",job_id);
            printf("result: %s",result);

            /* update job status */

            sem_wait(&worker_mutex);

            new_worker.jobs_completed++;
            new_worker.status = WORKER_IDLE;

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
            strncpy(notification,"JOB FAILED\n",11);
            strncpy(notification + 11,"Reason: ",8);
            strncpy(notification + 11 + 8,result,strlen(result));

            write(client_fd,notification,strlen(notification));

            /* logging the message to check on console as well */

            printf("== JOB DONE ==\n");
            printf("worker id: %d",new_worker.worker_id);
            printf("job id: %d",job_id);
            printf("result: %s",result);

            /* update job status */

            sem_wait(&worker_mutex);

            new_worker.jobs_failed++;
            new_worker.status = WORKER_IDLE;

            sem_post(&worker_mutex);
        
            sem_post(&workers_available);
        }

        else if (strncmp(buffer,"HEARTBEAT",9) == 0)
        {
            new_worker.last_heartbeat = time(NULL);
         
            /* change the status if it has been marked offline for some god damn reason */

            sem_wait(&worker_mutex);

            if (new_worker.status == WORKER_OFFLINE)
                new_worker.status = WORKER_IDLE;    

            sem_post(&worker_mutex);
        }
        memset(buffer,0,sizeof(buffer));
    }

    sem_wait(&registry_mutex);

    int i;
    
    for (i = 0 ; i < MAX_JOB_NUM ; i++)
    {
        if (registry[i] && registry[i]->worker_fd == fd && registry[i]->status == JOB_IN_PROGRESS)
        {
            sem_wait(&queue_mutex);
         
            registry[i]->status = JOB_PENDING;
            enqueue(&job_queue,registry[i]);
         
            sem_post(&queue_mutex);
    
            printf("== WORKER DOWN ==");
            printf("job with job id = %d readded to queue\n",registry);
        }
    }

    sem_post(&registry_mutex);

    close(fd);
}