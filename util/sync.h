#ifndef SYNC_H
#define SYNC_H

#include <semaphore.h>

extern sem_t queue_mutex;
extern sem_t registry_mutex;
extern sem_t empty;
extern sem_t full;
extern sem_t worker_mutex;
extern sem_t workers_available;
extern sem_t client_mutex;


void sync_init(int queue_size);

#endif