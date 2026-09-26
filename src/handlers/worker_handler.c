#include "worker_handler.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define WORKER_LINE_SIZE 2048

static int wait_for_semaphore(sem_t* semaphore)
{
    int result;
    do {
        result = sem_wait(semaphore);
    } while (result == -1 && errno == EINTR);
    return result;
}

static void log_worker_event(const char* format, ...)
{
    if (log_queue == NULL || wait_for_semaphore(&log_mutex) == -1)
        return;

    int idx = log_queue->head;
    va_list args;
    va_start(args, format);
    vsnprintf(log_queue->log_messages[idx], sizeof(log_queue->log_messages[idx]), format, args);
    va_end(args);

    log_queue->head = (idx + 1) % 200;
    if (log_queue->count < 200)
        log_queue->count++;
    sem_post(&log_mutex);
}

static void notify_client(int fd, const char* message)
{
    size_t sent = 0;
    size_t length = strlen(message);
    while (sent < length) {
        ssize_t result = send(fd, message + sent, length - sent, MSG_NOSIGNAL);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            break;
        sent += (size_t)result;
    }
}

static void handle_completion(int fd, const char* line, int failed)
{
    const char* payload = line + (failed ? 7 : 5);
    char result[1024];
    int job_id;
    int client_fd = -1;

    if (sscanf(payload, "%d %1023[^\r\n]", &job_id, result) != 2) {
        log_worker_event("Invalid worker completion message\n");
        return;
    }

    if (wait_for_semaphore(&registry_mutex) == -1)
        return;
    struct Job* job = find_job_by_id(registry, job_id);
    if (job == NULL || job->status != JOB_IN_PROGRESS || job->worker_fd != fd) {
        sem_post(&registry_mutex);
        log_worker_event("Ignoring completion for unknown/unassigned job %d\n", job_id);
        return;
    }

    job->status = failed ? JOB_FAILED : JOB_COMPLETED;
    job->completed_at = time(NULL);
    snprintf(job->result, sizeof(job->result), "%s", result);
    client_fd = job->client_fd;

    char notification[1100];
    snprintf(notification, sizeof(notification), "JOB %s --- Result: %s\n",
             failed ? "FAILED" : "COMPLETED", result);
    int client_locked = client_fd >= 0 && wait_for_semaphore(&client_mutex) == 0;
    sem_post(&registry_mutex);
    if (client_locked) {
        notify_client(client_fd, notification);
        sem_post(&client_mutex);
    }

    if (wait_for_semaphore(&worker_mutex) == 0) {
        struct Worker* worker = findWorkerByFd(&worker_pool, fd);
        if (worker != NULL) {
            if (failed)
                worker->jobs_failed++;
            else
                worker->jobs_completed++;

            if (worker->status == WORKER_BUSY) {
                worker->status = WORKER_IDLE;
                sem_post(&workers_available);
            }
        }
        sem_post(&worker_mutex);
    }

    log_worker_event("Job %d %s\n", job_id, failed ? "failed" : "completed");
}

static void handle_worker_line(int fd, char* line)
{
    size_t length = strlen(line);
    if (length > 0 && line[length - 1] == '\r')
        line[length - 1] = '\0';

    if (strncmp(line, "DONE ", 5) == 0) {
        handle_completion(fd, line, 0);
    } else if (strncmp(line, "FAILED ", 7) == 0) {
        handle_completion(fd, line, 1);
    } else if (strcmp(line, "HEARTBEAT") == 0) {
        if (wait_for_semaphore(&worker_mutex) == -1)
            return;

        struct Worker* worker = findWorkerByFd(&worker_pool, fd);
        if (worker != NULL) {
            worker->last_heartbeat = time(NULL);
            if (worker->status == WORKER_OFFLINE) {
                int job_in_progress = 0;
                if (wait_for_semaphore(&registry_mutex) == 0) {
                    for (int i = 0; i < MAX_JOB_NUM; i++) {
                        if (registry[i] != NULL && registry[i]->worker_fd == fd &&
                            registry[i]->status == JOB_IN_PROGRESS) {
                            job_in_progress = 1;
                            break;
                        }
                    }
                    sem_post(&registry_mutex);
                }

                worker->status = job_in_progress ? WORKER_BUSY : WORKER_IDLE;
                if (!job_in_progress)
                    sem_post(&workers_available);
            }
        }
        sem_post(&worker_mutex);
    } else if (line[0] != '\0') {
        log_worker_event("Ignoring unknown worker message\n");
    }
}

void* worker_handler(void* arg)
{
    if (arg == NULL)
        return NULL;

    int fd = *((int*)arg);
    free(arg);

    if (wait_for_semaphore(&worker_mutex) == -1) {
        close(fd);
        return NULL;
    }
    WorkerPool_Add(&worker_pool, createWorker(fd));
    struct Worker* worker = findWorkerByFd(&worker_pool, fd);
    int worker_id = worker != NULL ? worker->worker_id : -1;
    if (worker != NULL)
        sem_post(&workers_available);
    sem_post(&worker_mutex);

    if (worker == NULL) {
        close(fd);
        return NULL;
    }
    log_worker_event("Worker %d connected (fd: %d)\n", worker_id, fd);

    char read_buffer[1024];
    char line[WORKER_LINE_SIZE];
    size_t line_length = 0;
    int line_too_long = 0;

    for (;;) {
        ssize_t bytes_read = read(fd, read_buffer, sizeof(read_buffer));
        if (bytes_read < 0 && errno == EINTR)
            continue;
        if (bytes_read <= 0)
            break;

        for (ssize_t i = 0; i < bytes_read; i++) {
            char ch = read_buffer[i];
            if (ch == '\n') {
                if (line_too_long) {
                    log_worker_event("Discarded oversized worker message\n");
                } else {
                    line[line_length] = '\0';
                    handle_worker_line(fd, line);
                }
                line_length = 0;
                line_too_long = 0;
            } else if (!line_too_long) {
                if (line_length + 1 < sizeof(line))
                    line[line_length++] = ch;
                else
                    line_too_long = 1;
            }
        }
    }

    if (wait_for_semaphore(&registry_mutex) == 0) {
        for (int i = 0; i < MAX_JOB_NUM; i++) {
            struct Job* job = registry[i];
            if (job == NULL || job->worker_fd != fd || job->status != JOB_IN_PROGRESS)
                continue;

            if (wait_for_semaphore(&empty) == -1)
                continue;
            if (wait_for_semaphore(&queue_mutex) == -1) {
                sem_post(&empty);
                continue;
            }

            job->status = JOB_PENDING;
            job->worker_fd = -1;
            enqueue(&job_queue, job);
            sem_post(&queue_mutex);
            sem_post(&full);
            log_worker_event("Worker %d disconnected; job %d requeued\n", worker_id, job->job_id);
        }
        sem_post(&registry_mutex);
    }

    if (wait_for_semaphore(&worker_mutex) == 0) {
        worker = findWorkerByFd(&worker_pool, fd);
        if (worker != NULL && worker->status == WORKER_IDLE)
            sem_trywait(&workers_available);
        WorkerPool_Remove(&worker_pool, fd);
        sem_post(&worker_mutex);
    }

    log_worker_event("Worker removed (worker_id: %d, fd: %d)\n", worker_id, fd);
    close(fd);
    return NULL;
}