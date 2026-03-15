#ifndef WORKER_HANDLER_H
#define WORKER_HANDLER_H

#include <unistd.h>
#include <string.h>
#include "../lib/worker_pool.h"
#include "../lib/job_queue.h"
#include "../util/sync.h"


extern sem_t registry_mutex;
extern sem_t worker_mutex;
extern sem_t workers_available;
extern struct Job* registry[MAX_JOB_NUM];
extern struct WorkerPool worker_pool;


void* worker_handler(void* arg);

#endif