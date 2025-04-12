#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>

#include "sr_udp.h"

//-----------------------------------------------------------------------------------------------------------------------------
#define PORT 50000		//Porta di default del server
#define NUMPORT 15000	//Numero massimo di connessioni concorrenti
#define N 5				//Dimensione finestra
#define T 10000			//Timeout in microsecondi
#define ADAP 1			//Flag per il timeout adattivo
//-----------------------------------------------------------------------------------------------------------------------------

#define MAX 4096
#define PATH "server/files/"

int main(int argc, char **argv){
	srand ( time(NULL) );

	char port[MAX];
	char line[MAX];
	int sd,new_port;
	pid_t pid;
	struct sockaddr_in client;
	message m;
	struct sockaddr_in new_client;
	conn_arg ca;
	int new_sd;

	if((sd = socket(AF_INET,SOCK_DGRAM,0))<0){
		print_error(1,"Error in creating socket");
		return -1;
	}

	int sOpt=1;
	setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&sOpt,sizeof(int));

	memset((void *)&client, 0, sizeof(client));
 	client.sin_family = AF_INET;
  	client.sin_addr.s_addr = htonl(INADDR_ANY); /* il server accetta pacchetti su una qualunque delle sue interfacce di rete */
 	client.sin_port = htons(PORT); /* numero di porta del server */

 	/* assegna l'indirizzo al socket */
	if (bind(sd, (struct sockaddr *)&client, sizeof(client)) < 0) {
	  	print_error(1,"Error binding socket");
	  	return -1;
	}

	system("clear");
	print_success("Server started successfully. Waiting for clients...");

	while(1){


		if(recv_mess(sd,&client,sizeof(client),&m,0,0) == -1){
			print_error(1,"Error receiving message");
			continue;
		}

		if(strcmp(m.cmd,"conn") == 0){

			ca.window = N;
			ca.timeout = T;
			ca.adapt = ADAP;


			if((new_sd = socket(AF_INET,SOCK_DGRAM,0))<0){
				print_error(1,"Error creating socket");
				return -1;
			}

			memset((void *)&new_client, 0, sizeof(new_client));
			ca.port = find_port(new_sd,new_client,PORT,NUMPORT);	//Cerca una nuova porta per la connessione

			if(ca.port == -1){
				m.cmd = "err";
				m.mess = "Error starting connection: Server is full, please retry in a few minutes";

				send_mess(sd,client,&m);
				close(new_sd);
				continue;
			}

			if(connect_server(sd,client,ca) != -1){
				//la connessione è andata a buon fine
				pid = fork();

				if(pid == 0){
					//Processo figlio che si occupa del client
					close(sd);
					printf("[Port:%d] - Connected with new client, process ID: %d\n",ca.port,getpid());

					while(1){

						//Se il client non contatta il server per troppo tempo la comunicazione viene interrotta
						if(recv_mess(new_sd,&new_client,sizeof(new_client),&m,60*3,0) == -1){
							break;
						}

						printf("[Port:%d] - Command %s\n",ca.port,m.cmd);

						if (strcmp(m.cmd,"list")==0){
							if(listFunc(new_sd,new_client,N,T,ADAP,PATH) == -1) printf("[Port:%d] - Error sending list\n",ca.port);
						}
						else if (strcmp(m.cmd,"get")==0){
							sprintf(line,"%s%s",PATH,m.mess);
							if(getFunc(new_sd,new_client,line,N,T,ADAP) == -1) printf("[Port:%d] - Error sending file\n",ca.port);
							else printf("[Port:%d] - File '%s' successfully sent\n",ca.port,m.mess);
						}
						else if (strcmp(m.cmd,"put")==0){
							sprintf(line,"%s%s",PATH,m.mess);
							if(putFunc(new_sd,new_client,line,N) == -1) printf("[Port:%d] - Error receiving file\n",ca.port);
							else printf("[Port:%d] - File '%s' successfully received\n",ca.port,m.mess);
						}
						else if(strcmp(m.cmd,"quit")==0){
							break;
						}
						else printf("[Port:%d] - Wrong command\n",ca.port);
						fflush(stdout);

					}

					close(new_sd);
					printf("[Port:%d] - Closing connection with client\n",ca.port);
					
					exit(0);

				}
				else if(pid == -1){
					print_error(1,"Unable to start child process");
				}				
			}
			close(new_sd);	
		}
	}
}