CC	= gcc
CFLAGS	= -std=gnu99
LDFLAGS	= -pthread

.PHONY: all
all: client server

client: UDPClient.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^
server: UDPServer.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^
