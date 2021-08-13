#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "commands.h"
#include "clientThread.h"
#include "clientList.h"
#include "serverUtils.h"
#include "errors.h"

char *setup_server(int argc, char **argv, int *actualPortNo, int *fdListen);
int open_listen(char *port, int *actualPortNo);
void suppress_sigpipe();

int main(int argc, char **argv) {
    toggle_sighup(0, NULL);
    int actualPortNo;
    int fdListen;
    // Check server arguments validity and connect it to the given port
    char *password = setup_server(argc, argv, &actualPortNo, &fdListen);

    // Emit connected port number
    fprintf(stderr, "%d\n", actualPortNo);
    fflush(stderr);
    ClientList *clients = init_client_list();
    set_password(clients, password);

    pthread_t threadId;
    pthread_create(&threadId, NULL, sighup_stats_handler, clients);
    pthread_detach(threadId);

    suppress_sigpipe();

    while (1) {
        struct sockaddr_in fromAddr;
        socklen_t fromAddrSize = sizeof(struct sockaddr_in);
        int fd = accept(fdListen, (struct sockaddr *) &fromAddr,
                &fromAddrSize);
        spawn_client_thread(clients, fd);
    }

    return 0;
}

/*
 * Given the command line arguments with which a server was started, sets up 
 * the server by performing the following actions:
 *
 * - Verifies the validity of arguments and the given authfile;
 *   server is made to exit with communications error if any were invalid
 *
 * - Attempts to connect the server to the specified port and open a listening
 *   socket on that port; server is made to exit with communications error on 
 *   connection failure.
 *
 * - On successful setup, the port number of the connected port and the file 
 *   descriptor of the created listening socket respectively are stored at the 
 *   pointers given by the input arguments actualPortNo and fdListen.
 *
 *   The retrieved password is then returned.
 */
char *setup_server(int argc, char **argv, int *actualPortNo, int *fdListen) {
    
    // Usage error if the number of arguments to the server is incorrect
    if (argc != 2 && argc != 3) {
        exit_with_msg(USAGE, SERVER);
    }

    bool invalidAuthFile = false;

    // Retrieve password from given authfile; usage error on invalid authfile
    char *password = get_password(argv[1], &invalidAuthFile);
    if (invalidAuthFile) {
        exit_with_msg(USAGE, SERVER);
    }

    // Default port is 0 - which gives an ephemeral port
    char *port = "0";
    // Retrive port if given as an argument
    if (argc == 3) {
        port = argv[2];
    }

    // Try connecting to the given port; comms error if connection failed
    if ((*fdListen = open_listen(port, actualPortNo)) < 0) {
        free(password);
        exit_with_msg(COMMS, SERVER);
    }

    return password;
}

/*
 * Create a listening socket and connects it to the given port.
 *
 * On successful connection, the file descriptor to the socket is returned,
 * otherwise -1 is returned.
 *
 * As an ephemeral port might be used, on succesful connection the actual port
 * number connected to is saved to the location of a given int pointer.
 */
int open_listen(char *port, int *actualPortNo) {
    struct addrinfo *aiPort = NULL;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &aiPort)) {
        if (aiPort != NULL) {
            freeaddrinfo(aiPort);
        }
        return -1;
    }

    int fdListen = socket(AF_INET, SOCK_STREAM, 0);

    // Attempt to bind the socket
    if (bind(fdListen, (struct sockaddr *) aiPort->ai_addr,
            sizeof(struct sockaddr))) {
        freeaddrinfo(aiPort);
        return -1;
    }

    freeaddrinfo(aiPort);

    // Configure the socket to listen
    listen(fdListen, SOMAXCONN);

    /*
     * Get the actual port number (as the server may be allocated an epheremal 
     * port) and save its value to the given port string.
     */
    struct sockaddr_in actualAd;
    memset(&actualAd, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    getsockname(fdListen, (struct sockaddr *) &actualAd, &len);
    *actualPortNo = ntohs(actualAd.sin_port);

    return fdListen;
}

