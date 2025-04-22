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

#define MAX 4096         // Dimensione della PDU
#define PROB 0.125       // Probabilità simulata di errore

// PARAMETRI PER IL TIMEOUT ADATTIVO ----------------------------------------------------------------------------
#define ALPHA 0.125
#define BETA 0.25
//--------------------------------------------------------------------------------------------------------------

/*
typedef struct message {
    char *cmd;      // Comando
    char *mess;     // Contenuto del messaggio
} message;

typedef struct conn_arg {
    int port;       // Numero di porta
    int window;     // Dimensione della finestra
    int timeout;    // Timeout
    int adapt;      // Flag per il timeout adattivo
} conn_arg;

typedef struct thread_arg {
    int sd;                         // Socket
    struct sockaddr_in addr;        // Indirizzo a cui inviare i messaggi
    struct message *m;              // Messaggio da inviare
    long t;                         // Timeout
    long *time;                     // Indirizzo del tempo iniziale
} thread_arg;

typedef struct adaptive_tm {
    long est_rtt;
    long dev_rtt;
} adaptive_tm;


/*
================================================================================
Funzioni di I/O
================================================================================
*/

/*
 * @brief Visualizza un messaggio di errore evidenziato in rosso.
 * @param i Flag che, se attivo, utilizza perror; string Descrizione dell'errore.
 * @return Nessun valore restituito.
 */
void print_error(int i, char *string) {
    printf("\033[1;31m\n");
    if(i == 1){
        perror(string);
    }
    else{
        printf("%s\n", string);
    }
    printf("\033[0m\n\n");
}

/*
 * @brief Stampa un messaggio evidenziato in verde.
 * @param string Il testo da visualizzare.
 * @return Nessun valore restituito.
 */
void print_success(char *string) {
    printf("\033[1;32m\n\n");
    printf("%s", string);
    printf("\033[0m\n\n");
}

/*
 * @brief Trasferisce n byte da un buffer a un file.
 * @param f Puntatore al file dove scrivere i dati.
 * @param buf Buffer contenente i dati da scrivere.
 * @param n Numero di byte da trasferire.
 * @return Numero di byte effettivamente scritti.
 */
ssize_t writen(FILE *f, const void *buf, size_t n) {
    size_t nleft;
    const char *ptr;
    int i;
    char c;
    
    ptr = buf;
    nleft = n;
    i = 0;
    
    while (nleft > 0) {
        c = ptr[i];
        int res = fputc(c, f);
        //if (res == EOF) {
        //    return -1;
        //}
        nleft--;
        i++;
    }
    return(n - nleft);
}

/*
 * @brief Legge fino a n byte da un file e li memorizza in un buffer.
 * @param f Puntatore al file da leggere.
 * @param vptr Buffer in cui salvare i dati letti.
 * @param maxlen Dimensione massima dei byte da leggere.
 * @return Numero di byte letti.
 */
int readn(FILE* f, void *vptr, int maxlen) {
    int n, ch;
    char *ptr;
    
    ptr = vptr;
    for (n = 1; n < maxlen; n++) {
        if(ferror(f) != 0) {
            return -1;  // Errore
        }
        if ((ch = fgetc(f)) != EOF) { 
            ptr[n-1] = ch;
        } 
        else break;      // Interruzione della lettura
    }
    ptr[n-1] = '\0';
    return(n);  // Numero di byte letti
}


/*
================================================================================
Selective Repeat UDP
================================================================================
*/

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


/*
================================================================================
Connessione Client-Server
================================================================================
*/

/*
 * @brief Implementa un semplice 3-Way Handshake lato Client utilizzando UDP.
 *
 *        Fase 1: Il client invia un messaggio "SYN" al server per iniziare la connessione.
 *        Fase 2: Il client attende una risposta "SYN-ACK" contenente eventuali parametri.
 *        Fase 3: Il client invia un messaggio "ACK" per confermare la ricezione e completare il handshake.
 *
 * @param sd Descrittore del socket.
 * @param server Struttura contenente l’indirizzo del server.
 * @return Ritorna 0 se lo handshake va a buon fine, -1 in caso di errore.
 */
int syn_handshake_client(int sd, struct sockaddr_in server) {
    message m;
    char *line = malloc(MAX+20);
    socklen_t serlen = sizeof(server);

    // Fase 1: Invia SYN
    m.cmd = "SYN";
    m.mess = NULL;
    memcpy(line, m.cmd, 3);

    if(sendto(sd, line, MAX+20, 0, (struct sockaddr *)&server, sizeof(server)) < 0) {
        print_error(1, "Error sending SYN");
        return -1;
    }
    
    // Fase 2: Attende SYN-ACK

    if(recvfrom(sd, line, MAX+20, 0, (struct sockaddr *)&server, &serlen) < 0) {
        print_error(1, "Error receiving SYN-ACK");
        return -1;
    }
    printf("Client: ricevuto SYN-ACK: cmd='%s', mess='%s'\n", m.cmd, m.mess);
    if(strncmp(line, "SYN-ACK", 7) != 0) {
        print_error(0, "Handshake error: Expected SYN-ACK");
        return -1;
    }

    // Fase 3: Invia ACK
    m.cmd = "ACK";
    m.mess = NULL;
    memcpy(line, m.cmd, 20);
    if(sendto(sd, line, MAX+20, 0, (struct sockaddr *)&server, &serlen) < 0) {
        print_error(1, "Error sending ACK");
        return -1;
    }
    
    return 0;
}

/*
 * @brief Implementa un semplice 3-Way Handshake lato Server utilizzando UDP.
 *
 *        Fase 1: Il server attende un messaggio "SYN" dal client.
 *        Fase 2: Il server risponde con un messaggio "SYN-ACK" che può contenere parametri di connessione.
 *        Fase 3: Il server attende il messaggio "ACK" dal client per completare il handshake.
 *
 * @param sd Descrittore del socket.
 * @param client Puntatore alla struttura che conterrà l'indirizzo del client.
 * @return Ritorna 0 se lo handshake va a buon fine, -1 in caso di errore.
 */
