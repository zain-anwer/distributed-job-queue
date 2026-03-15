CC     = gcc
FLAGS  = -pthread -Wall -fsanitize=address -g

all: server client worker

server: src/server.c src/client_handler.c src/worker_handler.c src/dispatcher.c \
        lib/socket.c lib/job_queue.c lib/client_pool.c lib/worker_pool.c \
        util/sync.c util/health_check_handler.c
	$(CC) $(FLAGS) -o server src/server.c src/client_handler.c src/worker_handler.c \
	src/dispatcher.c lib/socket.c lib/job_queue.c lib/client_pool.c lib/worker_pool.c \
	util/sync.c util/health_check_handler.c

client: src/client.c lib/socket.c
	$(CC) $(FLAGS) -o client src/client.c lib/socket.c

worker: src/worker.c lib/socket.c
	$(CC) $(FLAGS) -o worker src/worker.c lib/socket.c

clean:
	rm -f server client worker