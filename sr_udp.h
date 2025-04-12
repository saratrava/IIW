typedef struct message{

  char *cmd;	//comando
  char *mess;	//contenuto del messaggio

} message;

typedef struct conn_arg{
	int port;		//numero di porta
	int window;		//dimensione della finestra
	int timeout;	//timeout
	int adapt;		//adaptive timeout flag
} conn_arg;


void print_error(int i,char *string);
void print_success(char *string);
int send_mess(int sd, struct sockaddr_in server, struct message *m);
int recv_mess(int sd, struct sockaddr_in *server, socklen_t size, struct message *m, int sec, int usec);
int find_port(int sd, struct sockaddr_in client, int start, int maxcon);
int connect_server(int sd, struct sockaddr_in client, conn_arg ca);
int listFunc(int sd, struct sockaddr_in client,int N, int start_timeout, int adapt, char* path);
int getFunc(int sd, struct sockaddr_in client,char *file,int N, int timeout, int adapt);
int putFunc(int sd, struct sockaddr_in client,char *file,int N);
