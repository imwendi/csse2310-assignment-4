#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "lineList.h"
#include "commands.h"
#include "clientData.h"
#include "clientUtils.h"
#include "errors.h"
#include "string.h"

/* Time delay between polling an input stream in usec as used by poll_stream */
#define TIMEOUT 1

/* 
 * Time delay client should wait for between receiving EOF from stdin and 
 * terminating due to this. This is to allow some time for the client to
 * terminate due to the server handling thread (i.e. getting kicked) instead.
 *
 * 50000 usec or 50ms
 */
#define STDIN_EOF_DELAY 50000

/*
 * The command numbers corresponding to commands a client can receive.
 * This corresponds to the outputs of the function get_cmd_no() from
 * commands.h
 */
typedef enum {
    WHO,
    NAME_TAKEN,
    AUTH,
    OK,
    KICK,
    LIST,
    MSG,
    ENTER,
    LEAVE
} ClientCmdNumbers;

/* 
 * typedef for client command handling functions, used to declare the const 
 * array handlers below
 */
typedef void (*ClientHandlerFunction)(ClientData *, LineList *);

void handle_unused(ClientData *data, LineList *cmdArgs);
void handle_kick(ClientData *data, LineList *cmdArgs);
void handle_list(ClientData *data, LineList *cmdArgs);
void handle_msg(ClientData *data, LineList *cmdArgs);
void handle_enter(ClientData *data, LineList *cmdArgs);
void handle_leave(ClientData *data, LineList *cmdArgs);

/*
 * Array of pointers to functions for handling commands sent to the client
 * with no input arguments.
 *
 * Each function takes as input arguments a ClientData struct storing data
 * about the current client and a LineList storing the arguments of a
 * received command.
 */
const ClientHandlerFunction handlers[] = {
        handle_unused,
        handle_unused,
        handle_unused,
        handle_unused,
        handle_kick,
        handle_list,
        handle_msg,
        handle_enter,
        handle_leave
        };

/*
 * Handles a command in string form sent to a client by a server.
 * All invalid commands are silently ignored.
 */
void handle_cmd(ClientData *data, char *cmd) {
    LineList *cmdArgs = cmd_to_lines(cmd, CLIENT);

    free(cmd);
    if (cmdArgs != NULL) {
        int cmdNo = get_cmd_no(cmdArgs->lines[0], CLIENT);
        handlers[cmdNo](data, cmdArgs);
    }
}

/*
 * Handler for a server's authentication challenge.
 * Reads server input until an AUTH: command is issued.
 *
 * Client sents AUTH:password to the server where password is that specified in
 * the client's authfile.
 *
 * Then waits for server to reply. If the reply was "OK:", the client may
 * continue with regular operation. Else if the server terminates its
 * connection with the client and the client reads EOF from the server, the
 * client will terminate too with an appropriate exit code.
 */
void authenticate_client(ClientData *data) {
    bool authorized = false;
    bool isLastLine = false;

    while (!authorized) {
        char *serverMsg = read_server_line(data, &isLastLine);
        LineList *cmdArgs = cmd_to_lines(serverMsg, CLIENT);

        if (isLastLine) {
            // Comms error if server disconnects
            free(serverMsg);
            free_line_list(cmdArgs);
            disable_client(data, COMMS);
            break;
        } else if (cmdArgs == NULL || 
                get_cmd_no(cmdArgs->lines[0], CLIENT) != AUTH) {
            // Ignore messages that aren't AUTH:
            free(serverMsg);
            free_line_list(cmdArgs);
            continue;
        }

        send_to_server(data, "AUTH:%s", data->password);
        free(serverMsg);
        free_line_list(cmdArgs);
        
        serverMsg = read_server_line(data, &isLastLine);
        // Check if the server responsed with "OK:"
        if (strcmp(serverMsg, "OK:") || isLastLine) {
            free(serverMsg);
            disable_client(data, FAILED_AUTH);
            break;
        } else {
            // Authentication complete
            free(serverMsg);
            break;
        }
    }
}

/*
 * Performs name negotiation with a server.
 * On receiving a WHO: command, the client responds with a NAME:clientName
 * command.
 *
 * If the server responds with an OK:, name negotation is complete and the
 * client will proceed to handle other messages from the server and user input
 * via stdin.
 *
 * If the server responded with NAME_TAKEN:, the client increments its clientNo
 * and awaits another WHO: message from the server to repeat the above process.
 *
 * Invalid/unexpected responses from the server are ignored.
 *
 * If the server disconnects during this process, the name negotiation loop
 * is broken. As the client's authenticated flag is never set to true if this
 * happens, the client should then proceed to terminate.
 */
