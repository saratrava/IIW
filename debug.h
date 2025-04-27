#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void file_transfer_debug(conn_arg args, int sd, struct sockaddr_in server, char *func, char *path);
