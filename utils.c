//utils.c

/*
================================================================================
Funzioni di I/O
================================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "types.h"

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