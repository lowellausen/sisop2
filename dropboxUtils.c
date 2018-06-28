#ifndef DROPBOXUTILS_C
#define DROPBOXUTILS_C

#include"dropboxUtils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <asm/errno.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>

#define TRUE 1
#define FALSE 0
#define PACKETSIZE 1250
#define SOCKET int
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
#define PING 10

struct packet {
	short int opcode;
	short int seqnum;
	char data [PACKETSIZE - 4];
};

struct file_info{
	char name[MAXNAME];
	char extension[MAXNAME];
	char last_modified[MAXNAME];
	int size;
};

struct client{
	int devices[2];
	char userid[MAXNAME];
	struct file_info fi[MAXFILES];
	int logged_in;
};

void removeBlank(char * filename){
	char aux[200]; int i=0; int j=0;

	const char *homedir;

	if ((homedir = getenv("HOME")) == NULL) {
	    homedir = getpwuid(getuid())->pw_dir;
	}

	strcpy(aux,homedir);
	while(aux[j]!='\0')
		j++;

	while((filename[i]!='\0')&&(filename[i]!='\n')){
		if ((filename[i]!=' ')&&(filename[i]!='~')){
			aux[j] = filename[i];
			j++;
		}
		i++;
	}
	aux[j]= '\0';
	strcpy(filename,aux);
}

int create_home_dir(char *userID){
	//cria diretório com nome do user no HOME do user, chamado pelo cliente
	char *path = malloc((strlen(userID)+15)*sizeof(char));
	//strcpy(dir, "mkdir ~/");
	strcpy(path, "~/sync_dir_");
	strcat(path, userID);    //forma o path utilizando o user id

	int ret=0;

	char *syscmd = malloc((((strlen(userID)+15)*2)+50)*sizeof(char));

	strcpy(syscmd, "if [ ! -d ");   //monta o comando em bash para cria o dir caso ele não excista
	strcat(syscmd, path);
	strcat(syscmd, " ];then\n mkdir ");
	strcat(syscmd, path);
	strcat(syscmd, "\nfi");

	ret = system(syscmd);  //chama o comando
	free(syscmd);

	free(path);
	return ret;
}

int create_home_dir_server(char *userID){
	//cria diretório com nome do user no HOME do user, chamado pelo cliente
	char *path = malloc((strlen(userID)+30)*sizeof(char));
	//strcpy(dir, "mkdir ~/");
	strcpy(path, "~/dropboxserver/sync_dir_");
	strcat(path, userID);

	int ret=0;

	char *syscmd = malloc((((strlen(userID)+23)*2)+50)*sizeof(char));

	strcpy(syscmd, "if [ ! -d ");   //monta o comando em bash para cria o dir caso ele não excista
	strcat(syscmd, path);
	strcat(syscmd, " ];then\n mkdir ");
	strcat(syscmd, path);
	strcat(syscmd, "\nfi");
	ret = system(syscmd);

	free(syscmd);

	free(path);
	return ret;
}

char * getArgument(char* command){
	char* argument;
	int i=0; int j=0;
	argument = (char*) malloc(sizeof(char)*100);
	while(command[i]!=' ')
		i++;
	while(command[i]==' ')
		i++;
	while(command[i]!=' '&& command[i]!='\0' && command[i]!='\n'){
		argument[j]= command[i];
		i++;
		j++;
	}
	argument[j] = '\0';
	return argument;
}
char * getSecondArgument(char* command){
	char* argument;
	int i=0; int j=0;
	argument = (char*) malloc(sizeof(char)*100);
	while(command[i]!=' ')
		i++;
	while(command[i]==' ')
		i++;
	while(command[i]!=' ')
		i++;
	while(command[i]==' ')
		i++;
	while(command[i]!=' '&& command[i]!='\0' && command[i]!='\n'){
		argument[j]= command[i];
		i++;
		j++;
	}
	argument[j] = '\0';
	return argument;
}

int create_server_root(){
	//cria diretório raiz do servidor na home da máquina, caso não exista ainda
	char *path = malloc(15*sizeof(char));
	strcpy(path, "~/dropboxserver");
	DIR *dir = opendir(path);
	int ret=0;

	char *syscmd = malloc(65*sizeof(char));

	strcpy(syscmd, "if [ ! -d ");   //monta o comando em bash para cria o dir caso ele não excista
	strcat(syscmd, path);
	strcat(syscmd, " ];then\n mkdir ");
	strcat(syscmd, path);
	strcat(syscmd, "\nfi");
	ret = system(syscmd);
	printf("Testing directory creation: %s\n\n", path);
	free(path);
	return ret;
}

int create_server_userdir(char *userID){
	//cria diretório com nome do user no na raiz do servidor
	char *path = malloc((strlen(userID)+17)*sizeof(char));
	strcpy(path, "~/dropboxserver/");
	strcat(path, userID);

	int ret=0;

	char *syscmd = malloc((((strlen(userID)+16)*2)+50)*sizeof(char));

	strcpy(syscmd, "if [ ! -d ");   //monta o comando em bash para cria o dir caso ele não excista
	strcat(syscmd, path);
	strcat(syscmd, " ];then\n mkdir ");
	strcat(syscmd, path);
	strcat(syscmd, "\nfi");
	ret = system(syscmd);

	free(path);
	return ret;
}

