#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include "commands.h"
#include "clientUtils.h"
#include "lineList.h"
#include "clientData.h"
#include "errors.h"

int connect_to_server(const char *serverPort);

int main(int argc, char **argv) {

    if (argc != 4) {
        exit_with_msg(USAGE, CLIENT);
    }

    bool invalidAuthFile = false;
    char *password = get_password(argv[2], &invalidAuthFile);
    if (invalidAuthFile) {
        free(password);
        exit_with_msg(USAGE, CLIENT);
    }

    int fdServer = connect_to_server(argv[3]);
    if (fdServer < 0) {
        free(password);
        exit_with_msg(COMMS, CLIENT);
    }

    ClientData *data = init_client_data(argv[1], password, fdServer);
    suppress_sigpipe();
    start_client(data);

    while (1) {
        pthread_mutex_lock(data->lock);
        if (!data->isActive) {
            pthread_mutex_unlock(data->lock);
            break;
        }
        pthread_mutex_unlock(data->lock);
    }

    end_client(data);

    return 0;
}

/*
 * Attempts to create a socket and connect it to the a port given in string
 * representation.
 *
 * On successful connection, the file descriptor to the socket is returned,
 * else -1 is returned.
 */
int connect_to_server(const char *serverPort) {
    struct addrinfo *aiServer = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
                                                                    
    if ((getaddrinfo("localhost", serverPort, &hints, &aiServer))) {
        freeaddrinfo(aiServer);
        return -1;
    }

    int fdServer = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(fdServer, aiServer->ai_addr, sizeof(struct sockaddr))) {
        freeaddrinfo(aiServer);
        return -1;
    }

    freeaddrinfo(aiServer);

    return fdServer;
}

