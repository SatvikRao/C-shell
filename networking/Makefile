CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lcrypto

all: server client

server: server.c network.c
	$(CC) $(CFLAGS) -o server server.c network.c $(LDFLAGS)

client: client.c network.c
	$(CC) $(CFLAGS) -o client client.c network.c $(LDFLAGS)

clean:
	rm -f server client *.o server_log.txt client_log.txt received_file