int syn_handshake_server(int sd, struct sockaddr_in client) {
    message m;
    char *line = malloc(MAX+20);
    socklen_t clilen = sizeof(client);
    struct sockaddr_in server;
    socklen_t serlen = sizeof(server);
    
    // Allocazione dei buffer per ricevere i messaggi
    m.cmd = malloc(20);
    if(m.cmd == NULL) {
        print_error(1, "Memory allocation error for m.cmd");
        return -1;
    }
    m.mess = malloc(MAX);
    if(m.mess == NULL) {
        print_error(1, "Memory allocation error for m.mess");
        free(m.cmd);
        return -1;
    }
    
    // Riceve il messaggio SYN dal client
    if(recvfrom(sd, line, 3, 0, (struct sockaddr *)&client, sizeof(client)) < 0) {
        print_error(1, "Error receiving SYN");
        free(m.cmd);
        free(m.mess);
        return -1;
    }
    // Controlla il contenuto
    if(strncmp(line, "SYN", 3) != 0) {
        print_error(0, "Handshake error: Expected SYN");
        free(m.cmd);
        free(m.mess);
        return -1;
    }
    printf("Server: ricevuto SYN: '%s'\n", line);
    
    // Preparazione del SYN-ACK
    char params[MAX];
    snprintf(params, MAX, "%d %d %d %d", 8080, 5, 10000, 1);  
    free(m.cmd); 
    m.cmd = "SYN-ACK";
    memcpy(line, m.cmd, 20);
    if(m.cmd == NULL) {
        print_error(1, "Memory allocation error for SYN-ACK cmd");
        free(m.mess);
        return -1;
    }
    m.mess = strdup(params);
    if(m.mess == NULL) {
        print_error(1, "Memory allocation error for SYN-ACK mess");
        free(m.cmd);
        return -1;
    }
    
    if(sendto(sd, line, MAX+20, 0, (struct sockaddr *)&server, &serlen) < 0) {
        print_error(1, "Error sending SYN-ACK");
        free(m.cmd);
        free(m.mess);
        return -1;
    }
    printf("Server: inviato SYN-ACK con parametri: %s\n", params);
    
    // Attende ACK dal client
    free(m.cmd);
    free(m.mess);
    
    m.cmd = malloc(20);
    m.mess = malloc(MAX);
    if(m.cmd == NULL || m.mess == NULL) {
        print_error(1, "Memory allocation error in ACK phase");
        free(m.cmd);
        free(m.mess);
        return -1;
    }
    
    if(recvfrom(sd, line, MAX+20, 0, (struct sockaddr *)&client, sizeof(client)) < 0) {
        print_error(1, "Error receiving ACK");
        free(m.cmd);
        free(m.mess);
        return -1;
    }
    if(strncmp(line, "ACK", 3) != 0) {
        print_error(0, "Handshake error: Expected ACK");
        free(m.cmd);
        free(m.mess);
        return -1;
    }
    
    free(m.cmd);
    free(m.mess);
    return 0;
}

/*
 * @brief Cerca una porta disponibile a partire da quella di inizio specificata.
 * @param sd Descrittore del socket.
 * @param client Struttura dell’indirizzo locale da impostare.
 * @param start Porta iniziale da cui partire la ricerca.
 * @param maxcon Numero massimo di tentativi (connessioni) consentiti.
 * @return Ritorna il numero della porta trovata se disponibile, -1 se nessuna porta risulta libera.
 */
int find_port(int sd, struct sockaddr_in client, int start, int maxcon) {
    int count, new_port;
    count = 0;
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = htonl(INADDR_ANY);

    for(int i = 0; i < maxcon; i++){
        count = (count+1) % (maxcon+1);
        new_port = start + count;
        client.sin_port = htons(new_port);     // Imposta il numero di porta

        if (bind(sd, (struct sockaddr *)&client, sizeof(client)) < 0) {
            if(errno != EADDRINUSE){
                return -1;  // Errore
            }
        } else {
            return new_port; 
        }
    }
    return -1;  // Nessuna porta libera
}

/*
 * @brief Inizializza la connessione lato client.
 * @param default_port Porta predefinita del server.
 * @param server_ip Indirizzo IP del server.
 * @param ca Puntatore a una struttura contenente i parametri di connessione (porta, dimensione finestra, timeout, flag adattivo).
 * @return Ritorna 0 se la connessione viene inizializzata correttamente, -1 in caso di errore.
 */
