#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void debug_and_test(conn_arg args, int sd, struct sockaddr_in server, char *func, char *path);
