#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include "lineList.h"
#include "clientThread.h"
#include "clientList.h"

/* Number of digits in the largest number an int can store (65535) */
#define MAX_DIGS 5
/* Number of different commands a server should store statistics for */
#define SERVER_STAT_NUM 6

/* 
 * Allocates memory for and initializes a new ClientNode.
 * Returns pointer to the created ClientNode.
 */
ClientNode *init_node(ClientThread *client) {
    ClientNode *node = (ClientNode *) malloc(sizeof(ClientNode));
    node->client = client;
    node->next = NULL;
    node->prev = NULL;

    return node;
}

/*
 * Frees memory allocated to a ClientNode and destroys its mutex lock member.
 */
void free_node(ClientNode *node) {
    free_client_thread(node->client);
    free(node);
}

/*
 * Compares the names of the clients stored by two ClientNode structs with 
 * strcmp and returns the integer value of that comparison
 */
int compare_node_names(ClientNode *node1, ClientNode *node2) {
    return strcmp(node1->client->name, node2->client->name);
}

/*
 * Adds a ClientNode to a ClientList - a linked list of ClientNodes.
 * ClientLists are sorted lexiographically by client name and nodes are added
 * following this by the name of their ClientThread member.
 */
void add_node(ClientList *clients, ClientNode *node) {
    pthread_mutex_lock(clients->lock);
    // If the list is empty, make the given node the head
    if (clients->head == NULL) {
        clients->head = node;
        pthread_mutex_unlock(clients->lock);
        return;
    }

    ClientNode *currentNode = clients->head;

    // Check if the node should be before the head
    if (compare_node_names(node, currentNode) < 0) {
        currentNode->prev = node;
        node->next = currentNode;
        clients->head = node;
        pthread_mutex_unlock(clients->lock);
        return;
    }

    /*
     * Iterate over the list and insert the node just before the first
     * node found that is lexiographically greater
     */
    while (currentNode->next != NULL) {
        currentNode = currentNode->next;
        if (compare_node_names(node, currentNode) < 0) {
            node->next = currentNode;
            node->prev = currentNode->prev;
            currentNode->prev->next = node;
            currentNode->prev = node;
            pthread_mutex_unlock(clients->lock);
            return;
        }
    }

    /* 
     * No node was found that was lexiographically greater than the given node,
     * so add the node to the list's end.
     */
    currentNode->next = node;
    node->prev = currentNode;
    pthread_mutex_unlock(clients->lock);
}

/*
 * Wrapper for add_node().
 * Given a ClientInstance struct, creates a new ClientNode for it and adds it
 * to a given ClientList.
 */
void add_client(ClientList *clients, ClientThread *client) {
    add_node(clients, init_node(client));
}

/*
 * Removes a ClientNode from the linked list it is part of and frees memory
 * allocated to it.
 */
void remove_node(ClientList *clients, ClientNode *node) {
    // Check if the node is the head of the list.
    if (clients->head == node) {
        // Make the next node the head
        clients->head = node->next;
    } else {
        // Set the node's next as the next of its prev node
        node->prev->next = node->next;
        // If the node is not as the end of the list, set the node's prev as 
        // the prev value of the next node
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }
    }

    free_node(node);
}

/*
 * Removes a ClientThread and the ClientNode it belongs to from a ClientList.
 * Memory allocated to that ClientNode and ClientThread is also freed.
 *
 * If the ClientThread is not in the ClientList, this function does nothing.
 */
void remove_client(ClientList *clients, ClientThread *client) {
    ClientThread *currentClient = NULL;
    pthread_mutex_lock(clients->lock);

    ClientNode *currentNode = clients->head;
    while (currentNode != NULL) {
        currentClient = currentNode->client;
        if (currentClient == client) {
        // Remove and free the matched node and client
            remove_node(clients, currentNode);
            break;
        }
        currentNode = currentNode->next;
    }

    pthread_mutex_unlock(clients->lock);
}

/*
 * Allocates memory for and creates a new ClientList struct.
 * Returns pointer to the new ClientList.
 */
ClientList *init_client_list() {
    ClientList *clients = (ClientList *) malloc(sizeof(ClientList));
    clients->password = NULL;
    clients->stats = calloc(SERVER_STAT_NUM, sizeof(int));
    clients->head = NULL;
    clients->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(clients->lock, 0);

    return clients;
}

/*
 * Sets the password member of a ClientList to a given string
 */
