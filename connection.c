/*
================================================================================
Client-Server Connection
================================================================================
*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "utils.h"
#include "types.h"
#include "reliable_com.h"

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
int connect_client(int default_port, char *server_ip, conn_arg *ca) {
    int count, i, sd;
    struct sockaddr_in server;
    message m;
    count = 0;
    
    if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        print_error(1,"Error connecting to the server (3)");
        return -1;
    }
    memset((void *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(default_port);
    
    if(inet_pton(AF_INET, server_ip, &server.sin_addr) <= 0){
        print_error(1, "Error connecting to the server (4)");
        return -1;
    }
    
    /*// ESEMPIO DI 3-Way HANDSHAKE
    if(syn_handshake_client(sd, server) == -1) {
        print_error(0, "Error during 3-way handshake (client side)");
        close(sd);
        return -1;
    }
    
    // Dopo il 3-way handshake, il client può procedere a inviare la richiesta di connessione*/

reconnect:
    m.cmd = "conn";
    m.mess = NULL;

    if(count >= 5){
        print_error(0, "Error connecting to server: Service temporarily unavailable");
        close(sd);
        return -1;
    }
    if(rdt_send(sd, server, &m) == -1){
        close(sd);
        return -1;
    }
    if(rdt_rcv(sd, &server, sizeof(server), &m, 1, 0) == -1){
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
    
    if(rdt_send(sd, client, &m) == -1){
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
void close_conn(int sd, struct sockaddr_in server) {
    message m;
    m.cmd = "quit";
    m.mess = NULL;
    rdt_send(sd, server, &m);
}