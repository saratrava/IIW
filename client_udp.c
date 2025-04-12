#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "sr_udp.h"

//---------------------------------------------------------------------------------------------------------------
#define IP "127.0.0.1"			//Ip server
#define DEFAULT 50000			//Porta di default del server
#define PATH "client/files/"	//Path della cartella dove si vogliono salvare i file
//---------------------------------------------------------------------------------------------------------------

int sd;
struct sockaddr_in server;

void say_hello();

void signal_handler(int signum){
	
	if(signum == SIGALRM){
		printf("\nTime limit exceeded. Disconnecting...\n");	
	} 
	quit_conn(sd,server);
	close(sd);
	exit(0);
}

int main(int argc, char *argv[]){

	int n;
	char input[300];
	memset(&input,0,300);
	char *cmd;
	char *par;
	conn_arg ca;

	if((sd = socket(AF_INET,SOCK_DGRAM,0))<0){
		print_error(1,"Error connecting to server (1)");
		return -1;
	}

	memset((void *)&server,0,sizeof(server));
	server.sin_family = AF_INET;

	if(inet_pton(AF_INET,IP,&server.sin_addr)<=0){
		print_error(1,"Error connecting to server (2)");
		close(sd);
		return -1;
	}

	if(connect_client(DEFAULT,IP,&ca) == -1){	//Contatta il server per ricevere i parametri di connessione
		close(sd);
		return -1;
	}

	server.sin_port = htons(ca.port);

	signal(SIGINT,signal_handler);
	signal(SIGALRM,signal_handler);

	system("clear");
	say_hello();
	while(1){

		alarm(60*3);	//se il client rimane inattivo per troppo tempo chiude la connessione

retry:
		printf("-> ");
		n = scanf("%[^\n]",input);
		getchar();

		if(n <= 0){
			if(n == EOF){
				print_error(1,"Error reading input");
				quit_conn(sd,server);
				close(sd);
				return -1;
			}
			goto retry;
		}

		alarm(0);

		cmd = strtok(input," ");

		if(strcmp(cmd,"get") == 0){
			par = strtok(NULL," ");
			if(par == NULL){
				print_error(0,"Wrong command: use get <file_name>");
				continue;
			}
			get_file(sd,server,par,PATH,ca.window, ca.timeout);
		}
		else if(strcmp(cmd,"put") == 0){
			par = strtok(NULL," ");
			if(par == NULL){
				print_error(0,"Wrong command: use put <file_name>");
				continue;
			}
			put_file(sd,server,par,PATH,ca.window,ca.timeout,ca.adapt);
		}
		else if(strcmp(cmd,"list") == 0){
			list_files(sd,server,ca.window,ca.timeout);
		}
		else if(strcmp(cmd,"q") == 0){
			quit_conn(sd,server);	//segnala al server che si intende chiudere la comunicazione
			close(sd);
			break;
		}
		else if(strcmp(cmd,"ls") == 0){	
			printf("\n");
			system("ls client/files/");	//stampa la lista dei file disponibili per l'upload su server
			printf("\n");
		}										
		else if(strcmp(cmd,"help") == 0){
			printf("\n");
			say_hello();	//stampa nuovamente la lista dei comandi
		}
		else if(strcmp(cmd,"test") == 0){
			#ifdef debug
				par = strtok(NULL," ");
				if(par == NULL){
					print_error(0,"Wrong command: use test <func_name>");
					continue;
				}				

				file_transfer_debug(ca,sd,server,par,PATH);
			#else
				print_error(0,"Command not found");
			#endif
		}
		else{
			print_error(0,"Command not found");			
		}
	}

	exit(0);
}

void say_hello(){

	printf("\033[1m -------------------------------------------------------- \n");
	printf("| WELCOME                                                |\n");
	printf("|--------------------------------------------------------|\n");
	printf("|                                                        |\n");
	printf("| list              Show the list of files on the server |\n");
	printf("| get <file_name>   Download a file from the server      |\n");
	printf("| put <file_name>   Upload a file on the server          |\n");
	printf("|                                                        |\n");
	printf("| ls                Lists the files available for upload |\n");
	printf("| help              Shows this command list              |\n");
#ifdef debug
	printf("| test <func_name>  Test the function efficiency         |\n");
#endif
	printf("| q                 Quit                                 |\n");
	printf("|                                                        |\n");
	printf(" --------------------------------------------------------\033[0m \n\n");

}