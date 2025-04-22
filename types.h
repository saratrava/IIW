//TYPES.h

#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX 4096    // Dimensione della PDU

/**
 * @brief Struttura per comando e payload
 * - cmd: stringa di comando (fissa a 20 byte)
 * - mess: payload (fino a MAX_PDU_SIZE byte)
 */
typedef struct message {
    char *cmd;      // Comando
    char *mess;     // Contenuto del messaggio
} message;

/**
 * @brief Argomenti di configurazione della connessione
 * - port: porta di destinazione
 * - window: dimensione della finestra SR
 * - timeout: timeout iniziale (microsec)
 * - adapt: flag timeout adattivo
 */
typedef struct conn_arg {
    int port;       // Numero di porta
    int window;     // Dimensione della finestra
    int timeout;    // Timeout
    int adapt;      // Flag per il timeout adattivo
} conn_arg;


/**
 * @brief Argomenti passati ai thread di ritrasmissione
 * - sd. socket descriptor
 * - addr: indirizzo destinatario
 * - m: puntatore a messaggio da inviare
 * - t: timeout in microsec
 * - time: puntatore a variabile per savlare timestamp invio
 */
typedef struct thread_arg {
    int sd;                         // Socket
    struct sockaddr_in addr;        // Indirizzo a cui inviare i messaggi
    struct message *m;              // Messaggio da inviare
    long t;                         // Timeout
    long *time;                     // Indirizzo del tempo iniziale
} thread_arg;


/**
 * @brief Struttura per stima adattiva del timeout
 * - est_rtt: stima corrente del RTT 
 * - dev_rtt: stima corrente della devianza dell'RTT
 */
typedef struct adaptive_tm {
    long est_rtt;
    long dev_rtt;
} adaptive_tm;

