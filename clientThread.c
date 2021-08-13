#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include "clientThread.h"
#include "clientList.h"

/* Number of digits in the largest number an int can store (65535) */
#define MAX_DIGS 5
/* 
 * Number of different commands a server should store statistics per each 
 * connected client 
 */
#define CLIENT_STAT_NUM 3
/*
 * Creates a new ClientThread struct, initialize default values for its members
 * and returns pointer to it.
 */
ClientThread *init_client_thread(FILE *readFrom, FILE *writeTo) {
    ClientThread *client = (ClientThread *) malloc(sizeof(ClientThread));
    client->isActive = true;
    client->name = NULL;
    client->stats = calloc(CLIENT_STAT_NUM, sizeof(int));
    client->readFrom = readFrom;
    client->writeTo = writeTo;
    client->lock = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(client->lock, 0);

    return client;
}

/*
 * Closes file descriptors used by a ClientThread struct, destroys its mutex
 * member and frees memory allocated to it.
 */
void free_client_thread(ClientThread *client) {
    pthread_mutex_lock(client->lock);
    free(client->name);
    free(client->stats);
    fclose(client->readFrom);
    fclose(client->writeTo);
    pthread_mutex_unlock(client->lock);
    pthread_mutex_destroy(client->lock);
    free(client->lock);
    free(client);
}

/*
 * Allocates memory for and sets the name member of a ClientThread struct to
 * a given string.
 */
void set_client_name(ClientThread *client, char *name) {
    pthread_mutex_lock(client->lock);
    client->name = (char *) calloc(strlen(name) + 1, sizeof(char));
    strcpy(client->name, name);
    pthread_mutex_unlock(client->lock);
}

/* Returns the isActive flag of a ClientThread struct */
bool get_active_status(ClientThread *client) {
    bool isActive;
    pthread_mutex_lock(client->lock);
    isActive = client->isActive;
    pthread_mutex_unlock(client->lock);

    return isActive;
}

/* Sets the isActive flag of a ClientThread struct to false */
void disable_client(ClientThread *client) {
    pthread_mutex_lock(client->lock);
    client->isActive = false;
    pthread_mutex_unlock(client->lock);
}

/*
 * Sends a given string to the client corresponding to the given ClientThread
 * struct.
 *
 * The string is given as a formatting string and a variable number of
 * arguments in a similar manner to printf() as vfprintf is used.
 *
 * Note that a new line character is appended to the end of the string before
 * it is sent.
 */
void send_client(ClientThread *client, char *format, ...) {
    // Retrieve string formatting arguments
    va_list args;
    va_start(args, format);

    vfprintf(client->writeTo, format, args);
    fprintf(client->writeTo, "\n");
    fflush(client->writeTo);

    va_end(args);
}

/*
 * Wrapper for read_file_line().
 * Reads a line of text sent by a client to a string and returns that string.
 * Also sets a bool flag to true if the read line is completely empty
 * (i.e. only contains EOF). (See read_file_line() in lineList.c)
 */
char *read_client_line(ClientThread *client, bool *isLineEmpty) {
    char *line = read_file_line(client->readFrom, isLineEmpty);

    return line;
}

/*
 * Creates and returns a string representation of a ClientThread's saved
 * statistics.
 *
 * The format of this string is:
 *
 * "<name>:SAY:<#SAY>:KICK:<#KICK>:LIST:<#LIST>\n"
 *
 * where name is the name of the client and #SAY etc. are the number of times
 * the client has called the respective commands.
 *
 * i.e. for a client John who's sent SAY, KICK and LIST each 3 times, its stat
 * line should be:
 *
 * "John:SAY:3:KICK:3:LIST:3"
 */
char *client_stat_line(ClientThread *client) {
    pthread_mutex_lock(client->lock);
    char *statLine = calloc(strlen(client->name)
            + strlen(":SAY::KICK::LIST:\n")
            + MAX_DIGS * 3 + 1, sizeof(char)); 

    int *stats = client->stats;
    sprintf(statLine, "%s:SAY:%d:KICK:%d:LIST:%d\n", client->name, 
            stats[SAY_COUNT], stats[KICK_COUNT], stats[LIST_COUNT]);

    pthread_mutex_unlock(client->lock);

    return statLine;
}
