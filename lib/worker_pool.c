#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

int curr_worker_id = 1;

/* ----------------------------- Worker Struct & Functions -----------------------------------*/

typedef enum {
    WORKER_IDLE,
    WORKER_BUSY,
    WORKER_OFFLINE
} WorkerStatus;

struct Worker {
    int          fd;
    int          worker_id;
    WorkerStatus status;
    int          jobs_completed;    // stat for dashboard
    int          jobs_failed;       // stat for dashboard
    time_t       last_heartbeat;    // stat for dashboard (computing health apparently)
};


struct Worker createWorker(int fd) {
    struct Worker worker = {fd,curr_worker_id++,WORKER_IDLE,0,0,time(NULL)};
}

/* ------------------------------------------------------------------------------------------ */

/* ----------------------------- Worker Pool Struct & Functions ------------------------------*/


struct WorkerPool {
    struct Worker* workers;
    int num_workers;
};

void WorkerPool_init(struct WorkerPool* pool) {
    pool->workers = NULL;
    pool->num_workers = 0;
}

void WorkerPool_Add(struct WorkerPool* pool, struct Worker worker) {
    if (pool->num_workers == 0)
    {
        pool->workers = (struct Worker*) malloc(sizeof(struct Worker));
        pool->workers[pool->num_workers] = worker;
        pool->num_workers += 1;
        return;
    }
    pool->num_workers += 1;
    pool->workers = realloc(pool->workers,sizeof(struct Worker)*(pool->num_workers));
    pool->workers[pool->num_workers-1] = worker;
}

void WorkerPool_Remove(struct WorkerPool* pool, int worker_fd)
{
    
    int i;
    struct WorkerPool* new_pool = malloc(sizeof(struct WorkerPool));
    WorkerPool_init(new_pool);

    for (i = 0;i < pool->num_workers;i++)
    {
        if (pool->workers[i].fd != worker_fd)
            WorkerPool_Add(new_pool,pool->workers[i]);
    }

    free(pool->workers);
    *pool = *new_pool;
    free(new_pool);
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

/* global worker pool instance that should be declared using extern globally in files that import it */

struct WorkerPool worker_pool;