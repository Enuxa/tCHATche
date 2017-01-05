CC = gcc
INCLUDE_DIR	=	.
CFLAGS =	-std=c11 -Wall -I$(INCLUDE_DIR) -g
LDFLAGS =
SERVER_EXEC =	tchatche_server
CLIENT_EXEC =	tchatche_client

all: tchatche_server tchatche_client

tchatche_server: server/server.o protocol.o common.o server/tchatche_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

tchatche_client: client/client.o protocol.o common.o client/tchatche_client.o
	$(CC) $(CFLAGS)  $(LDFLAGS) $^ -o $@

tchatche_server.o: server/server.h common.h

tchatche_client.o: client/client.h common.h

common.o: protocol.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o *.gch $(SERVER_EXEC) $(CLIENT_EXEC)

.PHONY: clean