int connect_client(int sd, int default_port, char *server_ip, conn_arg *ca) {
    int count, i;
    struct sockaddr_in server;
    message m;
    count = 0;
    
    /*if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        print_error(1,"Error connecting to the server (3)");
        return -1;
    }*/
    memset((void *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(default_port);
    
    if(inet_pton(AF_INET, server_ip, &server.sin_addr) <= 0){
        print_error(1, "Error connecting to the server (4)");
        return -1;
    }
    
    // ESEMPIO DI 3-Way HANDSHAKE
    if(syn_handshake_client(sd, server) == -1) {
        print_error(0, "Error during 3-way handshake (client side)");
        close(sd);
        return -1;
    }
    
    // Dopo il 3-way handshake, il client può procedere a inviare la richiesta di connessione

reconnect:
    m.cmd = "conn";
    m.mess = NULL;

    if(count >= 5){
        print_error(0, "Error connecting to server: Service temporarily unavailable");
        close(sd);
        return -1;
    }
    if(send_mess(sd, server, &m) == -1){
        close(sd);
        return -1;
    }
    if(recv_mess(sd, &server, sizeof(server), &m, 1, 0) == -1){
        count++;
        goto reconnect;
    }
    if(strncmp(m.cmd, "err", 3) == 0){
        print_error(0, m.mess);         // Stampa l’errore ricevuto dal server
        close(sd);
        return -1;
    }
    // Estrae i parametri per la connessione
    ca->port = strtol(strtok(m.mess," "), NULL, 10);
    ca->window = strtol(strtok(NULL," "), NULL, 10);
    ca->timeout = strtol(strtok(NULL," "), NULL, 10);
    ca->adapt = strtol(strtok(NULL," "), NULL, 10);
    
    close(sd);
    return 0;
}

/*
 * @brief Configura la connessione lato server inviando i parametri al client.
 * @param sd Descrittore del socket.
 * @param client Indirizzo del client che stabilisce la connessione.
 * @param ca Struttura contenente i parametri necessari alla connessione.
 * @return Ritorna 0 se la configurazione va a buon fine, -1 in caso di problemi.
 */
int connect_server(int sd, struct sockaddr_in client, conn_arg ca) {
    message m;
    m.cmd = malloc(20);
    m.mess = malloc(MAX);
    sprintf(m.cmd, "conn");
    sprintf(m.mess, "%d %d %d %d", ca.port, ca.window, ca.timeout, ca.adapt);
    
    if(send_mess(sd, client, &m) == -1){
        return -1;
    }
    free(m.cmd);
    free(m.mess);
    return 0;
}

/*
 * @brief Notifica al server l’intenzione di terminare la connessione.
 * @param sd Descrittore del socket.
 * @param server Struttura contenente l’indirizzo del server.
 * @return Nessun valore restituito.
 */
void quit_conn(int sd, struct sockaddr_in server) {
    message m;
    m.cmd = "quit";
    m.mess = NULL;
    send_mess(sd, server, &m);
}


/*
================================================================================
Funzioni LIST - GET - PUT
================================================================================
*/

/*
 * @brief Calcola un timeout aggiornato utilizzando i meccanismi di stima RTT e deviazione.
 * @param atm Struttura contenente le stime del RTT e la deviazione corrente.
 * @param new Nuovo valore misurato del timeout.
 * @return Il nuovo timeout calcolato.
 */
long get_timeout(adaptive_tm *atm, long new) {
    atm->est_rtt = (1-ALPHA)*(atm->est_rtt) + ALPHA*new;
    atm->dev_rtt = (1-BETA)*(atm->dev_rtt) + BETA*abs(new - atm->est_rtt);
    return (atm->est_rtt + 4*(atm->dev_rtt));
}

/*
 * @brief Funzione eseguita in un thread per l’invio ripetuto di un messaggio.
 * @param arg Puntatore a una struttura contenente i parametri necessari al thread (socket, indirizzo, messaggio, timeout, ecc.).
 * @return Nessun valore restituito.
 */
void *thread_send(void *arg) {
    thread_arg *ta = (thread_arg *)arg;
    struct timeval tv;
    
    pthread_cleanup_push(free, ta);
    pthread_cleanup_push(free, ta->m);
    pthread_cleanup_push(free, ta->m->mess);
    pthread_cleanup_push(free, ta->m->cmd);
    
    int oldstate;
    int oldtype;
    long sec = 0;
    long usec = 0;
    long t = 0;
    
    while(ta->t >= 1000000) {
        sec += 1;
        ta->t -= 1000000;
    }
    usec = ta->t;
    
    // Consente al processo padre di interrompere il thread in ogni momento
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    
    while(1) {
        send_mess(ta->sd, ta->addr, ta->m);  // Invia il messaggio
        if(gettimeofday(&tv, NULL) == -1){
            print_error(1, "Error getting time");
        }
        t = (tv.tv_sec) * 1000000 + tv.tv_usec;
        *(ta->time) = t;
        sleep(sec);
        usleep(usec);   // Timeout in microsecondi
    }
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
}

/*
 * @brief Riceve un file tramite socket e lo salva sul file system.
 * @param sd Descrittore del socket.
 * @param addr Indirizzo del mittente.
 * @param fd Puntatore al file destinazione dove scrivere i dati ricevuti.
 * @param N Dimensione della finestra di ricezione.
 * @return Ritorna 0 se il trasferimento del file ha successo, -1 in caso di errore.
 */
int download_file(int sd, struct sockaddr_in addr, FILE* fd, int N) {
    char *window[N];
    int dim[N];
    char line[MAX];
    char *new_mess;
    int n, rcv_base, wn2, count, timeout, pkt;
    struct message *m = malloc(sizeof(struct message));
    int lRes;
    char* tokDim;
    char* empty = malloc(MAX);
    memset(empty, 0, MAX);
    
    rcv_base = 0;  // Numero del prossimo messaggio in sequenza atteso
    count = 0;
    for(int i = 0; i < N; i++){
        window[i] = NULL;
    }
    while(1) {
        if(recv_mess(sd, &addr, sizeof(addr), m, 5, 0) == -1){
            print_error(1, "Connection error");
            return -1;
        }
        if(strncmp(m->cmd, "list", 4) == 0 || strncmp(m->cmd, "get", 3) == 0 || strncmp(m->cmd, "put", 3) == 0) {
            if(count > 3) return -1;
            count++;
            if (strncmp(m->cmd, "put", 3) == 0) {
                sprintf(m->mess, "ok");
                sprintf(m->cmd, "put");
                send_mess(sd, addr, m);
            }
            free(m->cmd);
            free(m->mess);
            continue;
        }
        tokDim = strtok(m->cmd, "=");
        pkt = strtol(tokDim, NULL, 10);
        tokDim = strtok(NULL, "=");
        lRes = strtol(tokDim, NULL, 10);
        // Se viene ricevuto un messaggio di errore, lo stampa e termina
        if(strncmp(m->cmd, "err", 3) == 0) {
            print_error(0, m->mess);
            free(m->cmd);
            free(m->mess);
            return -1;
        }
        else if(strncmp(m->cmd, "done", 4) == 0) {
            n = strtol(m->mess, NULL, 10);
            memset(m->mess, 0, MAX);
            timeout = strtol(strtok(NULL, "="), NULL, 10);
        }
        else {
            n = pkt; // Numero del pacchetto ricevuto
        }
        if(rcv_base <= n && n < rcv_base + N) {   // Il messaggio rientra nella finestra di ricezione
            if((new_mess = malloc(MAX)) == NULL) {
                print_error(1, "Error in malloc");
                free(tokDim);
                free(empty);
                free(m);
                return -1;
            }
            memcpy(new_mess, m->mess, lRes);
            window[n % N] = new_mess;
            dim[n % N] = lRes;
            
            memset(m->cmd, 0, 20);
            memset(m->mess, 0, MAX);
            sprintf(m->cmd, "ack");
            sprintf(m->mess, "%d", n);
            send_mess(sd, addr, m); // Invio dell'ack
            free(m->cmd);
            free(m->mess);
            m->mess = NULL;
            if(n == rcv_base) {
                // Sposta la finestra in avanti
                while(window[rcv_base % N] != NULL) {
                    if(memcmp(window[rcv_base % N], empty, lRes) == 0) {
                        free(empty);
                        while(1) {
                            m->cmd = malloc(20);
                            memset(m->cmd, 0, 20);
                            sprintf(m->cmd, "done=20=%d", timeout);
                            send_mess(sd, addr, m);
                            free(m->cmd);
                            m->mess = NULL;
                            if(recv_mess(sd, &addr, sizeof(addr), m, 0, timeout*20) == -1) {
                                return 0;
                            }
                            if(strncmp(m->cmd, "close", 5) == 0) {
                                free(m->cmd);
                                free(m->mess);
                                break;
                            }
                            free(m->cmd);
                            free(m->mess);
                        }
                        return 0;
                    }
                    wn2 = writen(fd, window[rcv_base % N], dim[rcv_base % N] - 1);
                    free(window[rcv_base % N]);
                    window[rcv_base % N] = NULL;
                    rcv_base++;
                }
            }
        }
        else if(rcv_base - N <= n && n < rcv_base) {
            // Reinvia l’ack se il pacchetto è già stato ricevuto
            memset(m->cmd, 0, 20);
            memset(m->mess, 0, MAX);
            sprintf(m->cmd, "ack");
            sprintf(m->mess, "%d", n);
            send_mess(sd, addr, m);
            free(m->cmd);
            free(m->mess);
        }
    }
}

/*
 * @brief Trasmette un file al destinatario tramite socket.
 * @param sd Descrittore del socket.
 * @param addr Indirizzo del destinatario.
 * @param fd Puntatore al file sorgente da inviare.
 * @param N Dimensione della finestra per la trasmissione.
 * @param start_timeout Timeout iniziale.
 * @param adapt Flag che indica se utilizzare il timeout adattivo.
 * @param dim Dimensione totale del file da trasferire.
 * @return Ritorna 0 se il file viene inviato correttamente, -1 in caso di errori.
 */
int upload_file(int sd, struct sockaddr_in addr, FILE* fd, int N, int start_timeout, int adapt, int dim) {
    pthread_t window[N];
    long time[N];
    int send_base, next_seq, i, n, end_num, res;
    long t, timeout;
    message *m;
    message rm;
    thread_arg *arg;
    char *line;
    struct timeval *tv;
    adaptive_tm atm;
    long tot_time;
    int lRes = MAX;
    
    timeout = start_timeout;
    int count = 1;
    int c = 0;
    float av = start_timeout;
    send_base = 0;
    end_num = -1; // Valore dell'ack dell'ultimo pacchetto
    if((tv = (struct timeval *)malloc(sizeof(struct timeval))) == NULL) {
        print_error(0, "Error initializing upload: malloc failed");
        return -1;
    }
    if(adapt == 1) {
        atm.est_rtt = start_timeout;
        atm.dev_rtt = 0;
    }
    for(i = 0; i < N; i++){
        window[i] = 1; 
        time[i] = 0;
    }
    if(gettimeofday(tv, NULL) == -1) {
        print_error(1, "Error getting time");
    }
    for(next_seq = 0; next_seq < send_base + N; next_seq++){
        if(end_num != -1) break;
        line = malloc(MAX);
        memset(line, 0, MAX);
redo:
        if((res = readn(fd, line, MAX)) > 1){
            lRes = res;
            arg = malloc(sizeof(struct thread_arg));
            m = malloc(sizeof(struct message)); 
            m->mess = line;
            line = (char *)malloc(MAX);
            memset(line, 0, MAX);
            sprintf(line, "%d=%d", next_seq, lRes);
            m->cmd = line;
            arg->sd = sd;
            arg->addr = addr;
            arg->m = m;
            arg->t = timeout;
            arg->time = &time[next_seq % N];
            window[next_seq % N] = 0;
            dim = dim - MAX;            
            if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0){
                print_error(0, "Error creating thread");
                for(i = 0; i < N; i++){
                    if(window[i] != 0 && window[i] != 1){
                        pthread_detach(window[i]);
                        pthread_cancel(window[i]);
                    }
                }
                usleep(2 * start_timeout);
                return -1;
            }
            if(gettimeofday(tv, NULL) == -1){
                print_error(1, "Error getting time");
            }
            time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
        }
        else{
            if(res == -1){
                print_error(0, "Error reading file");
                goto redo;
            }
            arg = malloc(sizeof(struct thread_arg));
            m = malloc(sizeof(struct message));
            line = (char *)malloc(20);
            memset(line, 0, 20);          
            sprintf(line, "done=%d=%ld", lRes, timeout);
            m->cmd = line;
            line = (char *)malloc(MAX);
            memset(line, 0, MAX);    
            sprintf(line, "%d", next_seq);
            m->mess = line;
            arg->sd = sd;
            arg->addr = addr;
            arg->m = m;
            arg->t = timeout;
            arg->time = &time[next_seq % N];
            window[next_seq % N] = 0;
            if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0){
                print_error(0, "Error creating thread");
                for(i = 0; i < N; i++){
                    if(window[i] != 0 && window[i] != 1){
                        pthread_detach(window[i]);
                        pthread_cancel(window[i]);
                    }
                }
                usleep(2 * start_timeout);
                return -1;
            }
            if(gettimeofday(tv, NULL) == -1){
                print_error(1, "Error getting time");
            }
            time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
            end_num = next_seq;
            break;
        }
    }
    while(1){
        if(recv_mess(sd, &addr, sizeof(addr), &rm, 5, 0) == -1){
            for(i = 0; i < N; i++){
                if(window[i] != 0 && window[i] != 1){
                    pthread_detach(window[i]);
                    pthread_cancel(window[i]);
                }
            }
            free(tv);
            usleep(2 * start_timeout);
            return -1;
        }
        if(strncmp(rm.cmd, "done", 4) == 0){
            if(gettimeofday(tv, NULL) == -1){
                print_error(1, "Error getting time");
            }
            tot_time = ((tv->tv_sec) * 1000000 + tv->tv_usec) - tot_time;
            // L'altro lato ha terminato il download
            for(i = 0; i < N; i++){
                if(window[i] != 0 && window[i] != 1){
                    pthread_detach(window[i]);
                    pthread_cancel(window[i]);
                }
            }
            av = av / count;
            free(tv);
            while(1){
                memset(rm.cmd, 0, 20);
                sprintf(rm.cmd, "close=20");
                rm.mess = NULL;
                send_mess(sd, addr, &rm);
                free(rm.cmd);
                free(rm.mess);
                if(recv_mess(sd, &addr, sizeof(addr), &rm, 0, 10 * timeout) == -1){
                    break;
                }
                if(strncmp(rm.cmd, "done", 4) == 0){
                    continue;
                }
                else{
                    break;
                }
            }
            usleep(2 * start_timeout);
            return 0;
        }
        else if(strncmp(rm.cmd, "ack", 3) == 0){
            n = strtol(rm.mess, NULL, 10);  // Numero dell'ack
            if(adapt == 1){
                if(gettimeofday(tv, NULL) == -1){
                    print_error(1, "Error getting time");
                }
                t = (tv->tv_sec) * 1000000 + tv->tv_usec;
                t = t - time[n % N];
                timeout = get_timeout(&atm, t);
                if (timeout < 3000) timeout = 3000;
                av += timeout;
                count++;
            }
            if(send_base < n && n < send_base + N){
                // L'ack è nella finestra
                pthread_detach(window[n % N]);
                pthread_cancel(window[n % N]);
                window[n % N] = 0;
            }
            else if(send_base == n){
                // Sposta la finestra
                pthread_detach(window[send_base % N]);
                pthread_cancel(window[send_base % N]);
                window[send_base % N] = 0;
                i = 0;
                while(window[send_base % N] == 0 && i < N){
                    send_base++;
                    i++;
                }
                for(next_seq; next_seq < send_base + N; next_seq++){
                    if(end_num != -1) break;
                    line = malloc(MAX);
                    memset(line, 0, MAX);
redo2:
                    res = readn(fd, line, MAX);
                    if(res > 1){
                        lRes = res;
                        arg = malloc(sizeof(struct thread_arg));
                        m = malloc(sizeof(struct message));
                        m->mess = line;
                        line = (char *)malloc(MAX);
                        memset(line, 0, MAX);
                        sprintf(line, "%d=%d", next_seq, lRes);
                        m->cmd = line;
                        arg->sd = sd;
                        arg->addr = addr;
                        arg->m = m;
                        arg->t = timeout;
                        arg->time = &time[next_seq % N];
                        window[next_seq % N] = 0;
                        dim = dim - MAX;
                        if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0){
                            print_error(0, "Error creating thread");
                            for(i = 0; i < N; i++){
                                if(window[i] != 0 && window[i] != 1){
                                    pthread_detach(window[i]);
                                    pthread_cancel(window[i]);
                                }
                            }
                            usleep(2 * start_timeout);
                            return -1;
                        }
                        if(gettimeofday(tv, NULL) == -1){
                            print_error(1, "Error getting time");
                        }
                        time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
                    }
                    else{
                        if(res == -1){
                            print_error(0, "Error reading file");
                            goto redo2;
                        }
                        arg = malloc(sizeof(struct thread_arg));
                        m = malloc(sizeof(struct message));
                        free(line);
                        line = (char *)malloc(20);
                        memset(line, 0, 20);
                        sprintf(line, "done=%d=%ld", lRes, timeout);
                        m->cmd = line;
                        line = (char *)malloc(MAX);
                        memset(line, 0, MAX);
                        sprintf(line, "%d", next_seq);
                        m->mess = line;
                        arg->sd = sd;
                        arg->addr = addr;
                        arg->m = m;
                        arg->t = timeout;
                        arg->time = &time[next_seq % N];
                        window[next_seq % N] = 0;
                        if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0){
                            print_error(0, "Error creating thread");
                            for(i = 0; i < N; i++){
                                if(window[i] != 0 && window[i] != 1){
                                    pthread_detach(window[i]);
                                    pthread_cancel(window[i]);
                                }
                            }
                            usleep(2 * start_timeout);
                            return -1;
                        }
                        if(gettimeofday(tv, NULL) == -1){
                            print_error(1, "Error getting time");
                        }
                        time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
                        end_num = next_seq;
                        break;
                    }
                }
            }
        }
        else{
            if(c >= 3){
                for(i = 0; i < N; i++){
                    if(window[i] != 0 && window[i] != 1){
                        pthread_detach(window[i]);
                        pthread_cancel(window[i]);
                    }
                }
                usleep(2 * start_timeout);
                return -1;
            }
            c++;
        }
    }
}

