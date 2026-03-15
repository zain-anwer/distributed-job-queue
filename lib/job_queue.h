#ifndef JOB_QUEUE_H 
#define JOB_QUEUE_H

#include <stdio.h>
#include <time.h>
#include <string.h>

#define MAX_JOB_NUM 200

extern int job_counter;
extern struct JobQueue job_queue;
extern struct Job* registry[MAX_JOB_NUM];
extern int jobs_registered;

/*---------------------------- JOB STRUCT & FUNCTIONS -----------------------------------------*/

typedef enum {
    JOB_PENDING,
    JOB_IN_PROGRESS,
    JOB_COMPLETED,
    JOB_FAILED
} JobStatus;

struct Job {
    int job_id;             // to identify jobs
    JobStatus status;       // to simulate and track worker thread activity
    char payload[1024];     // job message
    char result[1024];      // result filled in upon completion
    int worker_fd;          // identify the assigned worker
    int client_fd;          // identify client for sending the result
    time_t submitted_at;    // timestamp
    time_t completed_at; 
};

void Job_init(struct Job* job);

/* ------------------------------- JOB QUEUE & FUNCTIONS -------------------------------------*/


struct JobQueue
{
	struct Job* jobs[MAX_JOB_NUM];
	int front;
	int rear;
	int numJobs;
};

void enqueue(struct JobQueue* Q, struct Job* job);

struct Job* dequeue(struct JobQueue* Q);

struct Job* find_job_by_id(struct Job** registry, int job_id);

struct Job* find_job_by_worker_fd(struct Job** registry, int worker_fd);

/* ---------------------------------- JOB QUEUE VARIABLES ------------------------------------ */


#endif