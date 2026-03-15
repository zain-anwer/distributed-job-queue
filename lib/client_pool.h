#ifndef CLIENT_POOL_H
#define CLIENT_POOL_H

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>

extern int curr_client_id;
extern struct ClientPool client_pool;
extern pthread_mutex_t mutex;

/* ----------------------------------- CLIENT STRUCT ---------------------------------------- */


struct Client {
    int fd;
    int client_id;
    char ip_address[INET_ADDRSTRLEN];  // IPV4 address length
    int port_number;
    bool connected;
};

struct Client createClient(int fd, bool connected);

/* ----------------------------- CLIENT POOL STRUCT ----------------------------------------- */

struct ClientPool {
    struct Client* clients;
    int num_clients;
};

void ClientPool_init(struct ClientPool* pool);

void ClientPool_Add(struct ClientPool* pool, struct Client client);

void ClientPool_Remove(struct ClientPool* pool, int client_fd);

#endif