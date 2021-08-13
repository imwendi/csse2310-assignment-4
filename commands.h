#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include "lineList.h"
#include "stdbool.h"

/*
 * Enum respresenting possible locations to which commands are sent,
 * for use as the sentTo variable passed into get_cmd_master.
 * 0 corresponds to client programs, 1 corresponds to server
 */
typedef enum {
    CLIENT, SERVER
} CmdSentTo;

LineList *get_cmd_args(char *cmd, bool *invalidCmd, int sentTo);
int get_cmd_no(char *cmd, int sentTo);
LineList *cmd_to_lines(char *cmd, int sentTo);
char *get_password(char *authPath, bool *invalidAuthFile);

#endif
