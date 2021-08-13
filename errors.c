#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "commands.h"

/* Possible exit messages for the client program. */
const char *clientExitMessages[] = {
        NULL,
        "Usage: client name authfile port",
        "Communications error",
        "Kicked",
        "Authentication error"
        };

/* Possible exit messages for thr server program. */
const char *serverExitMessages[] = {
        NULL,
        "Usage: server authfile [port]",
        "Communications error"
        };

/* Possible exit messages for the client and server programs */
const char **serverMessages[] = {clientExitMessages, serverExitMessages};

/*
 * Terminates a server or client with a given errorcode. 
 * If the error code was < 0, then the error code is not properly set and the
 * server/client exits with 0 and no error message.
 *
 * Otherwise, the server/client terminates with the given error code and emits
 * the corresponding error message to stderr.
 */
void exit_with_msg(int exitCode, int sentTo) {
    if (exitCode < 0) {
        exit(0);
    } else {
        const char *errorMsg = serverMessages[sentTo][exitCode];
        if (errorMsg != NULL) {
            fprintf(stderr, "%s\n", serverMessages[sentTo][exitCode]);
            fflush(stderr);
        }

        exit(exitCode);
    }
}

/* Creates a new sigaction struct whose handler is SIG_IGN and sets this as
 * the handler for SIGPIPE to suppress it*/
void suppress_sigpipe() {
    struct sigaction ignoreSignal;
    memset(&ignoreSignal, 0, sizeof(struct sigaction));
    ignoreSignal.sa_handler = SIG_IGN;
    ignoreSignal.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &ignoreSignal, 0);
}


