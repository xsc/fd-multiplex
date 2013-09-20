example: server client
client:
	mkdir -p bin
	gcc -o bin/client.example -Isrc/ example/client.c src/multiplex.c

server:
	mkdir -p bin
	gcc -o bin/server.example -Isrc/ example/server.c src/multiplex.c
