#include "health_check_handler.h"

#include <errno.h>

static int wait_for_semaphore(sem_t* semaphore)
{
    int result;
    do {
        result = sem_wait(semaphore);
    } while (result == -1 && errno == EINTR);
    return result;
}

/* Mark stale workers offline and requeue any job they were processing. */
void* health_check(void* arg)
{
    (void)arg;

    while (true)
    {
        sleep(1);

        if (wait_for_semaphore(&worker_mutex) == -1)
            continue;

        for (int i = 0; i < worker_pool.num_workers; i++)
        {
            struct Worker* worker = &worker_pool.workers[i];
            if (worker->status == WORKER_OFFLINE ||
                time(NULL) - worker->last_heartbeat <= 15)
                continue;

            int worker_fd = worker->fd;
            if (worker->status == WORKER_IDLE)
                sem_trywait(&workers_available);
            worker->status = WORKER_OFFLINE;

            if (wait_for_semaphore(&registry_mutex) == -1)
                continue;

            for (int job_index = 0; job_index < MAX_JOB_NUM; job_index++)
            {
                struct Job* job = registry[job_index];
                if (job == NULL || job->worker_fd != worker_fd ||
                    job->status != JOB_IN_PROGRESS)
                    continue;

                if (wait_for_semaphore(&empty) == -1)
                    break;
                if (wait_for_semaphore(&queue_mutex) == -1)
                {
                    sem_post(&empty);
                    break;
                }

                job->status = JOB_PENDING;
                job->worker_fd = -1;
                enqueue(&job_queue, job);
                sem_post(&queue_mutex);
                sem_post(&full);
            }

            sem_post(&registry_mutex);
        }

        sem_post(&worker_mutex);
    }

    return NULL;
}