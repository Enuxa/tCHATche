#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "h/protocol.h"

char* make_header(char *buff, int length, char *type) {
    sprintf(buff, "%04d%s", length, type);
    return buff + MIN_REQUEST_LENGTH;
}

char* add_number(char *buff, int n) {
    sprintf(buff, "%04d", n);
    return buff + 4;
}

char* add_lnumber(char *buff, long n) {
    sprintf(buff, "%08ld", n);
    return buff + 8;
}

char *add_string(char *buff, char *str) {
    sprintf(buff, "%04d%s", (int) strlen(str), str);
    return buff + 4 + strlen(str);
}

void monitor_request(request *req) {
    printf("\n\033[1mTaille\033[0m\t\t%d\n", req->length);
    printf("\033[1mType\033[0m\t\t%s\n", req->type);
    printf("\033[1mContenu\033[0m\t\t\033[33m%s\033[0m\n\n", req->content);
}

request *read_request(char *buff) {
    request *req = calloc(1, sizeof(request));
    req->type = calloc(5, 1);

    //  Lecture de la longueur de la requête
    if (sscanf(buff, "%d", &(req->length)) != 1) {
        free(req->type);
        free(req);
        return NULL;
    }

    req->length -= MIN_REQUEST_LENGTH;

    //  Lecture du tye de la requête
    for (int i = 0; i < 4; i++)
        req->type[i] = buff[4 + i];
    req->type[4] = '\0';

    //  Lecture du contenu de la requête
    req->content = calloc(req->length, 1);
    memcpy(req->content, buff + MIN_REQUEST_LENGTH, req->length);

    return req;
}

void free_request(request *req) {
    free(req->type);
    free(req->content);
    free(req);
}

char *read_number(char *buff, int remaining, int *n) {
    //  S'il ne reste pas assez de caractères à lire dans le buffer
    if (remaining < 4)
        return NULL;

    char nb[5];
    for (int i = 0; i < 4; i++) //  Lecture du nombre
        nb[i] = buff[i];
    nb[4] = '\0';

    if (sscanf(nb, "%d", n) != 1)
        return NULL;

    return buff + 4;
}

char *read_lnumber(char *buff, int remaining, long *n) {
    //  S'il ne reste pas assez de caractères à lire dans le buffer
    if (remaining < 8)
        return NULL;

    char nb[9];
    for (int i = 0; i < 8; i++) //  Lecture du nombre
        nb[i] = buff[i];
    nb[8] = '\0';

    if (sscanf(nb, "%ld", n) != 1)
        return NULL;

    return buff + 8;
}

char *read_string(char *buff, char **str, int remaining) {
    int l;
    char *ptr;
    if (!(ptr = read_number(buff, remaining, &l)))
      return NULL;

    if (remaining - (ptr - buff) < l) //   S'il ne reste pas assez de caractères dans le buffer
        return NULL;

    *str = calloc(l + 1, 1);
    for (int i = 0; i < l; i++) //  Lecture de la chaîne
        (*str)[i] = buff[4 + i];

    (*str)[l] = '\0'; //    On ajoute le caractère de fin de chaîne

    return ptr + l;
}
