**Distributed Job Queue System**

A lightweight distributed job queue written in C — inspired by systems like Celery and RabbitMQ, built from scratch using sockets, POSIX threads, semaphores, and pipes.

**Overview**

This system allows multiple clients to submit jobs to a central broker, which queues them and dispatches them to a pool of registered workers. Workers execute jobs concurrently and report results back to the broker, which notifies the original client. The system handles worker failures gracefully by automatically re-queuing affected jobs.

**Architecture**
          
            [Client 1] ──┐                          ┌── [Worker 1 (parent+child)]
            [Client 2] ──┤──► [Job Broker] ─────────┤── [Worker 2 (parent+child)]
            [Client 3] ──┘                          └── [Worker 3 (parent+child)]


**Project Structure**


            distributed-job-queue/
            ├── src/
            │   ├── server.c                  # Broker main — accept loop
            │   ├── client.c                  # Client — submit/status/notifications
            │   ├── worker.c                  # Worker — fork/pipe/execute
            │   └── handlers/
            │       ├── client_handler.c      # Handles SUBMIT and STATUS from clients
            │       ├── worker_handler.c      # Handles DONE, FAILED, HEARTBEAT from workers
            │       ├── dispatcher.c          # Dequeues jobs, assigns to idle workers
            │       └── health_check_handler.c# Monitors heartbeats, re-queues on failure
            ├── lib/
            │   ├── job_queue.c / .h          # Circular job queue + registry + Job struct
            │   ├── worker_pool.c / .h        # Worker linked list + Worker struct
            │   └── client_pool.c / .h        # Client tracking
            ├── util/
            │   ├── socket.c / .h             # TCP socket helpers
            │   └── sync.c / .h               # All semaphores and mutexes + sync_init()
            └── Makefile
