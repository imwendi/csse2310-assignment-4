#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "serverUtils.h"
#include "commands.h"
#include "clientList.h"
#include "clientThread.h"
#include "lineList.h"
#include "errors.h"

/* 
 * As per the spec, client-handling threads sleep for 100ms or 100000usec in
 * between handling messages
 */
#define CLIENT_SLEEP 100000

/* 
 * The command numbers corresponding to commands a server can receive.
 * This correspond to the outputs of the function get_cmd_no() from commands.h
 */
typedef enum {
    NAME,
    AUTH,
    SAY,
    KICK,
    LIST,
    LEAVE
} ServerCmdNumbers;

/*
 * typedef for server command handling functions.
 * Used to declare the const array handlers below
 */
typedef void (*ServerHandlerFunction)(ClientThreadData *, LineList *);

void name_negotiate(ClientList *clients, ClientThread *client);
void authenticate_client(ClientList *clients, ClientThread *client);
void *client_thread_handler(void *arg);
void handle_cmd(ClientThreadData *data, char *cmd);

void handle_say(ClientThreadData *data, LineList *cmdArgs);
void handle_kick(ClientThreadData *data, LineList *cmdArgs);
void handle_list(ClientThreadData *data, LineList *cmdArgs);
void handle_leave(ClientThreadData *data, LineList *cmdArgs);

/*
 * Array of pointers to functions for handling commands sent to the server by
 * a client connected to it. This functions are used in the threads the server 
 * creates to handle communications with each individual client connected to 
 * it.
 *
 * Each function takes as input a ClientThreadData struct containing data about
 * the client that the thread is handling and a LineList storing the arguments 
 * of a received command.
 *
 * Note that the two NULLs are padding in place of the NAME: and AUTH:
 * commands which are handled separately from the other commands.
 */
const ServerHandlerFunction handlers[] = {
        NULL,
        NULL,
        handle_say,
        handle_kick,
        handle_list,
        handle_leave
        };

/*
 * Given a file descriptor to a new client received from a listening socket,
 * spawns a new thread to handle that client. Then conducts authentication and 
 * name negotiation on that client.
 *
 * Upon success of the above two procedures, adds a ClientNode containing
 * a ClientInstance storing information about the new client to an existing
 * ClientList. (see add_client() in clientList.c).
 *
 * Moreover, ENTER:<name> commands are then sent to all clients and a 
 * "(<name> has entered the chat)" is message emitted to stdout.
 */
void spawn_client_thread(ClientList *clients, int fdClient) {
    // dup() the clients file descriptor to separate read/write fds
    int fdWrite = dup(fdClient);
    FILE *readFrom = fdopen(fdClient, "r");
    FILE *writeTo = fdopen(fdWrite, "w");

    ClientThread *client = init_client_thread(readFrom, writeTo);
    authenticate_client(clients, client);
    if (get_active_status(client)) {
        name_negotiate(clients, client);
    } else {
        free_client_thread(client);
        return;
    }

    add_client(clients, client);
    
    // Create ClientThreadData struct to pass to the client handler thread
    ClientThreadData *data = (ClientThreadData *)
            malloc(sizeof(ClientThreadData));
    data->clients = clients;
    data->client = client;

    pthread_t threadId;
    pthread_create(&threadId, NULL, client_thread_handler, data);
    pthread_detach(threadId);

    // Send ENTER commands and emit stdout message
    char *name = get_printable(client->name);
    send_all_clients(clients, "ENTER:%s", name);
    printf("(%s has entered the chat)\n", name);
    fflush(stdout);
    free(name);
}

/*
 * Function used by client handling server threads to communicate with a client
 *
 * Handles client messages with a 100ms sleep between handling consecutive
 * messages.
 *
 * Upon a client exiting the server, appropriate LEAVE: commands are broadcast 
 * to the other clients and a "(... has left the chat)" message emitted to 
 * stdout. However, this is not done for kicked clients.
 */
void *client_thread_handler(void *arg) {
    toggle_sighup(0, NULL);
    ClientThreadData *data = (ClientThreadData *) arg;
    ClientThread *client = data->client;
    ClientList *clients = data->clients;
    
    while(get_active_status(client)) {
        usleep(CLIENT_SLEEP);
        bool isLineEmpty = false;
        char *clientMsg = read_client_line(client, &isLineEmpty);

        // Deactivate the client if the EOF was read from the client
        if (isLineEmpty) {
            disable_client(client);
            free(clientMsg);
            continue;
        }

        handle_cmd(data, clientMsg);
    }

    // Send LEAVE: message to all clients and emit leaving message to stdout.
    // This is not done for clients with null names (which should not occur
    // except in very edge cases)
    if (client->name != NULL) {
        char *name = get_printable(client->name);
        send_all_clients(clients, "LEAVE:%s", name);
        printf("(%s has left the chat)\n", name);
        fflush(stdout);
        free(name);
    }

    // Free memory allocated to handling the client and remove the client
    // from the linked list of clients
    remove_client(clients, client);
    free(data);

    return 0;
}

