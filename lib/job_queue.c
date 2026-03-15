#include "job_queue.h"

#define MAX_JOB_NUM 200

int job_counter = 1;
struct JobQueue job_queue = { .front = 0, .rear = 0, .numJobs = 0 };
struct Job* registry[MAX_JOB_NUM] = {NULL};
int jobs_registered = 0;

/* ------------------------------ JOB STRUCT & FUNCTIONS -------------------------------------*/

void Job_init(struct Job* job)
{
    job->job_id = job_counter++;
    job->status = JOB_PENDING;
    job->completed_at = 0;
    job->submitted_at = time(NULL);
    job->client_fd = -1;
    job->worker_fd = -1;
}

/* ------------------------------- JOB QUEUE & FUNCTIONS -------------------------------------*/

void enqueue(struct JobQueue* Q, struct Job* job)
{
	if (Q->numJobs == MAX_JOB_NUM)
	{
		printf("ERROR:MESSAGE_QUEUE_OVERFLOW\n");
		return;
	}
	Q->jobs[Q->rear] = job;
	Q->rear = (Q->rear + 1) % MAX_JOB_NUM;
	Q->numJobs++;
}

struct Job* dequeue(struct JobQueue* Q)
{
	if (Q->numJobs == 0)
	{
		Q->front = 0;
		Q->rear = 0;
		fprintf(stderr,"ERROR:QUEUE_EMPTY\n");
        return NULL;
	}
	Q->numJobs--;
	int index = Q->front;
	Q->front = (Q->front + 1) % MAX_JOB_NUM;
	return Q->jobs[index];
}

struct Job* find_job_by_id(struct Job** registry, int job_id)
{
	int i;
	for (i = 0 ; i < MAX_JOB_NUM ; i++)
		if (registry[i] != NULL && registry[i]->job_id == job_id)
			return registry[i];
	return NULL;
}

struct Job* find_job_by_worker_fd(struct Job** registry, int worker_fd)
{
	int i;
	for (i = 0 ; i < MAX_JOB_NUM ; i++)
		if (registry[i] != NULL && registry[i]->worker_fd == worker_fd)
			return registry[i];
	return NULL;
}