void set_password(ClientList *clients, char *password) {
    pthread_mutex_lock(clients->lock);
    clients->password = password;
    pthread_mutex_unlock(clients->lock);
    pthread_mutex_unlock(clients->lock);
}

/*
 * Returns a LineList struct containing the names of all clients stored in a
 * ClientList
 */
LineList *get_names(ClientList *clients) {
    pthread_mutex_lock(clients->lock);
    LineList *names = init_line_list();

    ClientNode *currentNode = clients->head;
    while (currentNode != NULL) {
        // Append each client name to the list
        add_to_lines(names, currentNode->client->name);
        currentNode = currentNode->next;
    }
    pthread_mutex_unlock(clients->lock);

    return names;
}

/*
 * Frees all memory in a ClientList struct as well as the corresponding
 * linked list. (i.e. memory allocated to each node connected to the head
 * of the ClientList is also freed).
 */
void free_client_list(ClientList *clients) {
    pthread_mutex_lock(clients->lock);
    
    // Free all nodes in the list
    if (clients->head != NULL) {
        ClientNode *currentNode = clients->head;
        ClientNode *nextNode;

        while (currentNode != NULL) {
            nextNode = currentNode->next;
            free_node(currentNode);
            currentNode = nextNode;
        }
    }

    if (clients->password != NULL) {
        free(clients->password);
    }

    // Destroy and free the lock
    pthread_mutex_unlock(clients->lock);
    pthread_mutex_destroy(clients->lock);
    free(clients->lock);
    free(clients->stats);

    free(clients);
}

/*
 * Finds and returns the first client in a ClientList with a given name.
 * If such a client is not found, NULL is returned.
 */
ClientThread *get_client_by_name(ClientList *clients, char *name) {
    ClientThread *client = NULL;
    pthread_mutex_lock(clients->lock);

    ClientNode *currentNode = clients->head;
    while (currentNode != NULL) {
        // Iterate over the linked list and break if a client with a matching 
        // name is found
        char *currentName = currentNode->client->name;
        if (currentName != NULL && !strcmp(currentName, name)) {
            client = currentNode->client;
            break;
        }
        currentNode = currentNode->next;
    }

    pthread_mutex_unlock(clients->lock);

    return client;
}

/*
 * Sends a string to all ACTIVE clients in a ClientList with a name that is not
 * NULL. (i.e. the string is not sent to clients who have not completed name
 * negotiation)
 *
 * The string is given as a formatting string and a variable number of
 * arguments in a similar manner to printf() as vprintf is used.
 *
 * Note that a new line character is appended to the end of the string before 
 * it is sent.
 */
void send_all_clients(ClientList *clients, char *format, ...) {
    pthread_mutex_lock(clients->lock);
    ClientNode *currentNode = clients->head;

    // Iterate of the linked list, sending the message to each client
    while (currentNode != NULL) {
        ClientThread *client = currentNode->client;
        pthread_mutex_lock(client->lock);
        if (client->isActive && client->name != NULL) {
            va_list args;
            va_start(args, format);
            vfprintf(currentNode->client->writeTo, format, args);
            fprintf(currentNode->client->writeTo, "\n");
            fflush(currentNode->client->writeTo);
            va_end(args);
        }
        pthread_mutex_unlock(client->lock);
        currentNode = currentNode->next;
    }
    
    pthread_mutex_unlock(clients->lock);
}

/*
 * Creates and returns a string representation of a ClientLists' statistics.
 * The format of this string (ignore spaces) is:
 *
 * "server:AUTH:<#AUTH>:NAME:<#NAME>:SAY:<#SAY>:KICK:<#KICK>:LIST:<#LIST>:
 * LEAVE:<#LEAVE>\n"
 *
 * where #SAY etc. are the number of times the client has called the respective
 * commands.
 *
 */
char *server_stat_line(ClientList *clients) {
    char *statLine = calloc(
            strlen("server:AUTH::NAME::SAY::KICK::LIST::LEAVE:\n") 
            + MAX_DIGS * 6 + 1, sizeof(char));

    int *stats = clients->stats;
    sprintf(statLine,
            "server:AUTH:%d:NAME:%d:SAY:%d:KICK:%d:LIST:%d:LEAVE:%d\n",
            stats[AUTH_COUNT], stats[NAME_COUNT], stats[SAY_COUNT],
            stats[KICK_COUNT], stats[LIST_COUNT], stats[LEAVE_COUNT]);

    return statLine;
}

