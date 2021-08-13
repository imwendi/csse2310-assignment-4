CC = gcc
CFLAGS = -Wall -pedantic -pthread --std=gnu99 -g
SERVER_OBJS = server.o clientThread.o clientList.o serverUtils.o lineList.o errors.o commands.o
CLIENT_OBJS = client.o clientUtils.o clientData.o commands.o lineList.o errors.o
.PHONY: all clean
.DEFAULT_GOAL := all

all : server client

clean :
	rm server client *.o

# Compile the server
server : $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile the client
client : $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Pattern rule for compiling .o objects given .c files
%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

# Dependency rules
server.o: clientList.h clientThread.h
client.o: clientData.h lineList.h
clientUtils.o: clientUtils.h commands.h lineList.h
clientData.o : clientData.h lineList.h errors.h
clientList.o: clientList.h clientThread.h
clientThread.o: clientThread.h lineList.h
serverUtils.o: serverUtils.h clientList.h clientThread.h commands.h
commands.o: commands.h lineList.h
lineList.o : lineList.h
errors.o : errors.h
