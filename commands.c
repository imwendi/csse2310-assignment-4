#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "commands.h"

/* Command number of SAY: command as per get_cmd_no() */
#define SAY 2
/* Command number of MSG: command as per get_cmd_no() */
#define MSG 6

/* Strings corresponding to commands that can be sent to a client */
const char *clientCmdWords[] = {
        "WHO",
        "NAME_TAKEN",
        "AUTH",
        "OK",
        "KICK",
        "LIST",
        "MSG",
        "ENTER",
        "LEAVE"
        };

/* 
 * Max valid number of arguments per command corresponding to each respective
 * command in clientCmdWords.
 */
const int maxClientCmdLengths[] = {1, 1, 1, 1, 1, 2, 3, 2, 2};

/*
 * Minimum valid number of valid arguments per command corresponding to each
 * respective command in clientCmdWords.
 */
const int minClientCmdLengths[] = {1, 1, 1, 1, 1, 1, 2, 2, 2};

/* Strings corresponding to commands that can be sent to a server.
 * "NAME" is not included here as name negotiation is handled separately
 * from the other commands. (see server.c)*/
const char *serverCmdWords[] = {
        "NAME",
        "AUTH",
        "SAY",
        "KICK",
        "LIST",
        "LEAVE"
        };

/*
 * Max valid number of arguments per command corresponding to each respective
 * command in serverCmdWords
 */
const int maxServerCmdLengths[] = {2, 2, 2, 2, 1, 1};

/*
 * Minimum valid number of arguments per command corresponding to each
 * respective command in serverCmdWords
 */
const int minServerCmdLengths[] = {1, 1, 1, 2, 1, 1};

/* Number of possible commands for client and server respectively*/
const int cmdCount[] = {9, 6};

/* 
 * Array of arrays containing valid command words that can be sent to client 
 * and server respectively
 */
const char **cmdWords[] = {clientCmdWords, serverCmdWords};

/*
 * Array of arrays containing the max valid number of arguments for client and
 * server commands respectively.
 */
const int *maxCmdLengths[] = {maxClientCmdLengths, maxServerCmdLengths};

/*
 * Array of arrays containing the minimum valid number of arguments for client
 * and server commands respectively.
 */
const int *minCmdLengths[] = {minClientCmdLengths, minServerCmdLengths};

/* 
 * Converts command cmd given as a string to the index of that string
 * in either clientCmdWords or serverCmdWords if it is in the array.
 *
 * If sentTo is 0, the string is looked for in clientCmdWords whilst if it 
 * is 1 it is looked for in serverCmdWords.
 *
 * If the index is found it is returned, else -1 is returned.
 *
 * (This function is adapted from my A3 submission)
 */
int get_cmd_no(char *cmd, int sentTo) {
    // Default value is -1, no command matched
    int matchedCmd = -1;
   
    // Check for empty command
    if (cmd == NULL || strlen(cmd) == 0) {
        return matchedCmd;
    }

    for (int i = 0; i < cmdCount[sentTo]; ++i) {
        if (!strcmp(cmd, cmdWords[sentTo][i])) {
            matchedCmd = i;
            break;
        }
    }

    return matchedCmd;
}

/*
 * Returns a LineList representation of the arguments of a command given the 
 * command as a string, where each argument delimited by ':' is saved as a 
 * separate entry in the LineList.
 *
 * If a pointer to a bool flag is given (i.e. not NULL), then
 * sets the flag (*invalidCmd) to true if the command is missing a terminating
 * colon where required.
 *
 * (This function is adapted from my A3 submission)
 */
