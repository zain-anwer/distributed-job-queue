/*

NOTES:

1. since the ncurses window and the normal I/O buffer will clash we will use logging queue
2. logging queue will be populated with the server messages and will be read from the dashboard
3. we don't use a file because disk access is slower than memory access

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "../lib/job_queue.h"
#include "../lib/client_pool.h"
#include "../lib/worker_pool.h"
#include "../lib/log_queue.h"

#include "handlers/health_check_handler.h"
#include "handlers/dashboard_handler.h"
#include "handlers/client_handler.h"
#include "handlers/worker_handler.h"
#include "handlers/dispatcher.h"

#include "../util/socket.h"
#include "../util/sync.h"

struct LogQueue* log_queue;

static int read_handshake(int fd, char handshake[7])
{
	size_t received = 0;
	while (received < 6) {
		ssize_t result = read(fd, handshake + received, 6 - received);
		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0)
			return -1;
		received += (size_t)result;
	}
	handshake[6] = '\0';
	return 0;
}

int main()
{
	/* initialization of synchronization locks for producer-consumer behaviour */

    sync_init(MAX_JOB_NUM);
	int idx;

	/* initializing log queue */

	log_queue = (struct LogQueue*) malloc (sizeof(struct LogQueue));
	if (log_queue == NULL)
	{
		fprintf(stderr, "Failed to allocate log queue\n");
		return EXIT_FAILURE;
	}
	log_queue->count = 0;
	log_queue->head = 0;

	/* creating server socket */

	int socket_fd = createTCPIpv4Socket();
	struct sockaddr* address = createTCPIpv4SocketAddress("",2000);
	if (socket_fd == -1 || address == NULL)
	{
		perror("server socket setup");
		if (socket_fd >= 0)
			close(socket_fd);
		free(address);
		free(log_queue);
		return EXIT_FAILURE;
	}
	int reuse_address = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));

	if (bind(socket_fd,address,sizeof(*address)) == 0)
	{
		sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Socket Bound Successfully\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

		sem_post(&log_mutex);
	}
	else
	{
		perror("bind");
		free(address);
		close(socket_fd);
		free(log_queue);
		return EXIT_FAILURE;
	}
	free(address);

	/* defining backlog (number of clients waiting to be serviced) as 10 for now */
	
	if (listen(socket_fd, 10) == -1)
	{
		perror("listen");
		close(socket_fd);
		free(log_queue);
		return EXIT_FAILURE;
	}

	pthread_t dispatcher_thread;
	pthread_t health_checker_thread;
	pthread_t dashboard_thread;

	/* these threads will continue to run in the background regardless of connections */

	pthread_create(&dashboard_thread,NULL,dashboard,NULL);
	pthread_detach(dashboard_thread);
	pthread_create(&dispatcher_thread,NULL,dispatch_jobs,NULL);
	pthread_detach(dispatcher_thread);
	pthread_create(&health_checker_thread,NULL,health_check,NULL);
	pthread_detach(health_checker_thread);

	/* differentiate between clients and workers connecting through handshake */

	char handshake[7];

	while (true)
	{
		struct sockaddr clientAddress;
		socklen_t clientAddressSize = sizeof(clientAddress);
		
		int* conn_fd = malloc(sizeof(*conn_fd));
		if (conn_fd == NULL)
			continue;
		*conn_fd = accept(socket_fd,&clientAddress,&clientAddressSize);
		if (*conn_fd == -1)
		{
			free(conn_fd);
			if (errno == EINTR)
				continue;
			perror("accept");
			continue;
		}

		pthread_t handler;

		if (read_handshake(*conn_fd, handshake) == -1)
		{
			close(*conn_fd);
			free(conn_fd);
			continue;
		}

		if (strncmp(handshake,"CLIENT",6) == 0)
		{
			if (pthread_create(&handler,NULL,client_handler,(void*)conn_fd) != 0)
			{
				close(*conn_fd);
				free(conn_fd);
				continue;
			}
			pthread_detach(handler);
		
			sem_wait(&log_mutex);

			idx = log_queue->head;

			snprintf(log_queue->log_messages[idx],1024,"CLIENT HANDSHAKE - CONNECTION ESTABLISHED\n");
			
			log_queue->head = (log_queue->head + 1) % 200;
			
			if (log_queue->count < 200)
				log_queue->count++;

			sem_post(&log_mutex);
		}

		else if (strncmp(handshake,"WORKER",6) == 0)
		{
			if (pthread_create(&handler,NULL,worker_handler,(void*)conn_fd) != 0)
			{
				close(*conn_fd);
				free(conn_fd);
				continue;
			}
			pthread_detach(handler);
			
			sem_wait(&log_mutex);

			idx = log_queue->head;

			snprintf(log_queue->log_messages[idx],1024,"WORKER HANDSHAKE - CONNECTION ESTABLISHED\n");
			
			log_queue->head = (log_queue->head + 1) % 200;
			
			if (log_queue->count < 200)
				log_queue->count++;

			sem_post(&log_mutex);
		}

		else 
		{
			sem_wait(&log_mutex);

			idx = log_queue->head;

			snprintf(log_queue->log_messages[idx],1024,"UNKNOWN HANDSHAKE - CONNECTION REFUSED\n");
			
			log_queue->head = (log_queue->head + 1) % 200;
			
			if (log_queue->count < 200)
				log_queue->count++;

			sem_post(&log_mutex);

			close(*conn_fd);
			free(conn_fd);
		}
		fflush(stdout);
	}
	shutdown(socket_fd,SHUT_RDWR);
	sem_destroy(&queue_mutex);
	sem_destroy(&empty);
	sem_destroy(&full);
	sem_destroy(&worker_mutex);
	sem_destroy(&workers_available);
	return 0;
}

