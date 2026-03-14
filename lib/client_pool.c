#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdbool.h>

/* ----------------------------------- CLIENT STRUCT ---------------------------------------- */

int curr_client_id = 1; 

struct Client {
    int fd;
    int client_id;
    char ip_address[INET_ADDRSTRLEN];  // IPV4 address length
    int port_number;
    bool connected;
};

struct Client createClient(int fd, bool connected) {
    struct Client client = {.client_id = curr_client_id++,.fd = fd,.connected = connected};
    return client;
}

/* ----------------------------- CLIENT POOL STRUCT ----------------------------------------- */

struct ClientPool {
    struct Client* clients;
    int num_clients;
};

void ClientPool_init(struct ClientPool* pool) {
    pool->clients = NULL;
    pool->num_clients = 0;
}

void ClientPool_Add(struct ClientPool* pool, struct Client client) {
    if (pool->num_clients == 0)
    {
        pool->clients = (struct Client*) malloc(sizeof(struct Client));
        pool->clients[pool->num_clients] = client;
        pool->num_clients += 1;
        return;
    }
    pool->num_clients += 1;
    pool->clients = realloc(pool->clients,sizeof(struct Client)*(pool->num_clients));
    pool->clients[pool->num_clients-1] = client;
}

void ClientPool_Remove(struct ClientPool* pool, int client_fd)
{
    
    int i;
    struct ClientPool* new_pool = malloc(sizeof(struct ClientPool));
    ClientPool_init(new_pool);

    for (i = 0;i < pool->num_clients;i++)
    {
        if (pool->clients[i].fd != client_fd)
            ClientPool_Add(new_pool,pool->clients[i]);
    }

    free(pool->clients);
    *pool = *new_pool;
    free(new_pool);
}

/* global client pool instance that should be declared using extern globally in files that import it */

struct ClientPool client_pool;