LineList *get_cmd_args(char *cmd, bool *invalidCmd, int sentTo) {
    // Make a copy of cmd for strtok to work on as strtok modifies strings
    char *cmdCopy = calloc(strlen(cmd) + 1, sizeof(char));
    strcpy(cmdCopy, cmd);
    LineList *cmdLines = init_line_list();

    // reference is used by strtok_r internally for thread safety
    char *reference;
    char *token = strtok_r(cmdCopy, ":", &reference);
    int cmdNo = get_cmd_no(token, sentTo);
    int maxCmdArgs;

    // Ensure first token is a valid command
    if (cmdNo < 0) {
        free(cmdCopy);
        return cmdLines;
    } else {
        maxCmdArgs = maxCmdLengths[sentTo][cmdNo];
    }

    int argCount = 1;
    while (token != NULL) {
        add_to_lines(cmdLines, token);
        // Break when enough arguments have been read
        if (++argCount >= maxCmdArgs) {
            break;
        }
        token = strtok_r(NULL, ":", &reference);
    }

    // Retrieve the part of cmdCopy not operated on by strtok
    if ((token = strtok_r(NULL, "", &reference)) != NULL) {
        add_to_lines(cmdLines, token);
    }
    free(cmdCopy);

    // Check the command has valid format
    if (invalidCmd != NULL) {
        if ((cmdLines->numLines < maxCmdLengths[sentTo][cmdNo]) &&
                (cmd[strlen(cmd) - 1] != ':')) {
        // Command invalid if of minimum valid length and doesn't end in ":"
            *invalidCmd = true;
        }
    }

    return cmdLines;
}

/*
 * Given a command in string format, converts it to a LineList representation
 * using get_cmd_str(). Also checks if the arguments given are valid for 
 * the command specified. (e.g. MSG: can have empty second argument whilst
 * other commands may not)
 *
 * If sentTo is 0, the command is checked if it is a valid command a client can
 * receive. If 1, it's check if it's a valid command a server can receive.
 *
 * Returns the LineList representation of the command if its number of
 * arguments is valid, else returns NULL.
 */
LineList *cmd_to_lines(char *cmd, int sentTo) {
    // Ignore empty commands
    if (cmd == NULL || strlen(cmd) < 1) {
        return NULL;
    }

    bool invalidCmd = false;
    LineList *parsedCmd = get_cmd_args(cmd, &invalidCmd, sentTo);

    // Return null if the command was empty
    if (parsedCmd->numLines < 1) {
        free_line_list(parsedCmd);
        return NULL;
    }

    int cmdNo = get_cmd_no(parsedCmd->lines[0], sentTo);

    if ((sentTo == SERVER && cmdNo != SAY) ||
            (sentTo == CLIENT && cmdNo != MSG)) {
        // Check commands apart from SAY: and MSG: do not have additional
        // colons or more than expected arguments
        if (pattern_match_string(":",
                parsedCmd->lines[parsedCmd->numLines - 1]) ||
                parsedCmd->numLines > maxCmdLengths[sentTo][cmdNo]) {
            invalidCmd = true;
        }
    }

    // Check validity of the command
    if (invalidCmd || cmdNo < 0 ||
            parsedCmd->numLines < minCmdLengths[sentTo][cmdNo]) {
        free_line_list(parsedCmd);
        return NULL;
    }

    return parsedCmd;
}

/*
 * Given a file path to an authfile, returns the string password stored in the
 * authfile if the authfile is formatted correctly. A bool flag is set to true
 * if the authfile is invalid.
 *
 * If the authfile is empty:
 *      Returns null, does not set the flag.
 *
 * If the authfile is correct, i.e. 1 line containing password only:
 *      Returns the password as a string, does not set the flag
 *
 * If the authfile could not be opened or it has more than 1 line:
 *      Returns null and sets the flag to true.
 */
char *get_password(char *authPath, bool *invalidAuthFile) {
    char *password = NULL;

    FILE *auth;
    if ((auth = fopen(authPath, "r")) != NULL) {
        LineList *authLines = file_to_lines(auth);
        fclose(auth);

        int numLines = authLines->numLines;
        if (numLines > 0) {
            password = calloc(strlen(authLines->lines[0]) + 1,
                    sizeof(char));
            strcpy(password, authLines->lines[0]);
            free_line_list(authLines);
        }
    } else {
        *invalidAuthFile = true;
    }

    return password;
}
