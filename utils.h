#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void print_error(int i, char *string);
void print_success(char *string);
ssize_t writen(FILE *f, const void *buf, size_t n);
int readn(FILE* f, void *vptr, int maxlen);
