#ifndef DROPBOXCLIENT_C
#define DROPBOXCLIENT_C

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
#include <netdb.h>
#include <pwd.h>

#define BUFFERSIZE 1250
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#define PING 10
#define FILENAMESIZE 50
#define MAXARQINDIR 30

struct packet {
	short int opcode;
	short int seqnum;
	char data [PACKETSIZE - 4];
};
struct sync_data {
	char client_new[FILENAMESIZE][MAXARQINDIR];
	char server_new[FILENAMESIZE][MAXARQINDIR];

	char client_old[FILENAMESIZE][MAXARQINDIR];
	char server_old[FILENAMESIZE][MAXARQINDIR];
};

int is_syncing = FALSE;
int mustexit = FALSE;
char userID[20];
char host[20];
int port;
int socket_local;
struct sockaddr_in serv_addr;
struct hostent *server;
double time_between_sync = 10.f;
int primeiro_sync = TRUE;
struct sync_data syncdataglobal;

pthread_mutex_t lockcomunicacao;
int encontrou(char name[FILENAMESIZE],char name_list[FILENAMESIZE][MAXARQINDIR]){
	int i =0;
	int encontrou = FALSE;


	while(encontrou ==FALSE && strcmp(name_list[i],"FIMDALISTA")!=0){
		if (strcmp(name_list[i],name)==0)
			encontrou =TRUE;
		i++;
	}
	return encontrou;

}

char* list_server();

void pickFileNameFromPath(char *path,char *filename){
	char aux[100]; char aux2[100];
	int i=0;int j=0;
	int lastdash=0;
	strcpy(aux,path);

	while(aux[i]!='\0'){
		if(aux[i]=='/')
			lastdash = i;
		i++;
	}
	strcpy(filename,&aux[lastdash+1]);

}

char* devolvePathSyncDir(){
	char * pathsyncdir;
	pathsyncdir = (char*) malloc(sizeof(char)*100);
	strcpy(pathsyncdir,"~/");
	removeBlank(pathsyncdir);
	strcat(pathsyncdir,"sync_dir_");
	strcat(pathsyncdir,userID);
	strcat(pathsyncdir,"/");


	return pathsyncdir;
}

char* devolvePathSyncDirBruto(){
	char* pathsyncdir;
	pathsyncdir = (char*) malloc(sizeof(char)*100);
	strcpy(pathsyncdir,"~/");
	strcat(pathsyncdir,"sync_dir_");
	strcat(pathsyncdir,userID);
	strcat(pathsyncdir,"/");


	return pathsyncdir;
}

char* findnext(char* list_server,int contador, int * contstr){
	char* findnext;
	int i=0;int j=0;
	findnext = (char*)malloc(sizeof(list_server));
	strcpy(findnext,"");
	if (contador ==0){
		while(list_server[i]!='\n') //pula toda a primeira linha
			i++;
		i++;
	}
	else{
		i = contstr[0];
	}
	if(list_server[i]=='\0')
		return findnext;
	i = i+3; // pula o ' - ' no inicio de cada linha
	//printf("vamos trabalhar a partir de ::%s::\n",&list_server[i]);
	while(list_server[i]!='\n' && list_server[i]!='\0'){
		findnext[j] = list_server[i];
		i++;j++;
	}
	if (list_server[i]=='\n')
		i++;
	findnext[j]='\0';

	contstr[0] = i;
	//printf("resultado do findnext é: ::%s::\n\n\n",findnext);
	return findnext;

}

void setsynctime(int newsynctime){
	time_between_sync = newsynctime;
}

