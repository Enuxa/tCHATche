COMPILER	=	gcc
INCLUDE_DIR	=	h
OPTIONS		=	-Wall -I${INCLUDE_DIR}

clean:
	rm -Rf *.o

all: tchatche_server

tchatche_server: server.o
	${COMPILER} ${OPTIONS} $@$*.c -o $* $^

server.o: common.o
	${COMPILER} ${OPTIONS} $@$*.c -c $^

common.o:
	${COMPILER} ${OPTIONS} $@$*.c -c $^
