#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "../lib/job_queue.c"
#include "../lib/socket.h"
#include "../lib/client_pool.c"
#include "../lib/worker_pool.c"
#include "client.c"

extern sem_t queue_mutex;
extern sem_t empty;
extern sem_t full;
extern sem_t worker_mutex;
extern sem_t workers_available;

extern worker_pool;
extern job_queue;

void* dispatch_jobs(void* arg);

int main()
{
	/* initialization of synchronization locks for producer-consumer behaviour */

    sync_init(MAX_JOB_NUM);

	/* creating server socket */

	int socket_fd = createTCPIpv4Socket();
	struct sockaddr* address = createTCPIpv4SocketAddress("",2000);
	if (bind(socket_fd,address,sizeof(*address)) == 0)
		printf("Socket Bound Successfully\n");

	/* defining backlog (number of clients waiting to be serviced) as 10 for now */
	
	listen (socket_fd,10);

	pthread_t dispatcher_thread;
	pthread_create(&dispatcher_thread,NULL,dispatch_jobs,NULL);

	while (true)
	{
		struct sockaddr clientAddress;
		socklen_t clientAddressSize = sizeof(clientAddress);
		
		int client_socket_fd = accept(socket_fd,&clientAddress,&clientAddressSize);

        ClientPool_Add(&client_pool,createClient(client_socket_fd,true));

		pthread_t tid;

		int* fd = (int*) malloc (sizeof(int));
		*fd = client_socket_fd;
		pthread_create(&tid,NULL,client_handler,(void*)fd);
	}
	
	shutdown(socket_fd,SHUT_RDWR);
	sem_destroy(&queue_mutex);
	sem_destroy(&empty);
	sem_destroy(&full);
	
	return 0;
}

/* dispatcher thread handler --- consumer part */

void* dispatch_jobs(void* arg)
{
	sem_wait(&full);
	sem_wait(&queue_mutex);
	sem_wait(&workers_available);

	struct Job* job = dequeue(job_queue);
	
	sem_wait(&worker_mutex);

	/* getting idle worker here to assign a job (evil capitalistic smile) */
	/* try to wait here for a while rather than just giving uppp */

	struct Worker* worker = findIdleWorker(worker_pool);

	worker->status = WORKER_BUSY;
	job->status = JOB_IN_PROGRESS;
	job->worker_fd = worker->fd;

	sem_post(&queue_mutex);
	sem_post(&worker_mutex);

	char msg[1100];
	snprintf(msg, sizeof(msg), "JOB %d %s\n", job->job_id, job->payload);
    write(worker->fd, msg, strlen(msg));

	/* simulate whatever at the worker's end for job resolution */
}