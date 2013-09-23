# Data
GCC=gcc -Isrc/
SRCS=src/multiplex.c -lpthread

# Make
example: server client
client:
	mkdir -p bin
	${GCC} -o bin/client.example example/client.c ${SRCS}
server:
	mkdir -p bin
	${GCC} -o bin/server.example example/server.c ${SRCS}
