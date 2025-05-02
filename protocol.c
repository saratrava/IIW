/*
================================================================================
Timeout and Pipelining
================================================================================
*/

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "types.h"
#include "reliable_com.h"
#include "utils.h"

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
        rdt_send(ta->sd, ta->addr, ta->m);  // Invia il messaggio
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