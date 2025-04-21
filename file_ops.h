// file_ops.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

int download_file(int sd, struct sockaddr_in addr, FILE* fd, int N);
int upload_file(int sd, struct sockaddr_in addr, FILE* fd, int N, int start_timeout, int adapt, int dim);
int listFunc(int sd, struct sockaddr_in client, int N, int start_timeout, int adapt, char *path);
int getFunc(int sd, struct sockaddr_in client, char *file, int N, int timeout, int adapt);
int putFunc(int sd, struct sockaddr_in client, char *file, int N);
int list_files(int sd, struct sockaddr_in server, int N, int timeout);
int get_file(int sd, struct sockaddr_in server, char *file, char *dir_path, int N, int timeout);
int put_file(int sd, struct sockaddr_in server, char *file, char *dir_path, int N, int timeout, int adapt);
