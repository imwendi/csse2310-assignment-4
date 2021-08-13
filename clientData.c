#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include "lineList.h"
#include "clientData.h"

/* 
 * Initializes a new ClientData struct given the name of the client, the
 * password from its authfile and file descriptor for the server it is 
 * connected to.
 *
 * Returns a pointer to the new struct.
 */
ClientData *init_client_data(char *name, char *password, int fdServer) {
    ClientData *data = (ClientData *) calloc(1, sizeof(ClientData));
    data->isActive = true;
    data->authenticated = false;
    data->name = name;
    data->password = password;
    data->exitCode = -1; // Default error code is -1, for an unset code
    data->clientNo = -1;
    data->lock = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(data->lock, 0);
    // Create read and write file pointers to the server
    data->readFrom = fdopen(fdServer, "r");
    data->writeTo = fdopen(dup(fdServer), "w");

    return data;
}

/* Increments the clientNo of a ClientData struct */
void next_client_no(ClientData *data) {
    data->clientNo++;
}

/* 
 * Sends a string to the server a client is connected to.
 * The string is given as a formatting string and a variable number of 
 * arguments in a similar manner to printf() as vfprintf is used.
 *
 * Note that a new line character is appended to the end of the string before
 * it is sent.
 */
void send_to_server(ClientData *data, char *format, ...) {
    // Retrive string formatting arguments
    va_list args;
    va_start(args, format);

    vfprintf(data->writeTo, format, args);
    fprintf(data->writeTo, "\n");
    fflush(data->writeTo);

    va_end(args);
}

/*
 * Reads a single line of messages a server has sent to a client and returns
 * the message as a string.
 *
 * Sets a given bool flag to true if the line read was empty (just contains
 * EOF). (see read_file_line() in lineList.c)
 *
 * Moreover, if the socket connect to the server has errors (i.e. if due to
 * unexpected server closure) and a line cannot be read from the server, NULL 
 * is returned instead and the given bool flag is set to true as well.
 */
char *read_server_line(ClientData *data, bool *serverLeft) {
    // Setup getsockopt to check if the server socket fd has any errors
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(fileno(data->readFrom), SOL_SOCKET, SO_ERROR, &error, &len);

    // Toggle isLineEmpty if there is an error
    if (error != 0) {
        printf("got sock error\n");
        *serverLeft = true;
        return NULL;
    }

    return read_file_line(data->readFrom, serverLeft);
}

/* Returns the name of the client as a string given data of the client.
 * If clientNo in data is <0, the default name of the client is returned.
 * Else the returned name is the default name with the clientNo appended to the
 * end of it, i.e. client0, client1
 *
 */
char *get_name(ClientData *data) {
    char *defaultName = data->name;
    int clientNo = data->clientNo;
    // Allocate memory for the name and 9 digits for the clientNo 
    char *name = calloc(strlen(defaultName) + 10, sizeof(char));

    if (clientNo < 0) {
        strcpy(name, defaultName);
    } else {
        sprintf(name, "%s%d", defaultName, clientNo);
    }

    return name;
}

/* 
 * Prepares a client to be terminated:
 * - Sets the client's isActive flag to false
 * - If the client's exitCode is -1, i.e. not yet set, then it is set to the
 *   given exit code. This is to prevent cases where a server disconnect could
 *   overwrite a kicked error code.
 */
void disable_client(ClientData *data, int exitCode) {
    pthread_mutex_lock(data->lock);
    data->isActive = false;
    if (data->exitCode < 0) {
        data->exitCode = exitCode;
    }
    pthread_mutex_unlock(data->lock);
}

/* Frees memory allocated to a ClientData structure */
void free_client_data(ClientData *data) {
    free(data->password);
    fclose(data->writeTo);
    fclose(data->readFrom);
    pthread_mutex_destroy(data->lock);
    free(data->lock);
    free(data);
}
