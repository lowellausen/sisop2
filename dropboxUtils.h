#ifndef DROPBOXUTILS_HEADER
#define DROPBOXUTILS_HEADER

#include<string.h>
#include<stdlib.h>
#include <sys/socket.h>

#define MAXNAME 20
#define MAXFILES 10
#define CHUNK 1240
#define OPCODE 10

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


void removeBlank(char * filename);

int create_home_dir(char *userID);

int create_home_dir_server(char *userID);

int create_server_userdir(char *userID);

int create_server_root();

int receive_int_from(int socket);

int send_int_to(int socket, int op);

char* receive_string_from(int socket);

int send_string_to(int socket, char* str);

int receive_file_from(int socket, char* file_name);

int send_file_to(int socket, char* file_name, struct sockaddr destination);

char * getArgument(char* command);

char * getSecondArgument(char* command);

#endif
