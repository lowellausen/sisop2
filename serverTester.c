


#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "dropboxUtils.h"
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <netdb.h>
#include <asm/errno.h>
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

struct packet {
	short int opcode;
	short int seqnum;
	char data [PACKETSIZE - 4];
};

struct sockaddr_in serv_addr;
struct hostent *server;
char host[20];
int port;
int socket_local;
char userID[20];

int receive_file_from(int socket, char* file_name){
	int file;
	struct packet file_packet, reply;
	struct sockaddr_in from;
	unsigned int length = sizeof(struct sockaddr_in);

	file = open(file_name, O_RDWR | O_CREAT, 0666);
	if (file == -1){
		printf("Error! Path: %s\n\n",file_name);
	}
	recvfrom(socket, (char *) &file_packet, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	while(file_packet.opcode == FILEPKT){
		printf("File was %hi for pkt #%hi, expected #%hi\n\n",file_packet.opcode,file_packet.seqnum);
		reply.opcode = ACK;
		reply.seqnum = file_packet.seqnum;
		write(file, file_packet.data, PACKETSIZE - 4);
		sendto(socket, (char *) &reply, PACKETSIZE, 0, (const struct sockaddr *) &from, sizeof(struct sockaddr_in));
		printf("Replied %hi for pkt #%hi\n\n",reply.opcode,reply.seqnum);
		recvfrom(socket, (char *) &file_packet, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	}
	reply.opcode = ACK;
	reply.seqnum = file_packet.seqnum;
	write(file, file_packet.data, (int) file_packet.seqnum);
	sendto(socket, (char *) &reply, PACKETSIZE, 0, (const struct sockaddr *) &from, sizeof(struct sockaddr_in));
	if (close(file) == -1) {
			 printf("Error\n\n");
			 return -1;
	 }
	return 0;
}

int send_file_to(int socket, char* file_name, struct sockaddr destination){
	int file, i = 1, bytes_read = 0;
	struct packet file_packet, reply;
	struct sockaddr_in from;
	unsigned int length = sizeof(struct sockaddr_in);

	file = open(file_name, O_RDONLY);
	if (file <0){
		return -1;
	}
	bytes_read = read(file, file_packet.data, PACKETSIZE - 4);
	while(bytes_read > 0){
		if (bytes_read == PACKETSIZE - 4){
			file_packet.opcode = FILEPKT;
			file_packet.seqnum = (short) i;
		}
		else{
			file_packet.opcode = LASTPKT;
			file_packet.seqnum = (short) bytes_read;
		}
		while(reply.opcode != ACK || reply.seqnum != file_packet.seqnum){
			sendto(socket, (char *) &file_packet, PACKETSIZE, 0, (const struct sockaddr *) &destination, sizeof(struct sockaddr_in));
			recvfrom(socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
			printf("Reply was %hi for pkt #%hi\n\n",reply.opcode,reply.seqnum);
		}
		bytes_read = read(file, file_packet.data, PACKETSIZE - 4);
		printf("Bytes read: %d\n\n", bytes_read);
		i++;
	}
	return 0;
}

int main(int argc,char *argv[]){
  char strporta[100];
  struct packet request, reply;
  struct sockaddr_in from;
  int n, length;

	if (argc!=4){
		printf("Escreva no formato: ./tester <ID_do_usuário> <endereço_do_host> <porta>\n");
	}
	else{ //wrote correcly the arguments
		strcpy(userID,argv[1]);
		strcpy(host,argv[2]);
		strcpy(strporta,argv[3]);
		port = atoi(strporta);
		server = gethostbyname(host);

		if ((socket_local = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			printf("ERROR opening socket");
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
		serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
		bzero(&(serv_addr.sin_zero), 8);
  }

  // Send a login request
	request.opcode = LOGIN;
	strncpy(request.data,userID,20);
	//printf("\nopcode is %hi\n\n",request.opcode);
	//printf("\ndata is %s\n\n",request.data);
	n = sendto(socket_local, (char *) &request, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != ACK){
		printf("Login unsuccesful\n\n");
		return -1;
	}
	printf("Login reply is %hi\n\n",request.opcode);


	// Set new port
	int new_port = (int) request.seqnum;
	printf("New connection port is %d\n\n",new_port);
	server = gethostbyname(host);

	if ((socket_local = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket\n\n");
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(new_port);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);
	sleep(2);
  // Send an upload request
	request.opcode = UPLOAD;
	strcpy(request.data,"doge.jpg\0");
	n = sendto(socket_local, (char *) &request, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != ACK){
		printf("Download ack unsuccesful\n\n");
		return -1;
	}
	printf("Received UPLOAD ACK\n\n");
	// Here will be the actual upload:
	char filename[4096];
	strcpy(filename,"/home/fenris/sisopdois/doge.jpg\0");
	if (send_file_to(socket_local, filename, *((struct sockaddr*) &serv_addr)) == 0){
		printf("Upload succesful\n\n");
	}
	else{
		printf("Upload failed\n\n");
	}

  // Send a download request
	request.opcode = DOWNLOAD;
	strcpy(request.data,"doge.jpg\0");
	n = sendto(socket_local, (char *) &request, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != ACK){
		printf("Download ack unsuccesful\n\n");
		return -1;
	}
	printf("Received DOWNLOAD ACK\n\n");
	// Here will be the actual download:
	strcpy(filename,"/home/fenris/doge.jpg\0");
	receive_file_from(socket_local,filename);

  // Send a delete request
	request.opcode = DELETE;
	n = sendto(socket_local, (char *) &request, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != ACK){
		printf("Delete ack unsuccesful\n\n");
		return -1;
	}
	printf("Received DELETE ACK\n\n");

  // Send a list request
	request.opcode = LIST;
	n = sendto(socket_local, (char *) &request, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != ACK){
		printf("List ack unsuccesful\n\n");
		return -1;
	}
	printf("Received LIST ACK\n\n");
/*
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != LIST){
		printf("List op unsuccesful\n\n");
		return -1;
	}
	printf("List of files is: %s\n\n",request.data);
*/
	// Above: can only be used once I fix List function... lmfao

	printf("Gonna wait a bit before sending CLOSE ;)\n\n");
	sleep(10);

	// Send a close request
	request.opcode = CLOSE;
	n = sendto(socket_local, (char *) &request, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	n = recvfrom(socket_local, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	if (request.opcode != ACK){
		printf("Close unsuccesful\n\n");
		return -1;
	}
	printf("Received CLOSE ACK\n\n");
}
