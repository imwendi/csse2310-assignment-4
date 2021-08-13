#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

/*
 * Struct containing information to an individual client being handled
 * by the server. This struct is used by the server's client handling
 * threads to perform various functionality to do with handling messages
 * sent by the client.
 */
typedef struct {
    /*
     * Flag for whether or not the client should still be communicated with.
     * Tells client handling threads when to exit their message handling
     * loops. true by default.
     */
    bool isActive;
    /* Name of the client; set by name negotiation */
    char *name; 
    /* 
     * Array containing the following statistics about the client:
     *
     * {#SAY, #KICK, #LIST}
     *
     * where #SAY etc. are the number of times the respective command was sent
     * by the client
     */
    int *stats;
    /*
     * File pointer wrapping a file descriptor used to receive messages
     * from a client.
     */
    FILE *readFrom;
    /*
     * File pointer wrapping a file descriptor used to send messages to a
     * client.
     */
    FILE *writeTo;
    /*
     * Mutex used to prevent concurrent modification of ClientThread
     * structs.
     */
    pthread_mutex_t *lock;
} ClientThread;

ClientThread *init_client_thread(FILE *readFrom, FILE *writeTo);
void free_client_thread(ClientThread *client);
void set_client_name(ClientThread *client, char *name);
bool get_active_status(ClientThread *client);
void disable_client(ClientThread *client);
void send_client(ClientThread *client, char *format, ...);
char *read_client_line(ClientThread *client, bool *isLineEmpty);
char *client_stat_line(ClientThread *client);

#endif
