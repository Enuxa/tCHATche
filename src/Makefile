CC = gcc
INCLUDE_DIR	=	h
CFLAGS =	-Wall -I$(INCLUDE_DIR)
LDFLAGS =
SERVER_EXEC =	tchatche_server

all: $(SERVER_EXEC)

tchatche_server: server.o common.o tchatche_server.o
	$(CC) $(LDFLAGS) $^ -o $@

tchatche_server.o: h/server.h h/common.h
common.o: h/common.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o *.gch $(SERVER_EXEC)

.PHONY: clean
