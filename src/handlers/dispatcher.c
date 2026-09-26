#include "dispatcher.h"

#include <errno.h>
#include <sys/socket.h>

/* dispatcher thread handler --- consumer part */

void* dispatch_jobs(void* arg)
{
	int idx;

	sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Dispatcher Thread Started\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

	sem_post(&log_mutex);
	
	while (1)
	{
		sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Dispatcher Waiting For Jobs\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

		sem_post(&log_mutex);
		
		sem_wait(&full);

		
		sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Dispatcher Waiting For Workers\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

		sem_post(&log_mutex);


		sem_wait(&workers_available);
		sem_wait(&queue_mutex);

		sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Dequeued Job\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

		sem_post(&log_mutex);


		struct Job* job = dequeue(&job_queue);

		sem_post(&queue_mutex);
		sem_post(&empty);
		if (job == NULL)
		{
			sem_post(&workers_available);
			continue;
		}
		sem_wait(&worker_mutex);

		/* getting idle worker here to assign a job (evil capitalistic smile) */
		/* try to wait here for a while rather than just giving uppp */

		struct Worker* worker = findIdleWorker(&worker_pool);
		if (worker == NULL)
		{
			sem_post(&worker_mutex);
			sem_wait(&empty);
			sem_wait(&queue_mutex);
			enqueue(&job_queue, job);
			sem_post(&queue_mutex);
			sem_post(&full);
			continue;
		}

		int worker_fd = worker->fd;
		worker->status = WORKER_BUSY;
		sem_wait(&registry_mutex);
		job->status = JOB_IN_PROGRESS;
		job->worker_fd = worker_fd;
		sem_post(&registry_mutex);

		sem_post(&worker_mutex);

		char msg[1100];
		snprintf(msg, sizeof(msg), "JOB %d %s\n", job->job_id, job->payload);
		size_t sent = 0;
		size_t msg_length = strlen(msg);
		while (sent < msg_length)
		{
			ssize_t result = send(worker_fd, msg + sent, msg_length - sent, MSG_NOSIGNAL);
			if (result < 0 && errno == EINTR)
				continue;
			if (result <= 0)
			{
				shutdown(worker_fd, SHUT_RDWR);
				break;
			}
			sent += (size_t)result;
		}
		
		sem_wait(&log_mutex);

		idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"Job Sent To Worker\n");
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

		sem_post(&log_mutex);

	}
	
	/* simulate whatever at the worker's end for job resolution */

}