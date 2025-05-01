/*
================================================================================
File Transfer Debug and Test
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
 * @brief Funzione di debug usata dal client per testare i comandi list, get e put.
 * @param args Struttura contenente i parametri di connessione.
 * @param sd Descrittore del socket.
 * @param server Indirizzo del server.
 * @param func Comando da testare (ad es. "list", "get", "put").
 * @param path Percorso della directory utilizzata per il test (sorgente o destinazione del file).
 * @return Nessun valore restituito.
 */
void debug_and_test(conn_arg args, int sd, struct sockaddr_in server, char *func, char *path) {
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
                close_conn(sd, server);
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
                close_conn(sd, server);
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
