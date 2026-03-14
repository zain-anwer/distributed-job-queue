#include "../lib/worker_pool.c"
#include "../lib/job_queue.c"
#include "sync.c"

extern struct JobQueue job_queue;
extern struct Job* registry[MAX_JOB_NUM];
extern sem_t worker_mutex;
extern sem_t workers_available;


extern struct WorkerPool worker_pool;
extern struct Job* registry[MAX_JOB_NUM];
extern struct JobQueue job_queue;


/* if a worker is unresponsive for 15 seconds mark it offline and remove job */

void* health_check(void* arg)
{
    Sleep(5);

    while (true)
    {
        int i;
        
        sem_wait(&worker_mutex);
        
        for (i = 0 ; i < worker_pool.num_workers ; i++)
        {
            if (time(NULL) - (worker_pool.workers[i].last_heartbeat) > 15)
            {
                sem_wait(&queue_mutex);
                sem_wait(&registry_mutex);

                worker_pool.workers[i].status = WORKER_OFFLINE;
                struct Job* job = find_job_by_worker_fd(registry,worker_pool.workers[i].fd);               
                job->status = JOB_PENDING;
                enqueue(&job_queue,job);
            
                sem_post(&registry_mutex);
                sem_post(&queue_mutex);
            
            }

            sem_post(&worker_mutex);
        }
    }
}