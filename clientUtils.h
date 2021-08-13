#ifndef CLIENTUTILS_H
#define CLIENTUTILS_H

#include "clientData.h"

void handle_cmd(ClientData *data, char *cmd);
void start_client(ClientData *data);
void end_client(ClientData *data);

#endif
