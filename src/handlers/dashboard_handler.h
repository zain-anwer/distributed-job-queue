#ifndef DASHBOARD_HANDLER_H
#define DASHBOARD_HANDLER_H

#include <ncurses.h>
#include <unistd.h>
#include "../../lib/client_pool.h"
#include "../../lib/worker_pool.h"
#include "../../lib/job_queue.h"
#include "../../util/sync.h"

void* dashboard(void* arg);

#endif