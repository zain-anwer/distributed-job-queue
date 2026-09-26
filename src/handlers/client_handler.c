// clients submit jobs that get assigned to workers by the central server
// clients can give two commands (submit and status) to get an equivalent response
// client handler thread

#include "client_handler.h"

#include <errno.h>
#include <limits.h>
#include <sys/socket.h>

static int wait_for_semaphore(sem_t* semaphore)
{
    int result;
    do {
        result = sem_wait(semaphore);
    } while (result == -1 && errno == EINTR);
    return result;
}

static int send_line(int fd, const char* message)
{
    if (wait_for_semaphore(&client_mutex) == -1)
        return -1;

    size_t sent = 0;
    size_t length = strlen(message);
    while (sent < length) {
        ssize_t result = send(fd, message + sent, length - sent, MSG_NOSIGNAL);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0) {
            sem_post(&client_mutex);
            return -1;
        }
        sent += (size_t)result;
    }
    sem_post(&client_mutex);
    return 0;
}

static void handle_client_line(int fd, char* line)
{
    if (strncmp(line, "SUBMIT ", 7) == 0) {
        char* payload = line + 7;
        if (*payload == '\0') {
            send_line(fd, "ERROR: JOB PAYLOAD EMPTY\n");
            return;
        }

        if (wait_for_semaphore(&registry_mutex) == -1)
            return;
        if (jobs_registered >= MAX_JOB_NUM) {
            sem_post(&registry_mutex);
            send_line(fd, "ERROR: JOB REGISTRY FULL\n");
            return;
        }

        struct Job* new_job = malloc(sizeof(*new_job));
        if (new_job == NULL) {
            sem_post(&registry_mutex);
            send_line(fd, "ERROR: OUT OF MEMORY\n");
            return;
        }

        Job_init(new_job);
        new_job->client_fd = fd;
        snprintf(new_job->payload, sizeof(new_job->payload), "%s", payload);
        registry[jobs_registered++] = new_job;
        sem_post(&registry_mutex);

        if (wait_for_semaphore(&empty) == -1) {
            send_line(fd, "ERROR: JOB QUEUE UNAVAILABLE\n");
            return;
        }
        if (wait_for_semaphore(&queue_mutex) == -1) {
            sem_post(&empty);
            send_line(fd, "ERROR: JOB QUEUE UNAVAILABLE\n");
            return;
        }
        enqueue(&job_queue, new_job);
        sem_post(&queue_mutex);
        sem_post(&full);

        char ack[ACK_SIZE];
        snprintf(ack, sizeof(ack), "ACK: JOB SUBMITTED - JOB ID: %d\n", new_job->job_id);
        send_line(fd, ack);
        return;
    }

    if (strncmp(line, "STATUS ", 7) == 0) {
        char* end = NULL;
        errno = 0;
        long requested_id = strtol(line + 7, &end, 10);
        if (errno != 0 || end == line + 7 || *end != '\0' ||
            requested_id < 1 || requested_id > INT_MAX) {
            send_line(fd, "ERROR: INVALID JOB ID\n");
            return;
        }

        char response[RESPONSE_SIZE];
        if (wait_for_semaphore(&registry_mutex) == -1)
            return;
        struct Job* job = find_job_by_id(registry, (int)requested_id);
        if (job == NULL) {
            snprintf(response, sizeof(response), "ERROR: JOB NOT FOUND\n");
        } else {
            const char* status = "UNKNOWN";
            switch (job->status) {
                case JOB_PENDING: status = "JOB_PENDING"; break;
                case JOB_IN_PROGRESS: status = "JOB_IN_PROGRESS"; break;
                case JOB_COMPLETED: status = "JOB_COMPLETED"; break;
                case JOB_FAILED: status = "JOB_FAILED"; break;
            }
            snprintf(response, sizeof(response), "STATUS: %s\n", status);
        }
        sem_post(&registry_mutex);
        send_line(fd, response);
        return;
    }

    send_line(fd, "ERROR: UNKNOWN COMMAND\n");
}

/* the following program will be the producer (populate the queue) */

void* client_handler(void* arg) {
    if (arg == NULL)
        return NULL;

    int fd = *(int*)arg;
    free(arg);

    if (wait_for_semaphore(&client_mutex) == 0) {
        ClientPool_Add(&client_pool, createClient(fd, true));
        sem_post(&client_mutex);
    }

    char buffer[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    size_t line_length = 0;
    int line_too_long = 0;

    for (;;) {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read < 0 && errno == EINTR)
            continue;
        if (bytes_read <= 0)
            break;

        for (ssize_t i = 0; i < bytes_read; i++) {
            char ch = buffer[i];
            if (ch == '\n') {
                if (!line_too_long) {
                    line[line_length] = '\0';
                    if (line_length > 0 && line[line_length - 1] == '\r')
                        line[line_length - 1] = '\0';
                    handle_client_line(fd, line);
                } else {
                    send_line(fd, "ERROR: REQUEST TOO LONG\n");
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
        for (int i = 0; i < jobs_registered; i++) {
            if (registry[i] != NULL && registry[i]->client_fd == fd)
                registry[i]->client_fd = -1;
        }
        sem_post(&registry_mutex);
    }

	if (wait_for_semaphore(&client_mutex) == 0) {
	    ClientPool_Remove(&client_pool, fd);
	    sem_post(&client_mutex);
	}
	if (wait_for_semaphore(&log_mutex) == 0) {

		int idx = log_queue->head;

		snprintf(log_queue->log_messages[idx],1024,"REMOVED CLIENT -> (client_fd: %d)\n",fd);
		
		log_queue->head = (log_queue->head + 1) % 200;
		
		if (log_queue->count < 200)
			log_queue->count++;

	sem_post(&log_mutex);
	}

        close(fd);
        return NULL;
}


