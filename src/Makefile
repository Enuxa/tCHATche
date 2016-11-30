COMPILER	=	gcc
INCLUDE_DIR	=	h
OPTIONS		=	 -I${INCLUDE_DIR}
SERVER_EXEC =	tchatche_server

clean:
	rm -Rf *.o

all: tchatche_server

tchatche_server: server.o
	${COMPILER} ${OPTIONS} ${SERVER_EXEC}.c -o ${SERVER_EXEC} server.o

server.o: server.c common.o
	${COMPILER} ${OPTIONS} server.c -c common.o

common.o:
	${COMPILER} ${OPTIONS} common.c -c
