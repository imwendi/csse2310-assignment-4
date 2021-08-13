#ifndef CLIENTLIST_H
#define CLIENTLIST_H

#include "clientThread.h"
#include "clientList.h"
#include "lineList.h"

/* 
 * Indices for the statistics values of the stats member of a ClientList or
 * a ClientThread struct
 * */
typedef enum {
    SAY_COUNT,
    KICK_COUNT,
    LIST_COUNT,
    AUTH_COUNT,
    NAME_COUNT,
    LEAVE_COUNT
} StatIndices;

typedef struct ClientNode ClientNode;

/*
 * Struct representing a node in a doubly linked list used to store
 * ClientThread structs.
 */
struct ClientNode {
    /* ClientThread struct of the node */
    ClientThread *client;
    /* Pointer to the previous node */
    ClientNode *prev;
    /* Pointer to the next node */
    ClientNode *next;
};

/*
 * Struct representing a linked list of ClientNodes.
 */
typedef struct {
    /* 
     * Password all clients are checked against during authentication.
     * This is set by the authfile given to the server.
     */
    char *password;
    /* Array containing the following statistics about clients in the server:
     *
     * {#SAY, #KICK, #LIST, #AUTH, #NAME, #LEAVE}
     *
     * where #SAY etc. are the number of times the respective command was
     * handled by the server from any client
     */
    int *stats;
    /* Pointer to the head of the list */
    ClientNode *head;
    /* Mutex controlling access to the list */
    pthread_mutex_t *lock;
} ClientList;

ClientList *init_client_list();
void set_password(ClientList *clients, char *password);
void free_client_list();
void add_client(ClientList *clients, ClientThread *client);
void remove_client(ClientList *clients, ClientThread *client);
ClientThread *get_client_by_name(ClientList *clients, char *name);
void send_all_clients(ClientList *clients, char *msg, ...);
LineList *get_names(ClientList *clients);
char *server_stat_line(ClientList *clients);

#endif