void name_negotiate(ClientData *data) {
    bool isLineEmpty = false;

    while (!data->authenticated && !isLineEmpty) {
        char *serverMsg = read_server_line(data, &isLineEmpty);
        LineList *cmdArgs = cmd_to_lines(serverMsg, CLIENT);

        // Check if the server sent WHO: and respond with the client's name
        if (cmdArgs != NULL && get_cmd_no(cmdArgs->lines[0], CLIENT) == WHO) {
            send_to_server(data, "NAME:%s", get_name(data));
            free(serverMsg);
            free_line_list(cmdArgs);

            // Get the server's next reply
            serverMsg = read_server_line(data, &isLineEmpty);
            cmdArgs = cmd_to_lines(serverMsg, CLIENT);

            if (cmdArgs == NULL) {
                free(serverMsg);
                continue;
            }

            // Check if the reply was OK: or NAME_TAKEN:
            int cmdNo = get_cmd_no(cmdArgs->lines[0], CLIENT);

            switch (cmdNo) {
                case OK:
                    // Naming is complete
                    data->authenticated = true;
                    break;
                case NAME_TAKEN:
                    // Increment client number
                    next_client_no(data);
                    break;
            }

            free(serverMsg);
            free_line_list(cmdArgs);
        }
    }

    // If EOF was sent by the server during name negotiation, the server has
    // disappeared so disable the client
    if (isLineEmpty) {
        disable_client(data, COMMS);
    }
}

/*
 * Handler for the WHO:, NAME_TAKEN:, AUTH: and OK: commands if they are sent
 * by the server AFTER authentication and name negotiation are complete.
 * 
 * This function does nothing except free the LineList of
 * arguments it is given.
 */
void handle_unused(ClientData *data, LineList *cmdArgs) {
    free_line_list(cmdArgs);
}

/*
 * Handler for the KICK: command received from a server.
 * Calls disable_client to disable the client's isActive flag and set its exit
 * code to be that for kicked clients. (See disable_client() in clientData.c)
 */
void handle_kick(ClientData *data, LineList *cmdArgs) {
    free_line_list(cmdArgs);
    disable_client(data, KICKED);
}

/*
 * Handler for the LIST: command from a server given a LineList containing
 * the given arguments for that command.
 *
 * Emits "(current chatters: <list of names>)" to stdout where list of names
 * is the string listing all clients in the server as specified by the given 
 * command arguments.
 */
void handle_list(ClientData *data, LineList *cmdArgs) {
    printf("(current chatters: %s)\n", cmdArgs->lines[1]);
    fflush(stdout);
    free_line_list(cmdArgs);
}

/*
 * Handler for the MSG: command from a server given a LineList containing
 * the given arguments for that command.
 *
 * Emits "<name>: <msg>" to stdout where name and msg are the name of the 
 * messenger sender and the sent message as specified by the given command
 * arguments.
 *
 * Note that empty message bodies are valid.
 */
void handle_msg(ClientData *data, LineList *cmdArgs) {
    char *name = cmdArgs->lines[1];

    // Check for an empty message body
    if (cmdArgs->numLines > 1) {
        char *msg = cmdArgs->lines[2];
        printf("%s: %s\n", name, msg);
    } else {
        printf("%s:\n", name);
    }
    fflush(stdout);
    free_line_list(cmdArgs);
}

/*
 * Hander for the ENTER: command from a server given a LineList containing
 * the given arguments for that command.
 *
 * Emits "(<name> has entered the chat)" to stdout where name is the name of 
 * the entering client as specified by the given command arguments.
 */
void handle_enter(ClientData *data, LineList *cmdArgs) {
    printf("(%s has entered the chat)\n", cmdArgs->lines[1]);
    fflush(stdout);
    free_line_list(cmdArgs);
}

/*
 * Hander for the LEAVE: command from a server given a LineList containing
 * the given arguments for that command.
 *
 * Emits "(<name> has left the chat)" to stdout where name is the name of 
 * the leaving client as specified by the given command arguments.
 */
void handle_leave(ClientData *data, LineList *cmdArgs) {
    printf("(%s has left the chat)\n", cmdArgs->lines[1]);
    fflush(stdout);
    free_line_list(cmdArgs);
}

/*
 * Uses select to poll a single given file descriptor and returns a non-zero 
 * int if the stream at that fd is ready to be read. Otherwise 0 is returned.
 *
 * Note that this should be called in a loop that is constantly checking for
 * input in order for select() to function correctly.
 *
 * (See server_comms_handler() or user_comms_handler() for usage)
 */
