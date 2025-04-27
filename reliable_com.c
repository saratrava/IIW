//socket_comm.c

/*
================================================================================
Selective Repeat UDP
================================================================================
*/

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

#include "types.h"



/*
 * @brief Simula la perdita di un pacchetto mediante un valore casuale.
 * @param p Probabilità che il pacchetto venga considerato perso.
 * @return Ritorna true se il pacchetto deve essere scartato, false altrimenti.
 */
 bool prob(double p) {
    return rand() < p * ((double)RAND_MAX + 1.0);
}

/*
 * @brief Configura il timeout di ricezione di un socket.
 * @param socket Descrittore del socket da configurare.
 * @param sec Numero di secondi per il timeout.
 * @param usec Numero di microsecondi per il timeout.
 * @return Nessun valore restituito.
 */
void set_timeout(int socket, int sec, long usec) {
    struct timeval tv;
    tv.tv_sec = sec;         // Secondi

    while(usec >= 1000000){
        tv.tv_sec += 1;
        usec -= 1000000;
    }
    tv.tv_usec = usec;
    
    if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) == -1) {
        print_error(1, "Error setting timeout");
        fflush(stdout);
        exit(-1);
    }
}

/*
 * @brief Invia un messaggio tramite un socket UDP.
 * @param sd Descrittore del socket.
 * @param server Struttura contenente l’indirizzo del destinatario.
 * @param m Struttura che contiene il comando e il messaggio da inviare.
 * @return Ritorna 0 se l’invio va a buon fine, -1 in caso di errore.
 */
int send_mess(int sd, struct sockaddr_in server, struct message *m) {
    char *line = malloc(20);
    char *line2 = malloc(MAX);
    char *lineT = malloc(MAX+20);
    memset(line, 0, 20);
    memset(line2, 0, MAX);
    memset(lineT, 0, 20+MAX);

    if(prob(PROB)){
        return 0;
    }
    if(m->cmd != NULL){
        memcpy(line, m->cmd, 20);
    }
    else sprintf(line, "(null)");

    if(m->mess != NULL){
        memcpy(line2, m->mess, MAX);
    }
    else sprintf(line2,"(null)");

    for (int i = 0; i < 20; i++){
        lineT[i] = line[i];
    }
    for (int i = 0; i < MAX; i++){
        lineT[20+i] = line2[i];
    }
    if(sendto(sd, lineT, 20+MAX, 0, (struct sockaddr *)&server, sizeof(server)) < 0){
        return -1;  // Errore
    }
    fflush(stdout);
    return 0;
}

/*
 * @brief Riceve un messaggio dal socket e lo memorizza in una struttura.
 * @param sd Descrittore del socket.
 * @param server Puntatore alla struttura che memorizza l’indirizzo del mittente.
 * @param size Dimensione della struttura dell’indirizzo.
 * @param m Struttura in cui verranno copiati il comando e il messaggio ricevuti.
 * @param sec Numero di secondi per il timeout.
 * @param usec Numero di microsecondi per il timeout.
 * @return Ritorna 0 se l’operazione ha successo, -1 in caso di fallimento.
 */
int recv_mess(int sd, struct sockaddr_in *server, socklen_t size, struct message *m, int sec, long usec) {
    int n;
    char line[MAX+20];
    
    memset(m, 0, sizeof(m));
    set_timeout(sd, sec, usec);     // Imposta il timeout
    
    if((n = recvfrom(sd, line, MAX+20, 0, (struct sockaddr *)server, &size)) < 0){
        return -1;  // Errore
    }
    if(line != NULL){
        m->cmd = malloc(20);
        m->mess = malloc(MAX);
        for (int i = 0; i < 20; i++){
            m->cmd[i] = line[i];
        } 
        for (int i = 0; i < MAX; i++){
            m->mess[i] = line[i+20];
        }
    }
    return 0;
}