int receive_int_from(int socket){
	//retorn número de op recebido no socket socket, ou -1(op inválida) caso erro
	int n;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	clilen = sizeof(struct sockaddr_in);

	//char *buf = malloc(sizeof(char)); //recebe um char apenas representando um número de op no intervalo [1...algo]
	int receivedInt = 0;

	n = recvfrom(socket,(char*) &receivedInt, sizeof(int), 0, (struct sockaddr *) &cli_addr, &clilen);
	if (n < 0)
		return -1;

	//retorna o ack
	n = sendto(socket, "ACK", 3, 0,(struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
	if (n  < 0)
		return -1;

	//int ret = atoi(buf);
	//free(buf);
	return ntohl(receivedInt);
}

int send_int_to(int socket, int op){
	//envia uma op como um inteiro, -1 se falha e  0  se sucesso
	int n;
	struct sockaddr_in serv_addr, from;
	//char *buf = malloc(sizeof(char)); //op tem apenas um char/dígito
	//sprintf(buf, "%d", op); //conversão de op para a string buf

	int sendInt = htonl(op);

	n = sendto(socket, (char*) &sendInt, sizeof(int), 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	if (n < 0)
		return -1;

	//free(buf);
	char *buf = malloc(sizeof(char)*3); //para receber o ack

	unsigned int length = sizeof(struct sockaddr_in);
	n = recvfrom(socket, buf, 3*sizeof(char), 0, (struct sockaddr *) &from, &length);
	if (n < 0)
		return -1;

	free(buf);

	return 0;
}

char* receive_string_from(int socket){
	int str_size = receive_int_from(socket); //recebe o tamamho do string a ser lido

	char *buf = malloc(sizeof(char)*str_size);

	int n;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	clilen = sizeof(struct sockaddr_in);

	n = recvfrom(socket,buf, str_size, 0, (struct sockaddr *) &cli_addr, &clilen);
	if (n < 0)
		return NULL;

	//retorna o ack
	n = sendto(socket, "ACK", 3, 0,(struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
	if (n  < 0)
		return NULL;

	return buf;
}

int send_string_to(int socket, char* str){
	int str_size = strlen(str);

	send_int_to(socket, str_size); //envia o tam do string a ser lido pro server

	int n;
	struct sockaddr_in serv_addr, from;

	n = sendto(socket, str, str_size, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	if (n < 0)
		return -1;

	char *buf = malloc(sizeof(char)*3); //para receber o ack

	unsigned int length = sizeof(struct sockaddr_in);
	n = recvfrom(socket, buf, 3*sizeof(char), 0, (struct sockaddr *) &from, &length);
	if (n < 0)
		return -1;

	free(buf);

	return 0;

}

int receive_file_from(int socket, char* filename){
	int file;
	struct packet file_packet, reply;
	struct sockaddr_in from;
	unsigned int length = sizeof(struct sockaddr_in);

	file = open(filename, O_RDWR | O_CREAT, 0666);
	if (file == -1){
		printf("Error! Path: %s\n\n",filename);
	}
	recvfrom(socket, (char *) &file_packet, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
	while(file_packet.opcode == FILEPKT){
		reply.opcode = ACK;
		reply.seqnum = file_packet.seqnum;
		write(file, file_packet.data, PACKETSIZE - 4);
		sendto(socket, (char *) &reply, PACKETSIZE, 0, (const struct sockaddr *) &from, sizeof(struct sockaddr_in));
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

int send_file_to(int socket, char* filepath, struct sockaddr destination){
	int file, i = 1, bytes_read = 0;
	struct packet file_packet, reply;
	struct sockaddr_in from;
	unsigned int length = sizeof(struct sockaddr_in);

	file = open(filepath, O_RDONLY);
	if (file <0){
		return -1;
	}
	bytes_read = read(file, file_packet.data, PACKETSIZE - 4);
	if (bytes_read == 0){
		file_packet.opcode = LASTPKT;
		file_packet.seqnum = (short) bytes_read;
		while(reply.opcode != ACK || reply.seqnum != file_packet.seqnum){
			sendto(socket, (char *) &file_packet, PACKETSIZE, 0, (const struct sockaddr *) &destination, sizeof(struct sockaddr_in));
			recvfrom(socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
			//printf("Reply was %hi for pkt #%hi, expected 1 for %hi\n\n",reply.opcode,reply.seqnum, file_packet.seqnum);
		}
	}
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
			//printf("Reply was %hi for pkt #%hi, expected 1 for %hi\n\n",reply.opcode,reply.seqnum, file_packet.seqnum);
		}
		bytes_read = read(file, file_packet.data, PACKETSIZE - 4);
		//printf("Bytes read: %d\n\n", bytes_read);
		i++;
	}
	return 0;
}

#endif
