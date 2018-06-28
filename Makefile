both: client server front


client: dropboxUtils.o dropboxClient.o 
	gcc -o dropboxClient dropboxUtils.o dropboxClient.o -lpthread 
server: dropboxUtils.o dropboxServer.o 
	gcc -o dropboxServer dropboxUtils.o dropboxServer.o -lpthread
front: frontEnd.o
	gcc -o frontEnd frontEnd.o -lpthread
frontEnd.o: frontEnd.c
	gcc -c frontEnd.c
dropboxUtils.o: dropboxUtils.c
	gcc -c dropboxUtils.c
dropboxClient.o: dropboxClient.c
	gcc -c dropboxClient.c
dropboxServer.o: dropboxServer.c
	gcc -c dropboxServer.c

clean:
	rm dropboxClient.o dropboxServer.o dropboxUtils.o dropboxClient dropboxServer