/*
 * Handles password authentication of a client.
 *
 * Does nothing if the server requires no authentication, otherwise:
 *
 * Sends an AUTH: command to a client and waits for a reply.
 *
 * If the client responded with an AUTH:<password> command, checks if the
 * password matches that of the server and if so the client is allowed to
 * continue to name negotiation.
 *
 * Otherwise, if the client responded with an incorrected password or any
 * response that wasn't a valid AUTH: command, communications with that client
 * are immediately terminated.
 */
void authenticate_client(ClientList *clients, ClientThread *client) {
    bool authenticated = false;
    // Authentication is successful by default if the server requires no 
    // authentication
    if (clients->password == NULL) {
        authenticated = true;
    } else {
        /*
         * Send AUTH: to the client and get its response
         */
        send_client(client, "AUTH:");
        char *clientReply = read_client_line(client, NULL);
        LineList *cmdArgs = cmd_to_lines(clientReply, SERVER);

        // Check for a valid AUTH: command
        if (cmdArgs != NULL && cmdArgs->numLines > 1 
                && get_cmd_no(cmdArgs->lines[0], SERVER) == AUTH) {
            // Update server stats
            clients->stats[AUTH_COUNT]++;
            // Check password
            if (!strcmp(clients->password, cmdArgs->lines[1])) {
                authenticated = true;
            }
        }

        free_line_list(cmdArgs);
        free(clientReply);
    }

    // Send OK: to the client on successful authentication
    if (authenticated) {
        send_client(client, "OK:");
    } else {
        // Else terminate communications with the client
        disable_client(client);
    }
}

/*
 * Performs name negotiation on a client.
 * Sends WHO: to a client and waits for a NAME:<name> reply.
 * If there isn't already another client in a given list of clients with that
 * name the following things are done:
 * - the client's name is set to the given name
 * - an OK: command is sent to the client
 * - a ENTER:<name> command is sent to all clients in the server where name is
 *   the name that was just set
 * - name negotiation is then ended
 *
 * Otherwise the server sends the NAME_TAKEN: command to the client and the
 * process is repeated.
 *
 * If the client responds with an invalid command (i.e. not a NAME:), then
 * the connection with the client is immediately terminated.
 */
void name_negotiate(ClientList *clients, ClientThread *client) {
    bool isLineEmpty = false;

    while (1) {
        /*
         * Send WHO: to the client then get its response
         */
        send_client(client, "WHO:");
        char *clientReply = read_client_line(client, &isLineEmpty);

        if (isLineEmpty) {
            disable_client(client);
            break;
        }

        LineList *cmdArgs = cmd_to_lines(clientReply, SERVER);
        free(clientReply);

        // Check the client's reply was a valid NAME: command
        if (cmdArgs != NULL && get_cmd_no(cmdArgs->lines[0], SERVER) == NAME) {
            char *name = cmdArgs->lines[1];
            // Update server stats
            clients->stats[NAME_COUNT]++;
            // Check if the given name was empty, and if not, if the name is
            // already taken
            if (cmdArgs->numLines > 1 &&
                    get_client_by_name(clients, name) == NULL) {
                // Set name and end name negotation if the name is not in use
                set_client_name(client, name);
                send_client(client, "OK:");
                free_line_list(cmdArgs);
                break;
            }
            
            send_client(client, "NAME_TAKEN:");
        } else {
            disable_client(client);
            break;
        }

        free_line_list(cmdArgs);
    }
}

/*
 * Handles a command in string form sent to the server by a client.
 *
 * Note that the commands NAME: and AUTH: are handled separately by
 * name_negotiate and authenticate_client
 * respectively.
 *
 * All invalid commands are silently ignored.
 */
void handle_cmd(ClientThreadData *data, char *cmd) {
    LineList *cmdArgs = cmd_to_lines(cmd, SERVER);

    free(cmd);
    if (cmdArgs != NULL) {
        int cmdNo = get_cmd_no(cmdArgs->lines[0], SERVER);
        // Only attempt to handle functions with command numbers greater than
        // one as 0, 1 correspond to NAME: and AUTH: which are not handled in
        // this function
        if (cmdNo > AUTH) {
            handlers[cmdNo](data, cmdArgs);
        }
    } else {
        free_line_list(cmdArgs);
    }
}

/*
 * Handler for the SAY: command from a client given a LineList containing
 * the arguments for that command.
 *
 * Rebroadcasts the message as a MSG:<name>:<contents> command to all clients 
 * in the server where:
 *  - name is the name of the client who sent the command
 *  - contents is the message given with the original SAY: command.
 *
 * The message is also emitted to stdout in the format bob: a message
 *
 * Note that empty message bodies are valid
 */
