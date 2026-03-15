#ifndef HEALTH_CHECK_HANDLER_H
#define HEALTH_CHECK_HANDLER_H

#include <unistd.h>
#include "../lib/worker_pool.h"
#include "../lib/job_queue.h"
#include "sync.h"

extern struct JobQueue job_queue;
extern struct Job* registry[MAX_JOB_NUM];
extern sem_t worker_mutex;
extern sem_t workers_available;


extern struct WorkerPool worker_pool;
extern struct Job* registry[MAX_JOB_NUM];
extern struct JobQueue job_queue;

/* if a worker is unresponsive for 15 seconds mark it offline and remove job */

void* health_check(void* arg);

#endif