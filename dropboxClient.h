#ifndef DROPBOXCLIENT_HEADER
#define DROPBOXCLIENT_HEADER



int login_server(char *host,int por);
void sync_client();
void send_file(char *file);
void get_file(char *file);
void delete_file(char *file);
void close_session();


#endif
