#ifndef LOG_QUEUE_H
#define LOG_QUEUE_H

struct LogQueue {
    char log_messages[200][1024];
    int count;
    int head;
};

extern struct LogQueue* log_queue;

#endif