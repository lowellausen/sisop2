#ifndef DROPBOXSERVER_C
#define DROPBOXSERVER_C

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
#include <arpa/inet.h>

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
#define PING 10

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
	struct sockaddr_in addr[MAXSESSIONS];
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

struct upload_info {
	char filename[MAXNAME];
	int session_port;
	char userID[MAXNAME];
};

struct client client_list [MAXCLIENTS];

SOCKET main_socket;
int primary_server_id, local_server_id, primary_len, inform_frontend_clients;
char ip_server_1[20];
char ip_server_2[20];
char ip_server_3[20];
struct hostent *host_server_1;
struct hostent *host_server_2;
struct hostent *host_server_3;
struct sockaddr_in server_list[4];
struct sockaddr_in server_election_list[4];
struct sockaddr_in primary_server;
int not_electing;
int session_count;
int election_setup = 0, answer_setup = 0;

char * devolvePathHomeServer(char *userID){
	char * pathsyncdir;
	pathsyncdir = (char*) malloc(sizeof(char)*100);
	strcpy(pathsyncdir,"~/");
	removeBlank(pathsyncdir);
	strcat(pathsyncdir,"dropboxserver/");
	strcat(pathsyncdir,userID);
	strcat(pathsyncdir,"/");


	return pathsyncdir;
}

int inactive_client(int index){
	int i;
	for (i = 0; i < MAXSESSIONS; i++){
		if (client_list[index].session_active[i] == 1) return 0;
	}
	return 1;
}

int identify_client(char user_id [MAXNAME], int* client_index){
	int i;
	*client_index = -1;
	for (i = 0; i < MAXCLIENTS; i++){
		if (strcmp(client_list[i].user_id,user_id) == 0){
			*client_index = i;
			return 0;
		}
	}
	for (i = 0; i < MAXCLIENTS; i++){
		if (inactive_client(i)){
			strncpy(client_list[i].user_id, user_id, MAXNAME);
			*client_index = i;
			return 0;
		}
	}
	return -1;
}

void send_file(char *file, int socket, char *userID, struct sockaddr client_addr){
	char path[300];
	strcpy(path, getenv("HOME"));
	strcat(path, "/dropboxserver/");
	strcat(path, userID);
	strcat(path, "/");
	strcat(path, file);
	printf("File path is: %s\n\n",path);
	send_file_to(socket, path, client_addr);
}

void receive_file(char *file, int socket, char*userID){
	char path[300];
	int i;
	struct sockaddr *dest;
	strcpy(path, getenv("HOME"));
	strcat(path, "/dropboxserver/");
	strcat(path, userID);
	strcat(path, "/");
	strcat(path, file);
	printf("File path is: %s\n\n",path);
	receive_file_from(socket, path);
}

int delete_file(char *file, int socket, char*userID){
	char path[300];
	strcpy(path, getenv("HOME"));
	strcat(path, "/dropboxserver/");
	strcat(path, userID);
	strcat(path, "/");
	strcat(path, file);
	if(remove(path) == 0){
		return 1;
	}
	return 0;
}

void list_files(SOCKET socket, struct sockaddr client, char *userID){
	char * path;
	int length, i = 0;
	int fd;
	int wd;
	struct dirent *ep;
	char files_list[PACKETSIZE - 4];
	struct packet reply;
	reply.opcode = ACK;

	path = devolvePathHomeServer(userID);
	DIR* dir = opendir(path);
	struct dirent * file;
	strcpy(files_list,"Conteúdo do diretório remoto:\n");
	while((file = readdir(dir)) != NULL){
		if(file->d_type==DT_REG){
			strcat(files_list," - ");

			strcat(files_list,file->d_name);
			strcat(files_list,"\n");
		}
	}

	strncpy(reply.data, files_list, PACKETSIZE - 4);
	sendto(socket, (char *) &reply, PACKETSIZE,0,(struct sockaddr *)&client, sizeof(client));
}

int inform_frontend(struct sockaddr client, SOCKET session_socket){
	struct sockaddr_in *fe_client;
	struct packet ping;
	int fe_len;
	fe_client = (struct sockaddr_in *) &client;
	(*fe_client).sin_port = htons(4000);
	fe_len = sizeof(fe_client);
	ping.opcode = PING;
	sendto(session_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *)&fe_client, fe_len);
	return 0;
}

