#include "worker_pool.h"

#include <string.h>

int curr_worker_id = 1;
struct WorkerPool worker_pool = {.workers = NULL, .num_workers = 0};

/* ----------------------------- Worker Struct Functions -----------------------------------*/

struct Worker createWorker(int fd) {
    struct Worker worker = {fd,curr_worker_id++,WORKER_IDLE,0,0,time(NULL)};
    return worker;
}

/* ------------------------------------------------------------------------------------------ */

/* ----------------------------- Worker Pool Struct Functions ------------------------------*/

void WorkerPool_init(struct WorkerPool* pool) {
    pool->workers = NULL;
    pool->num_workers = 0;
}

void WorkerPool_Add(struct WorkerPool* pool, struct Worker worker) {
    struct Worker* workers = realloc(pool->workers,
                                     sizeof(*pool->workers) * (size_t)(pool->num_workers + 1));
    if (workers == NULL)
        return;

    pool->workers = workers;
    pool->workers[pool->num_workers] = worker;
    pool->num_workers++;
}

void WorkerPool_Remove(struct WorkerPool* pool, int worker_fd)
{
    for (int i = 0; i < pool->num_workers; i++)
    {
        if (pool->workers[i].fd != worker_fd)
            continue;

        if (i + 1 < pool->num_workers)
            memmove(&pool->workers[i], &pool->workers[i + 1],
                    sizeof(*pool->workers) * (size_t)(pool->num_workers - i - 1));
        pool->num_workers--;

        if (pool->num_workers == 0)
        {
            free(pool->workers);
            pool->workers = NULL;
        }
        return;
    }
}

struct Worker* findIdleWorker(struct WorkerPool* pool) {
    
    int i;
    for (i = 0; i < pool->num_workers ; i++)
    {
        if (pool->workers[i].status == WORKER_IDLE)
            return &pool->workers[i];
    }
    return NULL;
}

struct Worker* findWorkerByFd(struct WorkerPool* pool,int fd) {
    
    int i;
    for (i = 0; i < pool->num_workers ; i++)
    {
        if (pool->workers[i].fd == fd)
            return &pool->workers[i];
    }
    return NULL;
}
