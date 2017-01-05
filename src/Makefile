CC = gcc
INCLUDE_DIR	=	.
CFLAGS =	-std=c11 -Wall -I$(INCLUDE_DIR) -g
LDFLAGS =
SERVER_EXEC =	tchatche_server
CLIENT_EXEC =	tchatche_client

all: tchatche_server tchatche_client

tchatche_server: server/tchatche_server.o common.o protocol.o server/server.o server/server_file.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

tchatche_client: client/tchatche_client.o common.o protocol.o client/client.o client/client_file.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

tchatche_server.o: server/server.o

tchatche_client.o: client/client.o

client/client.o: client/client_file.o common.o

server/server.o: server/server_file.o common.o

common.o: protocol.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o *.gch $(SERVER_EXEC) $(CLIENT_EXEC)

.PHONY: clean
