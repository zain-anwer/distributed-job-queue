<div align="center">

# Distributed Job Queue

A small distributed job queue in C, built with TCP sockets, POSIX threads, semaphores, and processes.

</div>

Clients submit jobs to a central broker. The broker queues them and assigns them to connected workers. Workers report completion or failure, and the broker sends completion notifications to the submitting client.

## Architecture

```text
Client(s) --TCP--> Broker --TCP--> Worker parent process
                      |                    |  pipe
                      |                    v
                      |              Worker child process
                      |<-- result/heartbeat-|
                      +-- ncurses dashboard
```

### Components

- **Client (`src/client.c`)**: An ncurses interface for submitting a payload and checking a job's status. Job completion notifications appear in the interface.
- **Broker (`src/server.c`)**: Listens on port `2000`, accepts client and worker connections, and coordinates job state.
- **Dispatcher (`src/handlers/dispatcher.c`)**: Waits for queued jobs and available workers, then sends each job to an idle worker.
- **Worker (`src/worker.c`)**: Each worker has a parent process that reads broker messages and sends heartbeats, and a child process that executes jobs. A pipe passes work from parent to child.
- **Handlers (`src/handlers/`)**: Process client requests, worker messages, health checks, and the broker dashboard.
- **Shared data and utilities (`lib/`, `util/`)**: Hold the job queue and registry, worker/client pools, socket helpers, and synchronization primitives.

## How a Job Flows

1. The client connects to the broker at `127.0.0.1:2000` and sends the `CLIENT` handshake.
2. The user submits a payload. The client sends `SUBMIT <payload>`; the broker registers the job, places it in the queue, and acknowledges it with a job ID.
3. The dispatcher waits for both a queued job and an available worker. It marks the job in progress and sends `JOB <job_id> <payload>` to that worker.
4. The worker parent forwards the job to its child through a pipe. The child simulates processing for 1-4 seconds and sends a `DONE` or `FAILED` message to the broker.
5. The broker updates the job status and notifies the submitting client. The client can also send `STATUS <job_id>` to query the current status.

Messages are newline-terminated after the initial six-byte `CLIENT` or `WORKER` handshake. Worker heartbeats are sent once per second.

## Concurrency and Synchronization

The broker starts three background threads: a dispatcher, a health checker, and the ncurses dashboard. It creates a detached handler thread for each accepted client or worker connection. The worker parent also starts a heartbeat thread while its child process handles jobs.

POSIX semaphores coordinate access to the queue, job registry, worker pool, client pool, and log queue. The `empty` and `full` semaphores track available queue slots and queued jobs; `workers_available` tracks idle workers. Mutex-like semaphores protect shared structures while they are read or updated.

## Failure Handling and Limits

- The health checker runs about once per second. If a worker has not sent a heartbeat for more than 15 seconds, it marks that worker offline and requeues its in-progress job.
- If a worker disconnects, its handler also requeues its in-progress jobs.
- Workers succeed by default. `WORKER_FAILURE_RATE` can simulate job failures; it accepts an integer percentage from `0` to `100`.
- The queue and job registry each have a capacity of 200. The registry currently retains jobs for the lifetime of the broker process and does not evict completed jobs, so a broker accepts at most 200 jobs before reporting that its registry is full.
- Requeueing supports recovery from worker failures, but this prototype does not provide durable storage or exactly-once execution guarantees.

## Requirements

The project targets Linux. Windows users can build and run it under WSL. You need Git, GCC, Make, pthreads, and the ncurses development headers/library.

On Ubuntu or WSL, install the build dependencies:

```bash
sudo apt update
sudo apt install -y git build-essential libncurses-dev
```

## Clone and Build

```bash
git clone https://github.com/zain-anwer/distributed-job-queue.git
cd distributed-job-queue
make
```

`make` builds `server`, `client`, `worker`, and `stress_test_client`. Build flags are defined in the `Makefile`; the current build enables AddressSanitizer.

To remove the build outputs and object files:

```bash
make clean
```

## Run Locally

Run each command in its own terminal. Start the broker first; it binds to all interfaces on port `2000` and displays an ncurses dashboard.

Terminal 1, broker:

```bash
./server
```

Terminal 2, worker:

```bash
./worker
```

Start additional workers by running `./worker` in more terminals. Then open the client:

Terminal 3, client:

```bash
./client
```

In the client, choose **1** to submit a job, **2** to check a job ID, or **3** to quit. The client terminal should be at least 60 columns by 14 rows. Both the client and broker dashboard use ncurses, so keep them in separate terminals.

The client and worker default to `127.0.0.1:2000`. To connect either to a broker on another host, pass the broker IP address and port. Replace `192.0.2.10` below with the broker's reachable IP address:

```bash
./worker 192.0.2.10 2000
./client 192.0.2.10 2000
```

The broker currently listens on port `2000`; it does not accept a port argument.

To simulate a 10% worker job-failure rate:

```bash
WORKER_FAILURE_RATE=10 ./worker
```

The stress-test client submits up to 200 sample jobs, at half-second intervals:

```bash
./stress_test_client
```

Use it against a fresh broker with available workers. It consumes the broker's finite 200-job registry.

## Protocol Overview

| Direction | Message | Purpose |
| --- | --- | --- |
| Client to broker | `SUBMIT <payload>` | Submit a job |
| Client to broker | `STATUS <job_id>` | Query job status |
| Broker to client | `ACK: JOB SUBMITTED - JOB ID: <id>` | Confirm submission |
| Broker to client | `STATUS: JOB_PENDING`, `JOB_IN_PROGRESS`, `JOB_COMPLETED`, or `JOB_FAILED` | Return status |
| Broker to worker | `JOB <job_id> <payload>` | Assign work |
| Worker to broker | `DONE <job_id> <result>` or `FAILED <job_id> <reason>` | Report completion |
| Worker to broker | `HEARTBEAT` | Report that the worker parent is alive |

## Project Layout

```text
distributed-job-queue/
|-- src/
|   |-- client.c
|   |-- server.c
|   |-- stress_test_client.c
|   |-- worker.c
|   `-- handlers/
|       |-- client_handler.c
|       |-- dashboard_handler.c
|       |-- dispatcher.c
|       |-- health_check_handler.c
|       `-- worker_handler.c
|-- lib/       # Job queue/registry and client/worker pools
|-- util/      # Socket and semaphore utilities
|-- jobs/      # Standalone image/OCR job programs
`-- Makefile
```
