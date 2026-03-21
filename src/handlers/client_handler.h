// clients submit jobs that get assigned to workers by the central server
// clients can give two commands (submit and status) to get an equivalent response
// client handler thread

#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../../lib/job_queue.h"
#include "../../lib/client_pool.h"
#include "../../util/sync.h"

#define BUFFER_SIZE 1024
#define RESPONSE_SIZE 1024
#define ACK_SIZE 64

/* the following program will be the producer (populate the queue) */

void* client_handler(void* arg);

#endif