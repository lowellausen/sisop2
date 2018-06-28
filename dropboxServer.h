#ifndef DROPBOXSERVER_HEADER
#define DROPBOXSERVER_HEADER


void sync_server();
void receive_file(char *file);
void send_file(char *file);

struct client {
  int devices[2];
  char userid[MAXNAME];
  struct file_info[MAXFILES];
  int logged_in;
}

struct file_info {
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  int size;
}


void sync_server(int socket,  char*userID);

void receive_file(char *file, int socket, char*userID);

void send_file(char *file, int socket, char*userID);

#endif
