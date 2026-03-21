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

/* dispatcher thread handler --- consumer part */

void* dispatch_jobs(void* arg);

#endif