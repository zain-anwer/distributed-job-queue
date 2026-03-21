#ifndef WORKER_HANDLER_H
#define WORKER_HANDLER_H

#include <unistd.h>
#include <string.h>
#include "../../lib/worker_pool.h"
#include "../../lib/job_queue.h"
#include "../../util/sync.h"


void* worker_handler(void* arg);

#endif