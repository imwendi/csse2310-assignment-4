#ifndef CLIENTDATA_H
#define CLIENTDATA_H

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

/*
 * Struct to store information pertaining to a client instance
 */
typedef struct {
    /* 
     * Whether or not the client should continue handling functionality
     * or terminate
     */
    bool isActive;
    /*
     * Whether or not the client has completed both authentication and name
     * negotiation.
     */
    bool authenticated;
    /*
     * The base name of a client - the name specified as a commandline arg
     * when the client was run.
     */
    char *name;
    /* The password given in the client's authfile */
    char *password;
    /* 
     * The error code the client should exit with when it terminates
     */
    int exitCode;
    /* 
     * Number to appended to name of a client to differentiate from other
     * clients in the server with the same base name.
     * starts at -1 (no number) and increments to 0, 1, 2... each time the
     * client receives a NAME_TAKEN command.
     */
    int clientNo;
    /*
     * File pointer wrapping a file descriptor used to send messages to a 
     * server.
     */
    FILE *writeTo;
    /*
     * File pointer wrapping a file descriptor used to read messages from 
     * a server.
     */
    FILE *readFrom;
    /* Thread id of the thread used to handle server input */
    pthread_t serverHandler;
    /* Thread id of the thread used to handle user input */
    pthread_t userHandler;
    /* 
     * Mutex used to block concurrent modification of ClientData structs by
     * multiple threads.
     */
    pthread_mutex_t *lock;
} ClientData;

ClientData *init_client_data(char *name, char *password, int fdServer);
void next_client_no(ClientData *data);
void send_to_server(ClientData *data, char *format, ...);
char *read_server_line(ClientData *data, bool *serverLeft);
void free_client_data(ClientData *data);
char *get_name(ClientData *data);
void disable_client(ClientData *data, int exitCode);

#endif
