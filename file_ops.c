/*
================================================================================
LIST - GET - PUT 
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

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "types.h"
#include "protocol.h"
#include "reliable_com.h"
#include "utils.h"

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
        if(rdt_rcv(sd, &addr, sizeof(addr), m, 5, 0) == -1){
            print_error(1, "Connection error");
            return -1;
        }
        if(strncmp(m->cmd, "list", 4) == 0 || strncmp(m->cmd, "get", 3) == 0 || strncmp(m->cmd, "put", 3) == 0) {
            if(count > 3) return -1;
            count++;
            if (strncmp(m->cmd, "put", 3) == 0) {
                sprintf(m->mess, "ok");
                sprintf(m->cmd, "put");
                rdt_send(sd, addr, m);
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
            rdt_send(sd, addr, m); // Invio dell'ack
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
                            rdt_send(sd, addr, m);
                            free(m->cmd);
                            m->mess = NULL;
                            if(rdt_rcv(sd, &addr, sizeof(addr), m, 0, timeout*20) == -1) {
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
            rdt_send(sd, addr, m);
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
        if(rdt_rcv(sd, &addr, sizeof(addr), &rm, 5, 0) == -1){
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
                rdt_send(sd, addr, &rm);
                free(rm.cmd);
                free(rm.mess);
                if(rdt_rcv(sd, &addr, sizeof(addr), &rm, 0, 10 * timeout) == -1){
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
        if(rdt_rcv(sd, &client, sizeof(client), &rm, 0, 500 * timeout) == -1){
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
                rdt_send(sd, client, &rm);
                free(rm.cmd);
                if(rdt_rcv(sd, &client, sizeof(client), &rm, 0, 5 * timeout) == -1){
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
            rdt_send(sd, client, &m);
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
        rdt_send(sd, client, &m);
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
            rdt_send(sd, client, &m);
            free(m.mess);
            return -1;
        }
        goto open;
    }
    close(fd);
#endif

    sprintf(m.mess, "ok");
    sprintf(m.cmd, "put");
    rdt_send(sd, client, &m);
    
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
    rdt_send(sd, server, m);
    free(m->cmd);
    if(rdt_rcv(sd, &server, sizeof(server), m, 0, 5 * timeout) == -1){
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
    if(rdt_send(sd, server, &m) == -1){
        return -1;
    }
    if(rdt_rcv(sd, &server, sizeof(server), &m, 0, 5 * timeout) == -1){
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
    rdt_send(sd, server, &m);
    if(rdt_rcv(sd, &server, sizeof(server), &m, 0, 5 * timeout) == -1){
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