//=======================================================
int login_server(char *host,int port){
	int n;
	struct sockaddr_in from;
	char buffer[BUFFERSIZE];
	int i;
	int recebeuack = FALSE;
	struct packet message, reply;
	unsigned int length = sizeof(struct sockaddr_in);

	pthread_mutex_lock(&lockcomunicacao);

	create_home_dir(userID);

	for (i=0;i<BUFFERSIZE;i++)
		buffer[i]='\0';

	message.opcode = LOGIN;
	message.seqnum = LOGIN;
	strcpy(message.data,userID);

	while(!recebeuack){
		n = sendto(socket_local, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
		n = recvfrom(socket_local, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
		if (reply.opcode == ACK){
			recebeuack = TRUE;
		}
	}
	int newport = reply.seqnum;

	if ((socket_local = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(newport);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	pthread_mutex_unlock(&lockcomunicacao);
	return 1;
}

void get_file(char *filename, char * finalpath){
	int n;
	struct sockaddr_in from;
	char buffer[BUFFERSIZE];
	int i;
	int recebeuack = FALSE;
	struct packet message, reply;
	unsigned int length = sizeof(struct sockaddr_in);
	char filepath[300];
	const char *homedir;
	char path[300];

	pthread_mutex_lock(&lockcomunicacao);

	if ((homedir = getenv("HOME")) == NULL) {
			homedir = getpwuid(getuid())->pw_dir;
	}

	message.opcode = DOWNLOAD;
	message.seqnum = 0;
	strcpy(message.data,filename);

	while(!recebeuack){
		n = sendto(socket_local, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
		n = recvfrom(socket_local, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
		if (reply.opcode == ACK){
			recebeuack = TRUE;
		}
	}

	strcpy(filepath,"~/");
	removeBlank(filepath);
	strcat(filepath,&finalpath[2]);
	strcat(filepath,filename);

	//printf("Recebemos ack. Vamos receber o arquivo agora. Nome do arquivo era: %s\ne o path era: %s\n",filename,filepath);
	recebeuack = receive_file_from(socket_local, filepath);

	pthread_mutex_unlock(&lockcomunicacao);

}

void send_file(char *file){
	int n;
	struct sockaddr_in from;
	char buffer[BUFFERSIZE];
	int i;
	int recebeuack = FALSE;
	struct packet message, reply;
	unsigned int length = sizeof(struct sockaddr_in);
	char filepath[300];
	struct sockaddr* destiny;
	char filename[100];

	pthread_mutex_lock(&lockcomunicacao);
	pickFileNameFromPath(file,filename);

	message.opcode = UPLOAD;
	message.seqnum = 0;
	strcpy(message.data,filename);

	while(!recebeuack){
		n = sendto(socket_local, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
		n = recvfrom(socket_local, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
		if (reply.opcode == ACK){
			recebeuack = TRUE;
		}
	}
	strcpy(filepath,file);
	removeBlank(filepath);
	//printf("Recebemos ack. Vamos enviar o arquivo agora. Nome do arquivo era: %s\ne o path era: %s\n",filename,filepath);
	send_file_to(socket_local, filepath, *((struct sockaddr*) &serv_addr));
	pthread_mutex_unlock(&lockcomunicacao);
}

void delete_file(char *filename){
	int n;
	struct sockaddr_in from;
	char buffer[BUFFERSIZE];
	int i;
	int recebeuack = FALSE;
	struct packet message, reply;
	unsigned int length = sizeof(struct sockaddr_in);
	int ret;
	FILE *fp;
	char path[300];

	pthread_mutex_lock(&lockcomunicacao);

	strcpy(path,"~/");
	removeBlank(path);
	strcat(path,"sync_dir_");
	strcat(path,userID);
	strcat(path,"/");
	strcat(path,filename);

	/*
	fp = fopen (path,"r");
  if (fp == NULL) {
      printf ("Arquivo não existe\n");
  }
	*/
		message.opcode = DELETE;
		message.seqnum = 0;
		strcpy(message.data,filename);

		while(!recebeuack){
			n = sendto(socket_local, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
			n = recvfrom(socket_local, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
			if (reply.opcode == ACK){
				recebeuack = TRUE;
			}
		}

	/*
	ret = remove(path);

	if(ret == 0) {
		 printf("Arquivo foi deletado com sucesso!\n");
	} else {
		 printf("Algo deu errado...\n");
	}
	*/
	pthread_mutex_unlock(&lockcomunicacao);
}

void executaSync(struct sync_data syncdata){
	int i;
	char* path;
	int ret=0;

	//printf("\nO que de novo deve ser enviado para o server:\n");
	i=0;
	while(strcmp(syncdata.client_new[i],"FIMDALISTA")!=0){
		if (!encontrou(syncdata.client_new[i],syncdata.client_old)){
			path = devolvePathSyncDirBruto();
			strcat(path,syncdata.client_new[i]);
			send_file(path);
			//printf(" - %s\n",path);
		}
		i++;
	}
	//printf("\nO que deve ser deletado do server:\n");
	i=0;
	while(strcmp(syncdata.client_old[i],"FIMDALISTA")!=0){
		if (!encontrou(syncdata.client_old[i],syncdata.client_new)){
			delete_file(syncdata.client_old[i]);
			//printf(" - %s\n",syncdata.client_old[i]);
		}
		i++;
	}
	//printf("\nO que deve ser baixado de novo do server:\n");
	i=0;
	while(strcmp(syncdata.server_new[i],"FIMDALISTA")!=0){
		if (!encontrou(syncdata.server_new[i],syncdata.server_old)){
			get_file(syncdata.server_new[i],devolvePathSyncDirBruto());
			//printf(" - %s\n",syncdata.server_new[i]);
		}
		i++;
	}
	//printf("\nO que deve ser deletado no cliente:\n");
	i=0;
	while(strcmp(syncdata.server_old[i],"FIMDALISTA")!=0){
		if (!encontrou(syncdata.server_old[i],syncdata.server_new)){
			path = devolvePathSyncDir();
			strcat(path,syncdata.server_old[i]);
			ret = remove(path);
			//printf(" - %s\n",path);
		}
		i++;
	}
	i=0;


}

void firstExecutaSync(struct sync_data syncdata){
	int i;
	char* path;
	int ret=0;

	//printf("\nO que deve ser baixado de novo do server:\n");
	i=0;
	while(strcmp(syncdata.server_new[i],"FIMDALISTA")!=0){
		if (!encontrou(syncdata.server_new[i],syncdata.client_new)){
			get_file(syncdata.server_new[i],devolvePathSyncDirBruto());
			//printf(" - %s\n",syncdata.server_new[i]);
		}
		i++;
	}
	//printf("\nO que deve ser deletado no cliente:\n");
	i=0;
	while(strcmp(syncdata.client_new[i],"FIMDALISTA")!=0){
		if (!encontrou(syncdata.client_new[i],syncdata.server_new)){
			path = devolvePathSyncDir();
			strcat(path,syncdata.client_new[i]);
			ret = remove(path);
			//printf(" - %s\n",path);
		}
		i++;
	}
	i=0;


}

void first_sync_client(){
	char * path;
	char * sendpath;
	int length, i;
	int fd;
	int wd;
	char buffer[BUF_LEN];
	char * list_serverstr;
	int contador;
	char * nomearqremoto;
	int contstr[1];
	int j;

	contstr[0] = 0;
	i = 0;
	contador= 0;

		//Verifica o que há no cliente
		j=0;
		path = devolvePathSyncDir();
		DIR* dir = opendir(path);
		struct dirent * file;
		while((file = readdir(dir)) != NULL){
			if(file->d_type==DT_REG){
				strcpy(syncdataglobal.client_new[j], file->d_name);
				j++;
			}
		}
		strcpy(syncdataglobal.client_new[j], "FIMDALISTA");

		//Verifica o que há no server
		j=0;
		list_serverstr = list_server();
		nomearqremoto = findnext(list_serverstr,contador,contstr);
		contador++;
		while(nomearqremoto[0]!='\0' && nomearqremoto[0]!='\n'){
			strcpy(syncdataglobal.server_new[j], nomearqremoto);
			nomearqremoto = findnext(list_serverstr,contador,contstr);
			contador++;
			j++;
		}
		strcpy(syncdataglobal.server_new[j], "FIMDALISTA");

		memcpy(syncdataglobal.client_old,syncdataglobal.client_new,FILENAMESIZE*MAXARQINDIR);
		memcpy(syncdataglobal.server_old,syncdataglobal.server_new,FILENAMESIZE*MAXARQINDIR);


		firstExecutaSync(syncdataglobal);

		primeiro_sync = FALSE;
}

void sync_client(){
	char * path;
	char * sendpath;
	int length, i;
	int fd;
	int wd;
	char buffer[BUF_LEN];
	char * list_serverstr;
	int contador;
	char * nomearqremoto;
	int contstr[1];
	int j;

	contstr[0] = 0;
	i = 0;
	contador= 0;


		//Verifica o que há no cliente
		j=0;
		path = devolvePathSyncDir();
		DIR* dir = opendir(path);
		struct dirent * file;
		while((file = readdir(dir)) != NULL){
			if(file->d_type==DT_REG){
				strcpy(syncdataglobal.client_new[j], file->d_name);
				j++;
			}
		}
		strcpy(syncdataglobal.client_new[j], "FIMDALISTA");
		//Verifica o que há no server
		j=0;
		list_serverstr = list_server();
		nomearqremoto = findnext(list_serverstr,contador,contstr);
		contador++;
		while(nomearqremoto[0]!='\0' && nomearqremoto[0]!='\n'){
			strcpy(syncdataglobal.server_new[j], nomearqremoto);
			nomearqremoto = findnext(list_serverstr,contador,contstr);
			contador++;
			j++;
		}
		strcpy(syncdataglobal.server_new[j], "FIMDALISTA");

		//printf("Chamando executa sync errado\n");
		executaSync(syncdataglobal);

		memcpy(syncdataglobal.client_old,syncdataglobal.client_new,FILENAMESIZE*MAXARQINDIR);
		memcpy(syncdataglobal.server_old,syncdataglobal.server_new,FILENAMESIZE*MAXARQINDIR);

}

void close_session(){
	int n;
	struct sockaddr_in from;
	char buffer[BUFFERSIZE];
	int i;
	int recebeuack = FALSE;
	struct packet message, reply;
	unsigned int length = sizeof(struct sockaddr_in);

	pthread_mutex_lock(&lockcomunicacao);
	create_home_dir(userID);

	for (i=0;i<BUFFERSIZE;i++)
		buffer[i]='\0';

	message.opcode = CLOSE;
	message.seqnum = 0;
	strcpy(message.data,userID);

	while(!recebeuack){
		n = sendto(socket_local, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
		n = recvfrom(socket_local, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
		if (reply.opcode == ACK){
			recebeuack = TRUE;
		}
	}
	pthread_mutex_unlock(&lockcomunicacao);
}
//=======================================================
char* list_server(){
	int n;
	struct sockaddr_in from;
	char buffer[BUFFERSIZE];
	int i;
	int recebeuack = FALSE;
	struct packet message, reply;
	unsigned int length = sizeof(struct sockaddr_in);

	pthread_mutex_lock(&lockcomunicacao);
	create_home_dir(userID);

	for (i=0;i<BUFFERSIZE;i++)
		buffer[i]='\0';

	message.opcode = LIST;
	message.seqnum = 0;
	strcpy(message.data,userID);

	while(!recebeuack){
		n = sendto(socket_local, (char *)&message, PACKETSIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
		n = recvfrom(socket_local, (char *)&reply, PACKETSIZE, 0, (struct sockaddr *) &from, &length);
		if (reply.opcode == ACK){
			recebeuack = TRUE;
		}
	}
	pthread_mutex_unlock(&lockcomunicacao);

	char * list_server;
	list_server = (char*)malloc(sizeof(reply.data));
	strcpy(list_server,reply.data);

	return list_server;
}

void list_client(){
	char * path;
	int length, i = 0;
	int fd;
	int wd;

	path = devolvePathSyncDir();
	DIR* dir = opendir(path);
	struct dirent * file;
	printf("Conteúdo do diretório local:\n");
	while((file = readdir(dir)) != NULL){
		if(file->d_type==DT_REG){
			printf(" - %s\n",file->d_name);
		}
	}


}

void treat_command(char command[100]){
	int result= 0;
	char * argument;
	char filename[100];

	if (!strncmp("exit",command,4)){
		close_session();
		mustexit = TRUE;
	}
	else if (!strncmp("upload",command,6)){
		argument = getArgument(command);
		send_file(argument);
		pickFileNameFromPath(argument,filename);
		//get_file(filename,devolvePathSyncDirBruto());
		result = 1;
	}
	else if (!strncmp("download",command,8)){
		argument = getArgument(command);
		get_file(argument,getSecondArgument(command));
		result = 2;
	}
	else if (!strncmp("list_server",command,11)){
		printf("%s",list_server());
		result = 3;
	}
	else if (!strncmp("list_client",command,11)){
		list_client();
		result =4;
	}
	else if (!strncmp("get_sync_dir",command,12)){
		result = create_home_dir(userID);
		result = 5;
	}
	else if (!strncmp("delete",command,6)){
		argument = getArgument(command);
		delete_file(argument);
		result =6;
	}
	else if (!strncmp("setsynctime",command,11)){
		argument = getArgument(command);
		setsynctime(atoi(argument));
		result =7;
	}


	if (!result){;
	}
	else{
		printf("Operação %d efetuada com sucesso!\n\n",result);
	}


	//printf("Seu comando foi: %s \n",command);
}

void* thread_sync(void *vargp){

		is_syncing = TRUE;
		sync_client();
		is_syncing = FALSE;

    pthread_exit((void *)NULL);
}

void* thread_interface(void *vargp){
		char command[100];
		printf("Escreva uma ação para o sistema:\n");

		while(!mustexit){
			printf(">>");
			fgets(command,100,stdin);
			treat_command(command);
		}

    pthread_exit((void *)NULL);
}

void* thread_frontend(){
	SOCKET frontend_socket;
	int n, frontend_len, from_len, online = 1;
	struct sockaddr_in frontend, from;
	struct packet message;

	if((frontend_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		exit(1);
	}
	memset((void *) &frontend,0,sizeof(struct sockaddr_in));
	frontend.sin_family = AF_INET;
	frontend.sin_addr.s_addr = htonl(INADDR_ANY);
	frontend.sin_port = htons(4000);
	frontend_len = sizeof(frontend);
	if (bind(frontend_socket,(struct sockaddr *) &frontend, frontend_len)) {
		exit(1);
	}
	while(online){
		n = recvfrom(frontend_socket, (char *) &message, PACKETSIZE, 0, (struct sockaddr *) &from, (socklen_t *) &from_len);
		if(n && message.opcode == PING){
			serv_addr = from;
		}
	}
}

int main(int argc,char *argv[]){
	int loginworked = FALSE;
	char strporta[100];

	if (pthread_mutex_init(&lockcomunicacao, NULL) != 0)
		{
				printf("\n inicialização do mutex falhou\n");
				return 1;
		}

	if (argc!=4){
		printf("Escreva no formato: ./dropboxClient <ID_do_usuário> <endereço_do_host> <porta>\n");
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

		printf("Socket pelo qual o cliente está enviando coisas: %d\n",socket_local);


		loginworked = login_server(host,port);
		if(!loginworked){
			printf("Login não funcionou!");
		}
		else{
			pthread_t tid[2];
	    int i = 0;
	    int n_threads = 2;
			double last_time;
			double actual_time;
			last_time=  (double) clock() / CLOCKS_PER_SEC;
			actual_time = (double) clock() / CLOCKS_PER_SEC;
			pthread_create(&(tid[0]), NULL, thread_interface,NULL);

			first_sync_client();

			while(!mustexit){ //exits here when the user digits 'quit' at the interface thread
				actual_time = (double) clock() / CLOCKS_PER_SEC;

				if ((!is_syncing)&&(actual_time - last_time >= time_between_sync)){ //throws a sync thread every 10 sec, if there isn't one already
					last_time = actual_time;
					pthread_create(&(tid[1]), NULL, thread_sync, NULL);
				}
			}
			pthread_join(tid[0], NULL);
			//close_session();
		}
	}

	return 0;
}

#endif
