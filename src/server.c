#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "../lib/job_queue.h"
#include "../lib/client_pool.h"
#include "../lib/worker_pool.h"

#include "handlers/health_check_handler.h"
#include "handlers/dashboard_handler.h"
#include "handlers/client_handler.h"
#include "handlers/worker_handler.h"
#include "handlers/dispatcher.h"

#include "../util/socket.h"
#include "../util/sync.h"

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0); // disable buffering

	/* initialization of synchronization locks for producer-consumer behaviour */

    sync_init(MAX_JOB_NUM);

	/* creating server socket */

	int socket_fd = createTCPIpv4Socket();
	struct sockaddr* address = createTCPIpv4SocketAddress("",2000);
	if (bind(socket_fd,address,sizeof(*address)) == 0)
		printf("Socket Bound Successfully\n");

	/* defining backlog (number of clients waiting to be serviced) as 10 for now */
	
	listen(socket_fd,10);

	pthread_t dispatcher_thread;
	pthread_t health_checker_thread;
	pthread_t dashboard_thread;

	/* these threads will continue to run in the background regardless of connections */

	pthread_create(&dashboard_thread,NULL,dashboard,NULL);
	pthread_detach(&dashboard_thread);
	pthread_create(&dispatcher_thread,NULL,dispatch_jobs,NULL);
	pthread_detach(dispatcher_thread);
	pthread_create(&health_checker_thread,NULL,health_check,NULL);
	pthread_detach(health_checker_thread);

	/* differentiate between clients and workers connecting through handshake */

	char handshake[1024];

	while (true)
	{
		struct sockaddr clientAddress;
		socklen_t clientAddressSize = sizeof(clientAddress);
		
		int* conn_fd = (int*) malloc(sizeof(int));
		*conn_fd = accept(socket_fd,&clientAddress,&clientAddressSize);

		pthread_t handler;

		read(*conn_fd,handshake,sizeof(handshake));

		if (strncmp(handshake,"CLIENT",6) == 0)
		{
			pthread_create(&handler,NULL,client_handler,(void*)conn_fd);
			pthread_detach(handler);
			printf("CLIENT HANDSHAKE - CONNECTION ESTABLISHED\n");
		}

		else if (strncmp(handshake,"WORKER",6) == 0)
		{
			pthread_create(&handler,NULL,worker_handler,(void*)conn_fd);
			pthread_detach(handler);
			printf("WORKER HANDSHAKE - CONNECTION ESTABLISHED\n");
		}

		else 
		{
			printf("ERROR: UNKNOWN CONNECTION/HANDSHAKE\n");
			fflush(stdout);
			close(*conn_fd);
			free(conn_fd);
		}

		fflush(stdout);
		memset(handshake,0,sizeof(handshake));
		
	}
	shutdown(socket_fd,SHUT_RDWR);
	sem_destroy(&queue_mutex);
	sem_destroy(&empty);
	sem_destroy(&full);
	sem_destroy(&worker_mutex);
	sem_destroy(&workers_available);
	return 0;
}

