#ifndef SERVERUTILS_H
#define SERVERUTILS_H

#include "clientList.h"

/*
 * Struct containing data to be passed to each thread of the server for
 * handling an individual client.
 */
typedef struct {
    /* ClientList of all clients in the server */
    ClientList *clients;
    /* ClientThread corresponding to the client being managed by a particular
     * client handling thread
     */
    ClientThread *client;
} ClientThreadData;

void spawn_client_thread(ClientList *clients, int fdClient);
void toggle_sighup(int mode, int *sig);
void *sighup_stats_handler(void *arg);

#endif
