#include "dispatcher.h"

/* dispatcher thread handler --- consumer part */

void* dispatch_jobs(void* arg)
{
	printf("Dispatcher thread started\n");
    fflush(stdout);
	while (1)
	{
		printf("Dispatcher waiting for jobs\n");
		fflush(stdout);
		sem_wait(&full);

		printf("Dispatcher waiting for workers\n");
		fflush(stdout);
		sem_wait(&workers_available);
		sem_wait(&queue_mutex);

		printf("Dequeued Job\n");
		fflush(stdout);
		struct Job* job = dequeue(&job_queue);

		sem_post(&queue_mutex);
		sem_post(&empty);
		sem_wait(&worker_mutex);

		/* getting idle worker here to assign a job (evil capitalistic smile) */
		/* try to wait here for a while rather than just giving uppp */

		struct Worker* worker = findIdleWorker(&worker_pool);

		worker->status = WORKER_BUSY;
		job->status = JOB_IN_PROGRESS;
		job->worker_fd = worker->fd;

		sem_post(&worker_mutex);
		sem_post(&empty);

		char msg[1100];
		snprintf(msg, sizeof(msg), "JOB %d %s\n", job->job_id, job->payload);
		write(worker->fd, msg, strlen(msg));
		printf("Job sent to worker\n");

	}
	printf("Dispatcher Thread Ended\n");
	fflush(stdout);
	/* simulate whatever at the worker's end for job resolution */
}