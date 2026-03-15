#include "health_check_handler.h"

/* if a worker is unresponsive for 15 seconds mark it offline and remove job */

void* health_check(void* arg)
{
    sleep(5);

    while (true)
    {
        int i;
        
        sem_wait(&worker_mutex);
        
        for (i = 0 ; i < worker_pool.num_workers ; i++)
        {
            if (time(NULL) - (worker_pool.workers[i].last_heartbeat) > 15)
            {

                sem_wait(&registry_mutex);

                worker_pool.workers[i].status = WORKER_OFFLINE;
                struct Job* job = find_job_by_worker_fd(registry,worker_pool.workers[i].fd);   
                
                if (job != NULL)
                {
                    sem_wait(&queue_mutex);
                    job->status = JOB_PENDING;
                    enqueue(&job_queue,job); 
                    sem_post(&queue_mutex);
                
                }                
               
                sem_post(&registry_mutex);
            
            }
        }
        sem_post(&worker_mutex);
    }
}