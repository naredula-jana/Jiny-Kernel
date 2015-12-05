SOURCES=main.c common.c 
SOURCES+=shm.c fd_list.c stat.c vring.c
SOURCES+=server.c vhost_server.c

HEADERS=common.h
HEADERS+=shm.h fd_list.h stat.h vring.h
HEADERS+=server.h vhost_server.h
HEADERS+=packet.h

BIN=vhost
CFLAGS += -Wall -Werror
CFLAGS += -ggdb
LFLAGS = -lrt

all: ${BIN}

${BIN}: ${SOURCES} ${HEADERS}
		${CC} ${CFLAGS} ${SOURCES} -static -o $@ ${LFLAGS} -lpthread

clean:
		rm -rf ${BIN}

