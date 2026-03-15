#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "../../lib/job_queue.h"
#include "../../util/socket.h"
#include "../../lib/client_pool.h"
#include "../../lib/worker_pool.h"
#include "../../util/sync.h"

extern sem_t queue_mutex;
extern sem_t empty;
extern sem_t full;
extern sem_t worker_mutex;
extern sem_t workers_available;

extern struct WorkerPool worker_pool;
extern struct JobQueue job_queue;

/* dispatcher thread handler --- consumer part */

void* dispatch_jobs(void* arg);

#endif