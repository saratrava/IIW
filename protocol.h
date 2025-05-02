#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

long get_timeout(adaptive_tm *atm, long new);
void *thread_send(void *arg);