/*
 * @brief Invia al client la lista dei file disponibili sul server.
 * @param sd Descrittore del socket.
 * @param client Indirizzo del destinatario.
 * @param N Dimensione della finestra di trasmissione.
 * @param start_timeout Timeout iniziale.
 * @param adapt Flag per l’utilizzo del timeout adattivo.
 * @param path Percorso della directory contenente i file.
 * @return Ritorna 0 se la lista viene inviata correttamente, -1 in caso di problemi.
 */
int listFunc(int sd, struct sockaddr_in client, int N, int start_timeout, int adapt, char *path) {
    DIR *d;
    struct dirent *dir;
    pthread_t window[N];
    long time[N];
    int send_base, next_seq, i, n, end_num, count;
    message *m;
    message rm;
    struct thread_arg *arg;
    char *line;
    struct timeval *tv;
    int lRes;
    long t, timeout;
    adaptive_tm atm;
    
    timeout = start_timeout;
    if((tv = (struct timeval *)malloc(sizeof(struct timeval))) == NULL) {
        print_error(1, "Error in malloc of timeval");
        return -1;
    }
    tv->tv_usec = 0;
    tv->tv_sec = 0;
    
    if(adapt == 1){
        atm.est_rtt = start_timeout;
        atm.dev_rtt = 0;
    }
    if((d = opendir(path)) == NULL){
        print_error(1, "Error opening directory");
        return -1;
    }
    send_base = 0;
    end_num = -1; // Valore dell'ack dell'ultimo pacchetto
    count = 0;
    for(i = 0; i < N; i++){
        window[i] = 1;
        time[i] = 0; 
    }
    for(next_seq = 0; next_seq < send_base + N; next_seq++){
        if(end_num != -1) break;
        arg = malloc(sizeof(struct thread_arg));
        m = malloc(sizeof(struct message));
redo:
        if((dir = readdir(d)) != NULL ){
            if(dir->d_type == DT_REG){ // Considera solo file regolari, esclude le directory
                line = (char *)malloc(MAX);
                memset(line, 0, MAX);
                sprintf(line, "%s\n", dir->d_name);
                m->mess = line;
                lRes = strlen(m->mess) + 1;
                line = (char *)malloc(MAX);
                memset(line, 0, MAX);
                sprintf(line, "%d=%d", next_seq, lRes);
                m->cmd = line;
                arg->sd = sd;
                arg->addr = client;
                arg->m = m;
                arg->t = timeout;
                arg->time = &time[next_seq % N];
                window[next_seq % N] = 0;
                if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0) {
                    print_error(0, "Error creating thread");
                    for(i = 0; i < N; i++){
                        if(window[i] != 0 && window[i] != 1){
                            pthread_detach(window[i]);
                            pthread_cancel(window[i]);
                        }
                    }
                    return -1;
                }
                if(gettimeofday(tv, NULL) == -1){
                    print_error(1, "Error getting time");
                }
                time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
            }
            else {
                goto redo;
            }
        }
        else {
            line = (char *)malloc(20);
            memset(line, 0, 20);
            sprintf(line, "done=20=%ld", timeout);
            m->cmd = line;
            line = (char *)malloc(MAX);
            memset(line, 0, MAX);
            sprintf(line, "%d", next_seq);
            m->mess = line;
            arg->sd = sd;
            arg->addr = client;
            arg->m = m;
            arg->t = timeout;
            arg->time = &time[next_seq % N];
            window[next_seq % N] = 0;
            if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0) {
                print_error(0, "Error creating thread");
                for(i = 0; i < N; i++){
                    if(window[i] != 0 && window[i] != 1){
                        pthread_detach(window[i]);
                        pthread_cancel(window[i]);
                    }
                }
                return -1;
            }
            if(gettimeofday(tv, NULL) == -1){
                print_error(1, "Error getting time");
            }
            time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
            closedir(d);
            end_num = next_seq;
            break;
        }
    }
    while(1){    
        if(recv_mess(sd, &client, sizeof(client), &rm, 0, 500 * timeout) == -1){
            for(i = 0; i < N; i++){
                if(window[i] != 0 && window[i] != 1){
                    pthread_detach(window[i]);
                    pthread_cancel(window[i]);
                }
            }
            free(tv);
            return -1;
        }
        if(strncmp(rm.cmd, "done", 4) == 0){
            // Il lato opposto ha terminato il download
            for(i = 0; i < N; i++){
                if(window[i] != 0 && window[i] != 1){
                    pthread_detach(window[i]);
                    pthread_cancel(window[i]);
                }
            }
            while(1){
                memset(rm.cmd, 0, 20);
                strcpy(rm.cmd, "close=20");
                rm.mess = NULL;
                send_mess(sd, client, &rm);
                free(rm.cmd);
                if(recv_mess(sd, &client, sizeof(client), &rm, 0, 5 * timeout) == -1){
                    break;
                }
                if(strncmp(rm.cmd, "done", 4) == 0){
                    continue;
                }
                else {
                    free(rm.cmd);
                    free(rm.mess);
                    break;
                }
            }
            return 0;
        }
        else if(strncmp(rm.cmd, "ack", 3) == 0){
            n = strtol(rm.mess, NULL, 10);  // Numero dell'ack
            if(adapt == 1){
                if(gettimeofday(tv, NULL) == -1){
                    print_error(1, "Error getting time");
                }
                t = (tv->tv_sec) * 1000000 + tv->tv_usec;
                t = t - time[n % N];
                timeout = get_timeout(&atm, t);
            }
            if(send_base < n && n < send_base + N){
                // L'ack è nella finestra
                pthread_detach(window[n % N]);
                pthread_cancel(window[n % N]);
                window[n % N] = 0;
            }
            else if(send_base == n){
                // Sposta la finestra
                pthread_detach(window[send_base % N]);
                pthread_cancel(window[send_base % N]);
                window[send_base % N] = 0;
                i = 0;
                while(window[send_base % N] == 0 && i < N){
                    send_base++;
                    i++;
                }
                for(next_seq; next_seq < send_base + N; next_seq++){
redo2:
                    if((dir = readdir(d)) != NULL ){
                        if(dir->d_type == DT_REG){
                            arg = malloc(sizeof(struct thread_arg));
                            m = malloc(sizeof(struct message));
                            line = (char *)malloc(MAX);
                            memset(line, 0, MAX);
                            sprintf(line, "%s\n", dir->d_name);
                            m->mess = line;
                            lRes = strlen(m->mess) + 1;
                            line = (char *)malloc(MAX);
                            memset(line, 0, MAX);
                            sprintf(line, "%d=%d", next_seq, lRes);
                            m->cmd = line;
                            arg->sd = sd;
                            arg->addr = client;
                            arg->m = m;
                            arg->t = timeout;
                            arg->time = &time[next_seq % N];
                            window[next_seq % N] = 0;
                            if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0){
                                print_error(0, "Error creating thread");
                                for(i = 0; i < N; i++){
                                    if(window[i] != 0 && window[i] != 1){
                                        pthread_detach(window[i]);
                                        pthread_cancel(window[i]);
                                    }
                                }
                                return -1;
                            }
                            if(gettimeofday(tv, NULL) == -1){
                                print_error(1, "Error getting time");
                            }
                            time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
                        }
                        else{
                            goto redo2;
                        }
                    }
                    else{
                        arg = malloc(sizeof(struct thread_arg));
                        m = malloc(sizeof(struct message));
                        line = (char *)malloc(20);
                        sprintf(line, "done=20=%ld", timeout);
                        m->cmd = line;
                        line = (char *)malloc(MAX);
                        memset(line, 0, MAX);      
                        sprintf(line, "%d", next_seq);
                        m->mess = line;
                        arg->sd = sd;
                        arg->addr = client;
                        arg->m = m;
                        arg->t = timeout;
                        arg->time = &time[next_seq % N];
                        window[next_seq % N] = 0;
                        if(pthread_create(&window[next_seq % N], NULL, thread_send, (void *)arg) != 0){
                            print_error(0, "Error creating thread");
                            for(i = 0; i < N; i++){
                                if(window[i] != 0 && window[i] != 1){
                                    pthread_detach(window[i]);
                                    pthread_cancel(window[i]);
                                }
                            }
                            return -1;
                        }
                        if(gettimeofday(tv, NULL) == -1){
                            print_error(1, "Error getting time");
                        }
                        time[next_seq % N] = (tv->tv_sec) * 1000000 + tv->tv_usec;
                        closedir(d);
                        end_num = next_seq;
                        break;
                    }
                }
            }
        }
        else{
            if(count >= 3){
                for(i = 0; i < N; i++){
                    if(window[i] != 0 && window[i] != 1){
                        pthread_detach(window[i]);
                        pthread_cancel(window[i]);
                    }
                }
                return -1;
            }
            count++;
        }
    }
}