void *replica_upload(void *args){
	//chama a função assim: pthread_create(&tid, NULL, replica_upload, (void *) &param_info);
	SOCKET local_socket;
	struct upload_info *info = args;
	struct packet upload_request, reply;
	struct sockaddr *destination;
	char filename[MAXNAME];
	char userID[MAXNAME];
	int  servo_id = local_server_id +1, i, length, session_port = info->session_port;
	strncpy(filename,info->filename,MAXNAME);
	strncpy(userID,info->userID,MAXNAME);

	if ((local_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	//Set upload request
	upload_request.opcode = UPLOAD;
	upload_request.seqnum = 0;
	strncpy(upload_request.data,filename,MAXNAME);

	while(servo_id <= 3){
		int recebeuack =  FALSE;
		struct packet reply;
		int length;


		struct sockaddr_in servo_logaddr = server_list[servo_id];
		servo_logaddr.sin_port = htons(session_port);


		while(!recebeuack){
			sendto(local_socket, (char *)&upload_request, PACKETSIZE, 0, (const struct sockaddr *) &servo_logaddr, sizeof(struct sockaddr_in));
			recvfrom(local_socket, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &servo_logaddr, &length);
			if (reply.opcode == ACK){
				recebeuack = TRUE;
				destination = (struct sockaddr *) &servo_logaddr;
				send_file(filename, local_socket, userID, *destination);
			}
		}
		servo_id++;
	}
	pthread_exit(0);
}

void *session_manager(void* args){
	char filename[MAXNAME];
	SOCKET session_socket;
	struct sockaddr client;
	struct sockaddr_in session, aux_server;
	struct packet request, reply;
	int i, c_id, s_id, session_port, session_len, client_len = sizeof(struct sockaddr_in), active = 1;
	int has_informed = 0;

	// Getting thread arguments
	struct pair *session_info = (struct pair *) args;
	c_id = (*session_info).c_id;
	s_id = (*session_info).s_id;
	//printf("\nClient Id is %d and  Session Id is %d\n\n", c_id, s_id);
	session_port = (int) client_list[c_id].session_port[s_id];

	if (client_list[c_id].socket_set[s_id] == 0){
		// Set up a new socket
		printf("Setting up a new socket for this session!\n\n");
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
		else{
			printf("Client %d, Session %d Socket initialized, waiting for requests.\n\n", c_id, s_id);
		}
		client_list[c_id].socket_set[s_id] = 1;
		client_list[c_id].socket[s_id] = session_socket;
	}
	else{
		session_socket = client_list[c_id].socket[s_id];
	}
	printf("Client %d, Session %d Port is %hi\n\n", c_id, s_id, session_port);
		// Setup done

	while(active){
		if(inform_frontend_clients > 0 && has_informed == 0){
			inform_frontend(client, session_socket);
			inform_frontend_clients--;
			has_informed = 1;
		}
		else if (inform_frontend_clients == 0){
			has_informed = 0;
		}
		if (!recvfrom(session_socket, (char *) &request, PACKETSIZE, 0, (struct sockaddr *) &client, (socklen_t *) &client_len)){
			printf("ERROR: Package reception error.\n\n");
		}
		printf("Client %d, Session %d Opcode is: %hi\n\n", c_id, s_id, request.opcode);
		switch(request.opcode){
			case UPLOAD:;
				strncpy(filename, request.data, MAXNAME);
				reply.opcode = ACK;
				sendto(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
				strncpy(filename, request.data, MAXNAME);
				receive_file(filename, session_socket, client_list[c_id].user_id);
					fprintf(stderr, "%s\n", "AAAAAAAAAAAAAAAAAAAAAAAAAAAA" );

					if(primary_server_id == local_server_id){
						pthread_t tid;
						struct upload_info upinfo;
						upinfo.session_port = session_port;
						strncpy(upinfo.filename, filename, MAXNAME);
						strncpy(upinfo.userID, client_list[c_id].user_id, MAXNAME);
						pthread_create(&tid, NULL, replica_upload, (void *) &upinfo);
					}
				break;
			case DOWNLOAD:
				reply.opcode = ACK;
				sendto(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
				strncpy(filename, request.data, MAXNAME);
				send_file(filename, session_socket, client_list[c_id].user_id, client);
				break;
			case LIST:
				reply.opcode = ACK;
				//sendto(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
				list_files(session_socket, client, client_list[c_id].user_id);
				break;
			case DELETE:

					if(primary_server_id == local_server_id){
						int servo_id = local_server_id +1;

						while(servo_id <= 3){
							int recebeuack =  FALSE;
							struct packet replyServo;
							int length;


							struct sockaddr_in servo_logaddr = server_list[servo_id];
							servo_logaddr.sin_port = htons(session_port);


							while(!recebeuack){
								sendto(session_socket, (char *)&request, PACKETSIZE, 0, (const struct sockaddr *) &servo_logaddr, sizeof(struct sockaddr_in));
								recvfrom(session_socket, (char *)&replyServo, PACKETSIZE, 0, (struct sockaddr *) &servo_logaddr, &length);
								if (reply.opcode == ACK){
									recebeuack = TRUE;
								}
							}
							servo_id++;
						}
					}



				reply.opcode = ACK;
				sendto(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
				strncpy(filename, request.data, MAXNAME);
				delete_file(filename, session_socket, client_list[c_id].user_id);
				break;
			case CLOSE:

			if(primary_server_id == local_server_id){
				int servo_id = local_server_id +1;

				while(servo_id <= 3){
					int recebeuack =  FALSE;
					struct packet replyServo;
					int length;


					struct sockaddr_in servo_logaddr = server_list[servo_id];
					servo_logaddr.sin_port = htons(session_port);

					while(!recebeuack){
						sendto(session_socket, (char *)&request, PACKETSIZE, 0, (const struct sockaddr *) &servo_logaddr, sizeof(struct sockaddr_in));
						recvfrom(session_socket, (char *)&replyServo, PACKETSIZE, 0, (struct sockaddr *) &servo_logaddr, &length);
						if (reply.opcode == ACK){
							recebeuack = TRUE;
						}
					}
					servo_id++;
				}
			}

				reply.opcode = ACK;
				sendto(session_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
				client_list[c_id].session_active[s_id] = 0;
				session_count--;
				pthread_exit(0); // Should have an 'ack' by the client allowing us to terminate, ideally!
				break;
			default:
				printf("ERROR: Invalid packet detected. Type %hi, seqnum: %hi.\n\n",request.opcode, request.seqnum);
		}
	}
}

int login(struct packet login_request){
	struct pair *thread_param;
	char user_id [MAXNAME];
    char ip_addr [20];
	int i, index;
	char *plus_index;
	short int port;
	pthread_t tid;
	struct hostent *host_cl;
	struct sockaddr_in cl_addr;

    if(primary_server_id != local_server_id){
        plus_index = strchr(login_request.data, '+');
        strncpy(ip_addr,plus_index+1, 20);
        (*plus_index) = 0;
        strncpy(user_id, login_request.data, MAXNAME);
        printf("IP is %s\n\n",ip_addr);
        printf("User is %s\n\n",user_id);
    }
    else{
        strncpy (user_id, login_request.data, MAXNAME);
    }


	identify_client(user_id, &index);
	if (login_request.opcode != LOGIN || index == -1){
		return -1;
	}
	for (i = 0; i < MAXSESSIONS; i++){
		if (client_list[index].session_active[i] == 0){
			port = MAIN_PORT + (2*index) + i + 1;
			client_list[index].session_active[i] = 1;
			client_list[index].session_port[i] = (short) port;
			thread_param = malloc(sizeof(struct pair));
			(*thread_param).c_id = index;
			(*thread_param).s_id = i;
			create_server_userdir(client_list[index].user_id);

			if(primary_server_id != local_server_id){
                host_cl = gethostbyname(ip_addr);
                cl_addr.sin_family = AF_INET;
                cl_addr.sin_port = htons(9999);
                cl_addr.sin_addr = *((struct in_addr *)host_cl->h_addr);
                bzero(&(cl_addr.sin_zero), 8);
                client_list[index].addr[i] = cl_addr;
            }

			printf("\nClient Id is %d and  Server Id is %d\n\n", index, i);
			pthread_create(&tid, NULL, session_manager, (void *) thread_param);
			return port;
		}
	}
	return -1;
}
// ========================================================================== //
void* sync_server_manager(){
	// Handle updating files to the non-primary servers
	//(doesn't handle login, close or delete requests, those are duplicated elsewhere)
}

void* election_answer(){
	SOCKET rm_socket;
	struct sockaddr_in primary_rm, this_rm, from;
	struct packet ping, reply;
	struct sockaddr_in election_s;
	int n, i, j, rm_port = 3000, this_len, from_len, online = 1;
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	ping.opcode = PING;
	ping.seqnum = (short) local_server_id;

	// Set up socket
	if(answer_setup == 0){
		if((rm_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			exit(1);
		}
		memset((void *) &this_rm,0,sizeof(struct sockaddr_in));
		this_rm.sin_family = AF_INET;
		this_rm.sin_addr.s_addr = htonl(INADDR_ANY);
		this_rm.sin_port = htons(rm_port);
		this_len = sizeof(this_rm);
		if (bind(rm_socket,(struct sockaddr *) &this_rm, this_len)) {
			exit(1);
		}
		answer_setup =11;
	}
	//
	reply.opcode = ACK;
	reply.seqnum = local_server_id;
	while(online){
		n = recvfrom(rm_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *) &from, (socklen_t *) &from_len);
		printf("Received bytes: %d\n\n", n);
		election_s.sin_family = AF_INET;
		election_s.sin_port = htons(2000);
		election_s.sin_addr = server_list[ping.seqnum].sin_addr;
		bzero(&(election_s.sin_zero), 8);
		n = sendto(rm_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&election_s, sizeof(election_s));
		while (n < 0){
			n = sendto(rm_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *)&election_s, sizeof(election_s));
		}
		printf("Sent bytes %d to server %d\n\n",n,ping.seqnum);
	}
	printf("Finished E_answer\n\n");
}

void* election_ping(){
	struct packet ping, reply;
	struct sockaddr_in from;
	int from_len;
	SOCKET ping_socket;
	int i, n, ping_len, not_done = 1;
	struct sockaddr_in pingaddr;

	// Socket setup
	if(election_setup == 0){
		if((ping_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			printf("ERROR: Socket creation failure.\n");
			exit(1);
		}
		memset((void *) &pingaddr,0,sizeof(struct sockaddr_in));
		pingaddr.sin_family = AF_INET;
		pingaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		pingaddr.sin_port = htons(2000);
		ping_len = sizeof(pingaddr);
		if (bind(ping_socket,(struct sockaddr *) &pingaddr, ping_len)) {
			printf("Binding error\n");
			exit(1);
		}
		printf("Socket initialized, waiting for requests.\n\n");
		election_setup = 1;
	}
	// Setup done

	struct sockaddr_in election_s;
	ping.opcode = PING;
	ping.seqnum = local_server_id;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (setsockopt(ping_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		perror("Error");
	}
	printf("Starting election process\n\n");
	i = 1;
	while(i < 4 && not_done == 1){
		printf("Iteration: %d\n\n",i);
		if (local_server_id == i){
			primary_server_id = i;
			primary_server = server_list[i];
			primary_len = sizeof(server_list[i]);
			printf("Chose %d as the new lead server\n\n",i);
			not_done = 0;
		}
		else{
			election_s = server_list[i];
			election_s.sin_port = htons(3000);
			n = sendto(ping_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *)&election_s, sizeof(election_s));
			while (n < 0){
				printf("Pinging\n\n");
				n = sendto(ping_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *)&election_s, sizeof(election_s));
			}
			n = recvfrom(ping_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &from, (socklen_t *) &from_len);
			if(0 < n){
				primary_server_id = i;
				primary_server = server_list[i];
				primary_len = sizeof(server_list[i]);
				printf("Chose %d as the new lead server\n\n",i);
				not_done = 0;
			}
		}
		i++;
	}
	printf("Got here\n\n");
	sleep(1);
	not_electing = 1;
	if(local_server_id == primary_server_id){
		inform_frontend_clients = session_count;
	}
	printf("Done here\n\n");
	pthread_exit(0);
}

void *replica_manager(){
	pthread_t thread_elect, thread_answer;
	SOCKET rm_socket;
	struct sockaddr_in primary_rm, this_rm, from;
	struct packet ping, reply;
	int i, j, rm_port = 5000, this_len, from_len, online = 1;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	int n, first_ping = 1;
	not_electing = 1;
	if((rm_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		exit(1);
	}
	memset((void *) &this_rm,0,sizeof(struct sockaddr_in));
	this_rm.sin_family = AF_INET;
	this_rm.sin_addr.s_addr = htonl(INADDR_ANY);
	this_rm.sin_port = htons(rm_port);
	this_len = sizeof(this_rm);
	if (bind(rm_socket,(struct sockaddr *) &this_rm, this_len)) {
		exit(1);
	}
	printf("Primary is %d\n\n", primary_server_id);
	reply.opcode = ACK;
	reply.seqnum = local_server_id;
	ping.opcode = PING;
	ping.seqnum = local_server_id;

	int timeouts = 0;

	while(online){

		if (primary_server_id != local_server_id){
			tv.tv_sec = 2;
			if (setsockopt(rm_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
				perror("Error");
			}
			n = sendto(rm_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *) &primary_server, primary_len);
			while(n < 0){
				n = sendto(rm_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *) &primary_server, primary_len);
			}
			//printf("Sent opcode %hi, pkt #%hi\n\n", ping.opcode, ping.seqnum);
			n = recvfrom(rm_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &from, (socklen_t *) &from_len);
			if(n<0)timeouts++;
			else timeouts = 0;

			if (timeouts >= 3 || ping.seqnum < reply.seqnum){
				printf("Ping timeout\n\n");
				not_electing = 0;
				pthread_create(&thread_elect, NULL, election_ping, NULL);
				pthread_create(&thread_answer, NULL, election_answer, NULL);
				pthread_join(thread_elect,(void *) &n);
			}
			//printf("Received opcode %hi, pkt #%hi\n\n", reply.opcode, reply.seqnum);
		}
		else{
			tv.tv_sec = 600;
			if (setsockopt(rm_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
				perror("Error");
			}
			n = recvfrom(rm_socket, (char *) &ping, PACKETSIZE, 0, (struct sockaddr *) &from, (socklen_t *) &from_len);
			//printf("Received opcode %hi, pkt #%hi\n\n", ping.opcode, ping.seqnum);
			n = sendto(rm_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &(server_list[ping.seqnum]), from_len);
			while(n < 0){
				n = sendto(rm_socket, (char *) &reply, PACKETSIZE, 0, (struct sockaddr *) &(server_list[ping.seqnum]), from_len);
			}
			if(ping.seqnum < local_server_id){
				not_electing = 0;
				pthread_create(&thread_elect, NULL, election_ping, NULL);
				pthread_create(&thread_answer, NULL, election_answer, NULL);
				pthread_join(thread_elect,(void *) &n);
			}
			//printf("Sent opcode %hi, pkt #%hi\n\n", reply.opcode, reply.seqnum);
		}
	}
}
//============================================================================
int main(int argc,char *argv[]){
	//char host[20];
	char strid[100];
	struct sockaddr client;
	struct sockaddr_in server, aux_server;
	struct packet message, login_request, login_reply;
	int i, j, session_port, server_len, client_len = sizeof(struct sockaddr_in), online = 1;
	pthread_t tid1, tid2;
	session_count = 0;
	// num_primario indicates which server is primary: 1 -> a, 2 -> b, 3 -> this one
	if (argc!=6){
		printf("Escreva no formato: ./dropboxServer <endereço_do_server_1> <endereço_do_server_2> <endereço_do_server_3> <id_do_server_local>\n\n");
		return 0;
	}
	strcpy(ip_server_1,argv[1]);
	strcpy(ip_server_2,argv[2]);
	strcpy(ip_server_3,argv[3]);
	strcpy(strid,argv[4]);
	local_server_id = atoi(strid);
	strcpy(strid,argv[5]);
	primary_server_id = atoi(strid);
	inform_frontend_clients = 0;

	host_server_1 = gethostbyname(ip_server_1);
	server_list[1].sin_family = AF_INET;
	server_list[1].sin_port = htons(5000);
	server_list[1].sin_addr = *((struct in_addr *)host_server_1->h_addr);
	bzero(&(server_list[1].sin_zero), 8);
	//
	host_server_2 = gethostbyname(ip_server_2);
	server_list[2].sin_family = AF_INET;
	server_list[2].sin_port = htons(5000);
	server_list[2].sin_addr = *((struct in_addr *)host_server_2->h_addr);
	bzero(&(server_list[2].sin_zero), 8);
	//
	host_server_3 = gethostbyname(ip_server_3);
	server_list[3].sin_family = AF_INET;
	server_list[3].sin_port = htons(5000);
	server_list[3].sin_addr = *((struct in_addr *)host_server_3->h_addr);
	bzero(&(server_list[3].sin_zero), 8);
	//
	primary_server = server_list[primary_server_id];
	primary_len = sizeof(server_list[primary_server_id]);


	for (i = 0; i < MAXCLIENTS; i++){
		for(j = 0; j < MAXSESSIONS; j++){
			client_list[i].socket_set[j] = 0;
		}
	}

	create_server_root();
	// Socket setup
	if((main_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("ERROR: Socket creation failure.\n");
		exit(1);
	}
	memset((void *) &server,0,sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(MAIN_PORT);
	server_len = sizeof(server);
	if (bind(main_socket,(struct sockaddr *) &server, server_len)) {
		printf("Binding error\n");
		exit(1);
	}
	printf("Socket initialized, waiting for requests.\n\n");
	// Setup done

	//pthread_create(&tid1, NULL, setrep, NULL);
	pthread_create(&tid2, NULL, replica_manager, NULL);

	while(online){
		if (!recvfrom(main_socket, (char *) &login_request, PACKETSIZE, 0, (struct sockaddr *) &client, (socklen_t *) &client_len)){
			printf("ERROR: Package reception error.\n\n");
		}
		else{
			if(login_request.opcode == LOGIN){

				if(primary_server_id == local_server_id){
					int servo_id = local_server_id +1;


					char ip_str[256];

					inet_ntop(AF_INET, &((struct sockaddr_in *)&client)->sin_addr, ip_str, INET_ADDRSTRLEN);

					fprintf(stderr, "\n%s\n", ip_str);

					char id_ip[MAXNAME + 256];

					strncpy(id_ip, login_request.data, MAXNAME);
					strcat(id_ip, "+");
					strcat(id_ip, ip_str);
					strcat(id_ip, "\0");

					fprintf(stderr, "\n%s\n", id_ip);

					message.opcode = LOGIN;
					message.seqnum = LOGIN;
					strcpy(message.data,id_ip);


					while(servo_id <= 3){
						int recebeuack =  FALSE;
						struct packet reply;
						int length;


						struct sockaddr_in servo_logaddr = server_list[servo_id];
						servo_logaddr.sin_port = htons(6000);


						while(!recebeuack){
							sendto(main_socket, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &servo_logaddr, sizeof(struct sockaddr_in));
							recvfrom(main_socket, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &servo_logaddr, &length);
							if (reply.opcode == ACK){
								recebeuack = TRUE;
							}
						}
						servo_id++;
					}
				}


				session_port = login(login_request);
				//printf("\nopcode is %hi\n\n",login_request.opcode);
				//printf("\ndata is %s\n\n",login_request.data);
				if (session_port > 0){
					session_count++;
					login_reply.opcode = ACK;
					login_reply.seqnum = (short) session_port;
					sendto(main_socket, (char *) &login_reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
					// Send to all other servers
					printf("Login succesful...\n\n");
				}
				else{
					login_reply.opcode = NACK;
					sendto(main_socket, (char *) &login_reply, PACKETSIZE, 0, (struct sockaddr *)&client, client_len);
					printf("ERROR: Login unsuccesful...\n\n");
				}
			}
			//
		}
		login_request.opcode = 0;
	}

	return 0;
}

#endif
