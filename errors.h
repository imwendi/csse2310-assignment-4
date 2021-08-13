#ifndef ERRORS_H
#define ERRORS_H

/* 
 * Enum containing error codes corresponding to various types of errors clients
 * or servers will exit on.
 */
typedef enum {
    NORMAL,
    USAGE,
    COMMS,
    KICKED,
    FAILED_AUTH
} ExitCodes;

void exit_with_msg(int exitCode, int sentTo);
void suppress_sigpipe();

#endif
