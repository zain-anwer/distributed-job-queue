#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

extern int curr_worker_id;
extern struct WorkerPool worker_pool;

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


struct Worker createWorker(int fd);

/* ------------------------------------------------------------------------------------------ */

/* ----------------------------- Worker Pool Struct & Functions ------------------------------*/


struct WorkerPool {
    struct Worker* workers;
    int num_workers;
};

void WorkerPool_init(struct WorkerPool* pool);

void WorkerPool_Add(struct WorkerPool* pool, struct Worker worker);

void WorkerPool_Remove(struct WorkerPool* pool, int worker_fd);

struct Worker* findIdleWorker(struct WorkerPool* pool);

struct Worker* findWorkerByFd(struct WorkerPool* pool,int fd);

#endif