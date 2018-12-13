#Makefile

all: client server

client: server.c
	gcc client.c -o client

server: server.c
	gcc server.c -lpthread -o server

clean: 
	rm -f client server
