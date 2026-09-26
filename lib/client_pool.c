#include "client_pool.h"

#include <string.h>

int curr_client_id = 1;
struct ClientPool client_pool = {.clients = NULL, .num_clients = 0};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* ----------------------------------- CLIENT STRUCT ---------------------------------------- */

struct Client createClient(int fd, bool connected) {
    struct Client client = {.client_id = curr_client_id++,.fd = fd,.connected = connected};
    return client;
}

/* ----------------------------- CLIENT POOL STRUCT ----------------------------------------- */

void ClientPool_init(struct ClientPool* pool) {
    pool->clients = NULL;
    pool->num_clients = 0;
}

void ClientPool_Add(struct ClientPool* pool, struct Client client) {
    struct Client* clients = realloc(pool->clients,
                                     sizeof(*pool->clients) * (size_t)(pool->num_clients + 1));
    if (clients == NULL)
        return;

    pool->clients = clients;
    pool->clients[pool->num_clients] = client;
    pool->num_clients++;
}

void ClientPool_Remove(struct ClientPool* pool, int client_fd)
{
    for (int i = 0; i < pool->num_clients; i++)
    {
        if (pool->clients[i].fd != client_fd)
            continue;

        if (i + 1 < pool->num_clients)
            memmove(&pool->clients[i], &pool->clients[i + 1],
                    sizeof(*pool->clients) * (size_t)(pool->num_clients - i - 1));
        pool->num_clients--;
        if (pool->num_clients == 0)
        {
            free(pool->clients);
            pool->clients = NULL;
        }
        return;
    }
}