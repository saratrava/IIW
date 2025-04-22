//TYPES.h

#include <stdbool.h>
#include <arpa/inet.h>

#include "sr_udp.h"
#include "types.h"

#define MAX 4096    // Dimensione della PDU


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