/*
 * @brief Lato server: configura i parametri per il comando get e verifica l’esistenza del file richiesto.
 * @param sd Descrittore del socket.
 * @param client Indirizzo del client.
 * @param file Percorso del file da trasmettere.
 * @param N Dimensione della finestra.
 * @param timeout Timeout iniziale.
 * @param adapt Flag per il timeout adattivo.
 * @return Ritorna 0 se tutto procede correttamente, -1 se si verifica un errore.
 */
int getFunc(int sd, struct sockaddr_in client, char *file, int N, int timeout, int adapt) {
    char line[MAX];
    int fd;
    FILE *f;
    message m;
open:
    if((f = fopen(file, "rb")) == NULL){
        if(errno != EINTR){
            print_error(1, "Error opening file (get)");
            m.cmd = "err";
            sprintf(line, "Error opening file: File not found");
            m.mess = line;
            send_mess(sd, client, &m);
            return -1;
        }
        goto open;
    }
    fclose(f);
    f = fopen(file, "ab");
    if(f == NULL){
        print_error(1, "Error opening file (get)");
        m.cmd = "err";
        sprintf(line, "Error opening file: File initialization failed");
        m.mess = line;
        send_mess(sd, client, &m);
        return -1;
    }
    int dim = ftell(f);
    fclose(f);
    f = fopen(file, "rb");
    if(upload_file(sd, client, f, N, timeout, adapt, dim) == -1){
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

/*
 * @brief Lato server: configura i parametri per il comando put e controlla la presenza del file da ricevere.
 * @param sd Descrittore del socket.
 * @param client Indirizzo del client.
 * @param file Percorso del file da ricevere.
 * @param N Dimensione della finestra.
 * @return Ritorna 0 se il processo di ricezione viene avviato correttamente, -1 in caso di errori.
 */
int putFunc(int sd, struct sockaddr_in client, char *file, int N) {
    char line[MAX];
    FILE *f;
    int n, fd;
    message m;
    
    m.mess = malloc(MAX);
    m.cmd = malloc(20);
    memset(m.cmd, 0, 20);
    memset(m.mess, 0, MAX);
    
#ifdef debug
#else
open:
    if((fd = open(file, O_CREAT | O_WRONLY | O_EXCL, 0666)) == -1) {
        if(errno != EINTR){
            print_error(1, "Error opening file (put)");
            m.cmd = "err";
            if(errno == EEXIST){
                sprintf(m.mess, "Error opening file: File already exists, please rename it before trying again!");
            }
            else{
                sprintf(m.mess, "Error opening file: Something went wrong");
            }
            send_mess(sd, client, &m);
            free(m.mess);
            return -1;
        }
        goto open;
    }
    close(fd);
#endif

    sprintf(m.mess, "ok");
    sprintf(m.cmd, "put");
    send_mess(sd, client, &m);
    
    f = fopen(file, "wb");
    if(download_file(sd, client, f, N) == -1){
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

/*
 * @brief Invia una richiesta al server per ottenere la lista dei file disponibili.
 * @param sd Descrittore del socket.
 * @param server Indirizzo del server.
 * @param N Dimensione della finestra di ricezione.
 * @param timeout Timeout iniziale.
 * @return Ritorna 0 se la richiesta viene completata con successo, -1 se si verifica un errore.
 */
int list_files(int sd, struct sockaddr_in server, int N, int timeout) {
    struct message *m = malloc(sizeof(struct message));
    // Invio del comando al server
    int listRN = 0;
retryList:
    m->cmd = malloc(20);
    memset(m->cmd, 0, 20);
    sprintf(m->cmd, "list");
    m->mess = NULL;
    send_mess(sd, server, m);
    free(m->cmd);
    if(recv_mess(sd, &server, sizeof(server), m, 0, 5 * timeout) == -1){
        listRN++;
        goto retryList;
    }
    free(m->cmd);
    printf("\n\033[1m----- Server File list -----\033[0m\n\n");
    if(download_file(sd, server, stdout, N) != -1){
        printf("\n\033[1m----------------------------\033[0m\n\n");
        free(m);
        return 0;
    }
    else{
        printf("\n\033[1m----------------------------\033[0m\n\n");
        free(m);
        return -1;
    }
}

/*
 * @brief Richiede il download di un file dal server.
 * @param sd Descrittore del socket.
 * @param server Indirizzo del server.
 * @param file Nome del file richiesto.
 * @param dir_path Percorso della directory dove salvare il file scaricato.
 * @param N Dimensione della finestra di ricezione.
 * @param timeout Timeout iniziale.
 * @return Ritorna 0 se il file viene scaricato con successo, -1 in caso di errore.
 */
int get_file(int sd, struct sockaddr_in server, char *file, char *dir_path, int N, int timeout) {
    FILE* f;
    char line[MAX], path[MAX];
    int n, getRN = 0;
    struct message m;
    m.cmd = malloc(20);
    m.mess = malloc(MAX);
    char *app = malloc(20);
    memset(&m, 0, sizeof(m));
    sprintf(path, "%s%s", dir_path, file);
retryGet:
    memset(app, 0, 20);
    sprintf(app, "get");
    m.cmd = app;
    m.mess = file;
    if(send_mess(sd, server, &m) == -1){
        return -1;
    }
    if(recv_mess(sd, &server, sizeof(server), &m, 0, 5 * timeout) == -1){
        getRN++;
        goto retryGet;
    }
    if(strncmp(m.cmd, "err", 3) == 0){
        print_error(0, m.mess);
        return -1;
    }
    if((f = fopen(path, "wb")) == NULL){
        print_error(1, "Error opening file");
        return -1;
    }
    free(m.cmd);
    free(m.mess);
    if(download_file(sd, server, f, N) != -1){
        sprintf(line, "Download of file '%s' complete", file);
        print_success(line);
        memset(&line, 0, MAX);
        fclose(f);
        return 0;
    }
    else{
        fclose(f);
        return -1;
    }
}

/*
 * @brief Invia una richiesta al server per caricare (upload) un file.
 * @param sd Descrittore del socket.
 * @param server Indirizzo del server.
 * @param file Nome del file da inviare.
 * @param dir_path Percorso della directory da cui prelevare il file.
 * @param N Dimensione della finestra per la trasmissione.
 * @param timeout Timeout iniziale.
 * @param adapt Flag per l’utilizzo del timeout adattivo.
 * @return Ritorna 0 se l’upload avviene correttamente, -1 in caso di problemi.
 */
int put_file(int sd, struct sockaddr_in server, char *file, char *dir_path, int N, int timeout, int adapt) {
    char path[MAX], line[20];  
    int n, fd, putRN = 0;
    struct message m;
    FILE* f;
    char *app = malloc(20);
    memset(&m, 0, sizeof(m));
    memset(path, 0, MAX);
    sprintf(path, "%s%s", dir_path, file);
open:
    if((fd = open(path, O_RDONLY)) == -1){
        if(errno != EINTR){
            print_error(1, "Error opening file");
            return -1;
        }
        goto open;
    }
    close(fd);
retryPut:
    memset(app, 0, 20);
    sprintf(app, "put");
    m.cmd = app;
    m.mess = file;
    send_mess(sd, server, &m);
    if(recv_mess(sd, &server, sizeof(server), &m, 0, 5 * timeout) == -1){
        putRN++;
        goto retryPut;
    }
    if(strncmp(m.cmd, "err", 3) == 0){
        print_error(0, m.mess);
        return -1;
    }
    f = fopen(path, "ab");
    int dim = ftell(f);
    fclose(f);
    f = fopen(path, "rb");
    free(m.cmd);
    free(m.mess);
    if(upload_file(sd, server, f, N, timeout, adapt, dim) != -1){
        print_success("File successfully uploaded to the server!");
        fflush(stdout);
        fclose(f);
        return 0;
    }
    else{
        print_error(0, "Error sending file");
        fclose(f);
        return -1;
    }
}
*/

/*
================================================================================
Debug e Test del Trasferimento File
================================================================================
*/

/*
 * @brief Funzione di debug usata dal client per testare i comandi list, get e put.
 * @param args Struttura contenente i parametri di connessione.
 * @param sd Descrittore del socket.
 * @param server Indirizzo del server.
 * @param func Comando da testare (ad es. "list", "get", "put").
 * @param path Percorso della directory utilizzata per il test (sorgente o destinazione del file).
 * @return Nessun valore restituito.
 */
void file_transfer_debug(conn_arg args, int sd, struct sockaddr_in server, char *func, char *path) {
    int n, count, i, j;
    float total, average;
    char file[200];
    memset(&file, 0, 200);
    FILE *fd;
    struct timeval start, stop;
    char *filename = malloc(MAX);
    sprintf(filename, "%sN%dP%f.csv", func, args.window, PROB);
    
    if((fd = fopen(filename, "w+")) == NULL){
        print_error(1, "Error creating file");
        return;
    }
retry:
    printf("Insert a number : ");  // Richiede il numero di test da effettuare
    n = scanf("%d", &count);
    getchar();
    if(n <= 0){
        if(n == EOF){
            print_error(1, "Error reading input");
            fclose(fd);
            return;
        }
        goto retry;
    }
    if(strncmp(func, "list", 4) == 0){
        printf("\nWindow: %d\nError probability: %lf\nTimeout: %d\nAdaptive: %d\nPdu size: %d\nRepetitions: %d\n",
               args.window, PROB, args.timeout, args.adapt, MAX, count);
        printf("\n--- Starting system debug ---\n");
        fprintf(fd, "Num,Command,Tot_time");
        for(i = 1; i <= count; i++){
            printf("Test %d: ", i);
            fprintf(fd, "\n%d", i);
            fprintf(fd, ",list");
            if(gettimeofday(&start, NULL) == -1){
                print_error(1, "Error getting time");
            }
            if(list_files(sd, server, args.window, args.timeout) == -1){
                total = -1;
                fprintf(fd, ",%f", total);
                usleep(10 * args.timeout);
                continue;
            }
            if(gettimeofday(&stop, NULL) == -1){
                print_error(1, "Error getting time");
            }
            total = (stop.tv_sec * 1000000 + stop.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
            fprintf(fd, ",%f", total);
            usleep(args.timeout * 10);
        }
        printf("\n-----------------------------\n");
        fclose(fd);
    }
    else if(strncmp(func, "get", 3) == 0){
        printf("Insert the name of the file you want to use : ");
        n = scanf("%[^\n]", file);
        getchar();
retry1:
        if(n <= 0){
            if(n == EOF){
                print_error(1, "Error reading input");
                quit_conn(sd, server);
                fclose(fd);
                return;
            }
            goto retry1;
        }
        printf("\nWindow: %d\nError probability: %lf\nTimeout: %d\nAdaptive: %d\nPdu size: %d\nRepetitions: %d\n",
               args.window, PROB, args.timeout, args.adapt, MAX, count);
        printf("\n--- Starting system debug ---\n");
        fprintf(fd, "Num,Command,Tot_time");
        for(i = 1; i <= count; i++){
            printf("Test %d: ", i);
            fprintf(fd, "\n%d", i);
            fprintf(fd, ",get");
            if(gettimeofday(&start, NULL) == -1){
                print_error(1, "Error getting time");
            }
            if(get_file(sd, server, file, path, args.window, args.timeout) == -1){
                fprintf(fd, ",-1");
                usleep(10 * args.timeout);
                continue;
            }
            if(gettimeofday(&stop, NULL) == -1){
                print_error(1, "Error getting time");
            }
            total = (stop.tv_sec * 1000000 + stop.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
            fprintf(fd, ",%f", total);
            usleep(args.timeout * 10);
        }
        printf("\n-----------------------------\n");
        fclose(fd);
    }
    else if(strncmp(func, "put", 3) == 0){
        printf("Insert the name of the file you want to use : ");
        n = scanf("%[^\n]", file);
        getchar();
retry2:
        if(n <= 0){
            if(n == EOF){
                print_error(1, "Error reading input");
                quit_conn(sd, server);
                fclose(fd);
                return;
            }
            goto retry2;
        }
        printf("\nWindow: %d\nError probability: %lf\nTimeout: %d\nAdaptive: %d\nPdu size: %d\nRepetitions: %d\n",
               args.window, PROB, args.timeout, args.adapt, MAX, count);
        printf("\n--- Starting system debug ---\n");
        fprintf(fd, "Num,Command,Tot_time");
        for(i = 1; i <= count; i++){
            printf("Test %d: ", i);
            fprintf(fd, "\n%d", i);
            fprintf(fd, ",put");
            if(gettimeofday(&start, NULL) == -1){
                print_error(1, "Error getting time");
            }
            if(put_file(sd, server, file, path, args.window, args.timeout, args.adapt) == -1){
                fprintf(fd, ",-1");
                usleep(50 * args.timeout);
                continue;
            }
            if(gettimeofday(&stop, NULL) == -1){
                print_error(1, "Error getting time");
            }
            total = (stop.tv_sec * 1000000 + stop.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
            fprintf(fd, ",%f", total);
            usleep(50 * args.timeout);
        }
        printf("\n-----------------------------\n");
        fclose(fd);
    }
    else {
        print_error(0, "Wrong command: you can only debug the 'list', 'get', 'put' functions");
        fclose(fd);
        sprintf(filename, "rm %s.csv", func);
        system(filename);
    }
    free(filename);
}
