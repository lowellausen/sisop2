#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/queue.h>
#include "dropboxUtils.h"
#include <dirent.h>
#include <netdb.h>
#include <pwd.h>
#include <unistd.h>

#define SOCKET int
#define PACKETSIZE 1250
#define MAIN_PORT 6000
#define MAXCLIENTS 10
#define MAXSESSIONS 2
#define NACK 0
#define ACK 1
#define UPLOAD 2
#define DOWNLOAD 3
#define DELETE 4
#define LIST 5
#define CLOSE 6
#define LOGIN 7
#define FILEPKT 8
#define LASTPKT 9
// Part II
#define PING 10
#define MAXSERVERS 3
#define FRONTEND 999
#define NONE 1000

// Structures
struct file_info {
	char name[MAXNAME];
	char extension[MAXNAME];
	char last_modified[MAXNAME];
	int size;
};

struct client {
	char user_id [MAXNAME];
	int session_active [MAXSESSIONS];
	short int session_port [MAXSESSIONS];
	int socket_set[MAXSESSIONS];
	SOCKET socket[MAXSESSIONS];
};

struct packet {
	short int opcode;
	short int seqnum;
	char data [PACKETSIZE - 4];
};

struct pair {
	int c_id;
	int s_id;
};

struct serverlist{
	int active[MAXSERVERS];
	struct sockaddr_in addr[MAXSERVERS];
};

// G
int session_port, frontend_len, client_len, server_len, primary_id;
SOCKET fe_socket;
struct sockaddr client;
struct sockaddr_in server;
struct sockaddr_in frontend;
struct serverlist serverlist;
struct hostent *serv_addr;

void *getnewleader(){
}

void *session(){
	int i;
	SOCKET session_socket;
	struct sockaddr client;
	struct sockaddr_in session;
	struct packet request, reply;
	int from_len;
	struct sockaddr from;
	int session_len, client_len = sizeof(struct sockaddr_in), active = 1;
	struct timeval tv;
	tv.tv_sec = 100;
	tv.tv_usec = 0;
	// Socket setup
	if((session_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("ERROR: Socket creation failure.\n");
		exit(1);
	}
	memset((void *) &session,0,sizeof(struct sockaddr_in));
	session.sin_family = AF_INET;
	session.sin_addr.s_addr = htonl(INADDR_ANY);
	session.sin_port = htons((int) session_port);
	session_len = sizeof(session);
	if (bind(session_socket,(struct sockaddr *) &session, session_len)) {
		printf("Binding error\n");
		active = 0;
	}
	for (i = 0; i < MAXSERVERS; i++){
		serverlist.active[i] = 0;
	}
	while(active){
		// Wait for a packet from client
		recvfrom(session_socket, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &client, &client_len);
		// Reroute to server
		sendto(session_socket, (char *) &request, PACKETSIZE, 0, (struct sockaddr *)&server, server_len);
		// Wait for a reply packet from server
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		if (setsockopt(session_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			perror("Error");
		}
		recvfrom(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &from, &from_len);
		// Reroute to client
		sendto(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
	}
}

int main(int argc,char *argv[]){
	char host[20];
	int online = TRUE;
	struct packet request, reply;
	int from_len;
	struct sockaddr from;
	pthread_t tid1, tid2;
	// Get server address
	if (argc!=2){
		printf("Escreva no formato: ./frontEnd <endereço_do_server_primario>\n\n");
		return 0;
	}
	strcpy(host,argv[1]);
	serv_addr = gethostbyname(host);
	server.sin_family = AF_INET;
	server.sin_addr = *((struct in_addr *)serv_addr->h_addr);
	server.sin_port = htons(MAIN_PORT);
	server_len = sizeof(server);
	// Socket setup
	if((fe_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("ERROR: Socket creation failure.\n");
		exit(1);
	}
	memset((void *) &frontend,0,sizeof(struct sockaddr_in));
	frontend.sin_family = AF_INET;
	frontend.sin_addr.s_addr = htonl(INADDR_ANY);
	frontend.sin_port = htons(MAIN_PORT);
	frontend_len = sizeof(frontend);
	if (bind(fe_socket,(struct sockaddr *) &frontend, frontend_len)) {
		printf("Binding error\n");
		exit(1);
	}
	printf("Socket initialized, waiting for requests.\n\n");
	// Setup done

	// Login request
	recvfrom(fe_socket, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &client, &client_len);
	// Login reroute
	sendto(fe_socket, (char *) &request, PACKETSIZE, 0, (struct sockaddr *)&server, server_len);
	// Login reply
	recvfrom(fe_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &from, &from_len);
	session_port = reply.seqnum;
	// Login reply reroute to client
	sendto(fe_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
	// call ping
 	pthread_create(&tid1, NULL, getnewleader, NULL);
	// call session
	pthread_create(&tid2, NULL, session, NULL);
	return 0;
}
