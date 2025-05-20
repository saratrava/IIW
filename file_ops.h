#include <stdio.h>

int download_rcv(int sd, struct sockaddr_in addr, FILE* fd, int N);
int upload_sender(int sd, struct sockaddr_in addr, FILE* fd, int N, int start_timeout, int adapt, int dim);
int list_rcv(int sd, struct sockaddr_in client, int N, int start_timeout, int adapt, char *path);
int getFunc(int sd, struct sockaddr_in client, char *file, int N, int timeout, int adapt);
int putFunc(int sd, struct sockaddr_in client, char *file, int N);
int list_files(int sd, struct sockaddr_in server, int N, int timeout);
int get_file(int sd, struct sockaddr_in server, char *file, char *dir_path, int N, int timeout);
int put_file(int sd, struct sockaddr_in server, char *file, char *dir_path, int N, int timeout, int adapt);
