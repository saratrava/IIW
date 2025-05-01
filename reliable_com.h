//socket_comm.h

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

bool prob(double p);
void set_timeout(int socket, int sec, long usec);
int rdt_send(int sd, struct sockaddr_in server, struct message *m);
int rdt_rcv(int sd, struct sockaddr_in *server, socklen_t size, struct message *m, int sec, long usec);
