all:server
CFLAGS = -g -pthread -O0
cc = gcc
server : http_srv.o
	$(CC) $(CFLAGS) -o server http_srv.o
	
httpserver.o : http_srv.c
	$(CC) $(CFLAGS) -c http_srv.c
	
clean:
	rm -rf server*.o
