#ifndef HEALTH_CHECK_HANDLER_H
#define HEALTH_CHECK_HANDLER_H

#include <unistd.h>
#include "../../lib/worker_pool.h"
#include "../../lib/job_queue.h"
#include "../../lib/log_queue.h"
#include "../../util/sync.h"

extern struct LogQueue* log_queue;

/* if a worker is unresponsive for 15 seconds mark it offline and remove job */

void* health_check(void* arg);

#endif