int poll_stream(int fdStream) {
    fd_set fds;
    struct timeval tv;
    tv.tv_usec = TIMEOUT;
    tv.tv_sec = 0;

    FD_ZERO(&fds);
    FD_SET(fdStream, &fds);
    select(fdStream + 1, &fds, NULL, NULL, &tv);

    return FD_ISSET(fdStream, &fds);
}

/*
 * Thread function used by the thread for handling server communications.
 * Repeatedly handles server messages until either the server disconnected or 
 * the isActive flag of the current client is toggled to false, i.e. by a 
 * handle server command, stdin ending, server disconnecting etc.
 */
void *server_comms_handler(void *arg) {
    ClientData *data = (ClientData *) arg;

    authenticate_client(data);
    if (data->isActive) {
        name_negotiate(data);
    }

    while (data->isActive) {
        if (poll_stream(fileno(data->readFrom))) {
            bool serverLeft = false;
            char *serverMsg = read_server_line(data, &serverLeft);
           
            // Break if the last read_server_line call detected that the server
            // is no longer active (see read_server_line in clientData.c)
            if (serverLeft && data->isActive) {
                free(serverMsg);
                disable_client(data, COMMS);
                break;
            }
            handle_cmd(data, serverMsg);
        }
    }

    return 0;
}

/*
 * Sends a given message to the server a client is connected to based on given
 * rules in the specification, i.e.:
 *
 * - Messages starting with an asterisk are considered to be commands and sent
 *   to the client. i.e. User input "*LIST:" will send "LIST:" to the server.
 *
 * - If a user enters "*LEAVE:", the corresponding command is sent to the
 *   server and the client exits normally
 *
 * - Any other commands are interpreted as messages, i.e. "SAY:" is appended to
 *   the start of the message and sent to the server.
 */
void send_user_msg(ClientData *data, char *msg) {
    // Check if the message is *LEAVE:
    int isLeave = strcmp(msg, "*LEAVE:");

    // Check if the message is a command
    if (msg[0] == '*') {
        send_to_server(data, msg + 1);
    } else {
        // Otherwise send the message with a SAY:
        send_to_server(data, "SAY:%s", msg);
    }

    free(msg);
    
    // Disable the client if user inputs *LEAVE:
    if (!isLeave) {
        disable_client(data, NORMAL);
        // Emit leave message
        printf("(%s has left the chat)\n", data->name);
        fflush(stdout);
    }
}

/*
 * Thread function for handling input from the user from stdin.
 * 
 * Repeatedly handles lines from stdin whilst the client has not been
 * terminated (its isActive flag is set).
 *
 * Note that no input handling is done until the server handling thread has
 * finished authentication and name negotiation.
 */
void *user_input_handler(void *arg) {
    ClientData *data = (ClientData *) arg;
    bool isLineEmpty = false;
    char *userMsg;
 
    // Block until the client has completed authentication and name negotiation
    // or it is deactivated on failing either of the two
    while(!(data->authenticated) && data->isActive) {
        ;
    }

    while (data->isActive && !isLineEmpty) {
        if (poll_stream(fileno(stdin))) {
            userMsg = read_line_stdin(&isLineEmpty);
            if (!isLineEmpty && data->isActive) {
                send_user_msg(data, userMsg);
            }
        }
    }

    if (isLineEmpty) {
        // Delay to allow the client a chance to terminate due to server input
        // (This allowed for more consistent passing of the given unit tests)
        usleep(STDIN_EOF_DELAY);
        disable_client(data, 0);
    }

    return 0;
}

/*
 * Creates threads to handle server and user input and stores the two pthread
 * ids to the given ClientData struct.
 */
void start_client(ClientData *data) {
    pthread_create(&(data->serverHandler), NULL, server_comms_handler, data);
    pthread_create(&(data->userHandler), NULL, user_input_handler, data);
}

/*
 * Ends a client program.
 * Performs the following tasks:
 * - Join threads handling server and user input
 * - Free the client's data
 * - Exit with the exit code specified by the client's data on terminating
 */
void end_client(ClientData *data) {
    pthread_mutex_lock(data->lock);
    pthread_join(data->serverHandler, NULL);
    pthread_join(data->userHandler, NULL);
    pthread_mutex_unlock(data->lock);

    int exitCode = data->exitCode;
    free_client_data(data);
    exit_with_msg(exitCode, CLIENT);
}
