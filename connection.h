#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

int syn_handshake_client(int sd, struct sockaddr_in server);
int syn_handshake_server(int sd, struct sockaddr_in client);
int find_port(int sd, struct sockaddr_in client, int start, int maxcon);
int connect_client(int default_port, char *server_ip, conn_arg *ca);
int connect_server(int sd, struct sockaddr_in client, conn_arg ca);
void close_conn(int sd, struct sockaddr_in server);
