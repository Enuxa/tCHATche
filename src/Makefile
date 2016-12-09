CC = gcc
INCLUDE_DIR	=	h
CFLAGS =	-std=c11 -Wall -I$(INCLUDE_DIR)
LDFLAGS =
SERVER_EXEC =	tchatche_server
CLIENT_EXEC =	tchatche_client

all: $(SERVER_EXEC)
	${CLIENT_EXEC}

tchatche_server: server.o protocol.o common.o tchatche_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

tchatche_client: client.o protocol.o common.o tchatche_client.o
	$(CC) $(CFLAGS)  $(LDFLAGS) $^ -o $@

tchatche_server.o: h/server.h h/common.h

common.o: protocol.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o *.gch $(SERVER_EXEC)

.PHONY: clean