void handle_say(ClientThreadData *data, LineList *cmdArgs) {
    // Update stats
    data->clients->stats[SAY_COUNT]++;
    data->client->stats[SAY_COUNT]++;

    char *name = get_printable(data->client->name);
    if (cmdArgs->numLines > 1) {
        char *msg = get_printable(cmdArgs->lines[1]);
        send_all_clients(data->clients, "MSG:%s:%s", name, msg);
        printf("%s: %s\n", name, msg);
        free(msg);
    } else {
        send_all_clients(data->clients, "MSG:%s", name);
        printf("%s:\n", name);
    }

    fflush(stdout);
    free(name);
    free_line_list(cmdArgs);
}

/*
 * Handles the KICK:<name> command from a client.
 * If there is a client in the server with the same name as specified by the
 * command arguments, a KICK: command is sent to it and the kicked flag in the
 * ClientThread for that client is set to true.
 *
 * Otherwise, this function does nothing.
 */
void handle_kick(ClientThreadData *data, LineList *cmdArgs) {
    char *name = cmdArgs->lines[1];
    ClientList *clients = data->clients;

    // Update stats
    clients->stats[KICK_COUNT]++;
    data->client->stats[KICK_COUNT]++;

    ClientThread *client = get_client_by_name(clients, name);
    if (client != NULL) {
        send_client(client, "KICK:");
    }

    free_line_list(cmdArgs);
}

/*
 * Handler for the LIST: command from a client given a LineList containing
 * the arguments for that command.
 *
 * Broadcasts the command LIST:<namesLine> to every client where <namesLine> is
 * a comma separated string containing the names of every client in the server
 * as per the given spec.
 *
 * The string "(current chatters: <namesLine>)" is emitted to stdout.
 */
void handle_list(ClientThreadData *data, LineList *cmdArgs) {
    ClientList *clients = data->clients;

    // Update stats
    clients->stats[LIST_COUNT]++;
    data->client->stats[LIST_COUNT]++;

    // Get a LineList of the names of every client
    LineList *names = get_names(clients);
    char *namesLine = (char *) calloc(1, sizeof(char));

    for (int i = 0; i < names->numLines; ++i) {
        // Append each client name to namesLine
        char *name = get_printable(names->lines[i]);
        add_to_string(&namesLine, name);
        free(name);
        // Append a comma for all but the last name
        if (i < (names->numLines - 1)) {
            add_to_string(&namesLine, ",");
        }
    }

    // Emit and send required commands/messages
    send_all_clients(clients, "LIST:%s", namesLine);
    printf("(current chatters: %s)\n", namesLine);
    fflush(stdout);

    free(namesLine);
    free_line_list(names);
    free_line_list(cmdArgs);
}

/*
 * Handler for the LEAVE: command from a client.
 * Just sets the isActive flag of the client being handled to false as sending
 * leave messages etc. are handled after the fact in client_thread_handler
 */
void handle_leave(ClientThreadData *data, LineList *cmdArgs) {
    // Update server stats
    data->clients->stats[LEAVE_COUNT]++;

    disable_client(data->client);
    free_line_list(cmdArgs);
}

/*
 * If mode is 0:
 *      - SIGHUP is ignored/masked on the calling thread.
 *        Note that in this case, the sig argument is ignored.
 *
 * If mode is non-zero:
 *      - sigwait() is called on SIGHUP on the calling thread with the argument
 *        sig passed to sigwait() to return a signal number in
 */
void toggle_sighup(int mode, int *sig) {
    // Create sigset_t containing SIGHUP
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);

    if (mode) {
        sigwait(&set, sig);
    } else {
        pthread_sigmask(SIG_SETMASK, &set, NULL);
    }
}

/*
 * Thread function which waits for the server to be sent a SIGHUP signal.
 * On receiving this signal, it prints server statistics to stderr then waits
 * again for the another SIGHUP.
 */
void *sighup_stats_handler(void *arg) {
    ClientList *clients = (ClientList *) arg;
    int sig;

    while (1) {
        // Block until next sighup
        toggle_sighup(1, &sig);

        char *stats = calloc(1, sizeof(char));

        pthread_mutex_lock(clients->lock);
        add_to_string(&stats, "@CLIENTS@\n");

        ClientNode *currentNode = clients->head;
        while (currentNode != NULL) {
            // Iterate over clients in the server and add their stat lines to
            // stats.
            char *clientStats = client_stat_line(currentNode->client);
            add_to_string(&stats, clientStats);
            free(clientStats);

            currentNode = currentNode->next;
        }

        // Get the server stats
        add_to_string(&stats, "@SERVER@\n");
        char *serverStats = server_stat_line(clients);
        add_to_string(&stats, serverStats);
        free(serverStats);

        pthread_mutex_unlock(clients->lock);

        fprintf(stderr, stats);
        fflush(stderr);
        free(stats);
    }
}
