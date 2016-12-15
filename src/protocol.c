#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "h/protocol.h"

char* make_header(char *buff, int length, char *type) {
    sprintf(buff, "%04d%s", length, type);
    return buff + 8;
}

char* add_number(char *buff, int n) {
    sprintf(buff, "%04d", n);
    return buff + 4;
}

char *add_string(char *buff, char *str) {
    sprintf(buff, "%04d%s", (int) strlen(str), str);
    return buff + 4 + strlen(str);
}

void monitor_request(request *req) {
    printf("\n\033[1mNombre\033[0m\t\t%d\n", req->length);
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

    //  Lecture du tye de la requête
    for (int i = 0; i < 4; i++)
        req->type[i] = buff[4 + i];
    req->type[4] = '\0';

    //  Lecture du contenu de la requête
    req->content = calloc(req->length, 1);
    for (int i = 0; i < req->length; i++)
        req->content[i] = buff[MIN_REQUEST_LENGTH + i];

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

    char nb[4];
    for (int i = 0; i < 4; i++) //  Lecture du nombre
        nb[i] = buff[i];

    if (sscanf(nb, "%d", n) != 1)
        return NULL;

    return buff + 4;
}

char *read_string(char *buff, char **str, int remaining) {
    //  S'il ne reste pas assez de caractères à lire dans le buffer
    if (remaining < 4)
        return NULL;

    int l;
    char nb[4];
    for (int i = 0; i < 4; i++)//   Lecture de la longueur de la chaîne
        nb[i] = buff[i];
    if (sscanf(nb, "%d", &l) != 1)
        return NULL;

    if (remaining - 4 < l) //   S'il ne reste pas assez de caractères dans le buffer
        return NULL;

    *str = calloc(l + 1, 1);
    for (int i = 0; i < l; i++) //  Lecture de la chaîne
        (*str)[i] = buff[4 + i];

    (*str)[l] = '\0'; //    On ajoute le caractère de fi de chaîne

    return buff + 4 + l;
}
