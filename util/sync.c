#include <semaphore.h>

sem_t queue_mutex;
sem_t registry_mutex;
sem_t empty;
sem_t full;
sem_t worker_mutex;
sem_t workers_available;

void sync_init(int queue_size)
{
	sem_init(&queue_mutex,0,1);
	sem_init(&registry_mutex,0,1);
	sem_init(&empty,0,queue_size);
	sem_init(&full,0,0);

	/* workers will be added upon handshake and removed upon failure or job assignment */
	sem_init(&workers_available,0,0);

	/* worker mutex to access and write to worker pool */
	sem_init(&worker_mutex,0,1);
}
