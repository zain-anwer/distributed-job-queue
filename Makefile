CC = gcc
CFLAGS = -lncurses -Wall -g -pthread -fsanitize=address

# Source files
SERVER_SRC = src/server.c src/handlers/client_handler.c src/handlers/worker_handler.c \
             src/handlers/dispatcher.c src/handlers/health_check_handler.c src/handlers/dashboard_handler.c \
             util/sync.c lib/job_queue.c lib/client_pool.c lib/worker_pool.c util/socket.c

CLIENT_SRC = src/client.c util/socket.c
WORKER_SRC = src/worker.c util/socket.c
STRESS_TEST_CLIENT_SRC = src/stress_test_client.c util/socket.c

# Object files
SERVER_OBJS = $(SERVER_SRC:.c=.o)
CLIENT_OBJS = $(CLIENT_SRC:.c=.o)
WORKER_OBJS = $(WORKER_SRC:.c=.o)
STRESS_TEST_CLIENT_OBJS = $(STRESS_TEST_CLIENT_SRC:.c=.o)

all: server client worker stress_test_client

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS)

worker: $(WORKER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(WORKER_OBJS)

stress_test_client: $(STRESS_TEST_CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(STRESS_TEST_CLIENT_OBJS)


clean:
	rm -f server client worker stress_test_client *.o lib/*.o src/*.o src/handlers/*.o lib/*.o util/*.o
