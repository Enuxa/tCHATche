#include "h/client.h"
#include "h/common.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#define QUIT_COMMAND 1
#define QUIT_SEQUENCE "/quit"

extern struct timespec sleep_time;

int process_command(char *buff) {
    if (!strcmp(buff, QUIT_SEQUENCE))
        return QUIT_COMMAND;

    return 0;
}

int connect(server *srvr, char *path) {
    //  Si le serveur n'existe pas
    if (access(path, F_OK)) {
        perror("Impossible de se connecter au serveur ");
        return 1;
    }

    srvr->pipepath = malloc(PATH_LENGTH);
    strcpy(srvr->pipepath, path);

    //  Se connecte au serveur
    srvr->pipe = open(path, O_WRONLY | O_NONBLOCK);
    if (srvr->pipe < 0) {
        perror("Imposible d'ouvrir le tube serveur ");
        free(srvr->pipepath);
        return 1;
    }

    return 0;
}

int join(client *clnt, char *username, server *srvr) {
    //  Ouverture du tube
    clnt->pipepath = malloc(BUFFER_LENGTH);
    int i = 1;
    do {
        sprintf(clnt->pipepath, "%s/pipe%d", ROOT_PATH, i);
        i++;
    } while (!access(clnt->pipepath, F_OK));    //  Tant qu'on n'a pas trouvé de nom disponible

    printf("Ouverture d'un tube à l'adresse %s\n", clnt->pipepath);
    fflush(stdout);

    //  Création du tube client
    int rv;
    if ((rv = mkfifo(clnt->pipepath, S_IRWXU | S_IWGRP))) {
        printf("Impossible d'ouvrir le tube à l'adresse %s", clnt->pipepath);
        fflush(stdout);
        perror("");
        free(clnt->pipepath);
        return 1;
    }
    clnt->pipe = open(clnt->pipepath, O_RDONLY | O_NONBLOCK);

    if (clnt->pipe < 0) {
        printf("Erreur lors de l'ouverture du tube à l'adresse %s", clnt->pipepath);
        fflush(stdout);
        perror("");
        free(clnt->pipepath);
        return 1;
    }

    //  Construction du message
    char *buff = malloc(BUFFER_LENGTH);
    char *ptr = make_header(buff, 4 + 4 + 4 + strlen(username) + 4 + strlen(clnt->pipepath), CODE_JOIN);
    ptr = add_string(ptr, username);
    ptr = add_string(ptr, clnt->pipepath);

    //  Envoie le message
    if (write(srvr->pipe, buff, BUFFER_LENGTH) < 0) {
        perror("Erreur durant la demande de connexion");

        free(buff);
        close(clnt->pipe);
        remove(clnt->pipepath);
        free(clnt->pipepath);

        return 1;
    };

    printf("Demande de connexion envoyée au serveur en tant que %s.\nVeuillez patienter ", username);

    //  Attend la réponse du serveur
    int read_ret;
    do {
        nanosleep(&sleep_time, NULL);
        putchar('.');
        fflush(stdout);
    } while ((read_ret = read(clnt->pipe, buff, BUFFER_LENGTH)) <= 0);
    if (read_ret < MIN_REQUEST_LENGTH + 4) {
        printf("Réponse invalide du serveur\n");
    }
    buff[read_ret - 1] = '\0';

    printf("\n");
    //  Traite la réponse du serveur
    int ret_val;
    request *req = read_request(buff);
    if (!strcmp(req->type, CODE_SUCCESS)) {
        if (read_number(req->content, req->length, &clnt->id)) {
            printf("Requête invalide\n");
            ret_val = 1;
        }
        printf("Connexion réussie !\n");
        ret_val = 0;
    } else {
        printf("Echec de connexion !\n");
        ret_val = 1;
    }

    free_request(req);

    free(buff);

    return ret_val;
}

int color_code(char *username) {
    int n = 0;
    int i = 0;
    while (username[i] != '\0') {
        n += username[i];
        i++;
    }

    return 30 + n % 8;
}

int process_message(request *req) {
    char *username = read_string(req->content, req->length);
    if (!username) {
        printf("Message invalide\n");
        return 1;
    }
    char *msg = read_string(req->content + 4 + strlen(username), req->length - 4 - strlen(username));
    if (!msg) {
        printf("Message invalide\n");
        return 1;
    }

    printf("\033[%d;1m%s\033[0m: %s\n", color_code(username), username, msg);

    return 0;
}

int client_loop(client *clnt, server *srvr) {
    //  Rendre la lecture sur stdin non bloquante
    fcntl(0, F_SETFL, O_NONBLOCK | fcntl(0, F_GETFL, 0));

    char *buff_stdin = malloc(BUFFER_LENGTH);
    char *buff_pipe = malloc(BUFFER_LENGTH);
    char *buff_request = malloc(BUFFER_LENGTH);

    int alive = 1;
    while (alive) {
        int read_ret = read(0, buff_stdin, BUFFER_LENGTH);

        //  "Troncature" du message
        buff_stdin[read_ret - 1] = '\0';
        int cmd = process_command(buff_stdin);

        //  Si aucun message n'a été lu
        if (read_ret < 0){}
        //  Si un message a été saisi
        else if (buff_stdin[0] != '/') {
            send_message(clnt, srvr, buff_stdin, NULL);
        //  Si une commande a été saisie
        } else if (cmd == QUIT_COMMAND) {
            alive = 0;
            char *ptr = make_header(buff_request, 4 + 4 + 4, CODE_DISCONNECT);
            add_number(ptr, clnt->id);
            fflush(stdout);
            if (write(srvr->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH)
                perror("Erreur lors de la demande de deconnexion");
        }

        //  Lecture depuis le tube client
        read_ret = read(clnt->pipe, buff_pipe, BUFFER_LENGTH);
        if (read_ret < MIN_REQUEST_LENGTH && errno != EAGAIN) {
            perror("Erreur de lecture depuis le tube");
            printf("%d\n", srvr->pipe);
            fflush(stdout);
        } else if (read_ret >= MIN_REQUEST_LENGTH) {
            request *req = read_request(buff_pipe);
            if (!strcmp(req->type, CODE_SHUTDOWN)) {
                printf("Le serveur a été fermé\n");
                alive = 0;
            } else if (!strcmp(req->type, CODE_MESSAGE) || !strcmp(req->type, CODE_PRIVATE)) {
                process_message(req);
            }
            free_request(req);
        }
    }

    //  Nettoyage
    free(buff_request);
    free(buff_pipe);
    free(buff_stdin);

    return 0;
}

int run_client(client *clnt, const char *sp, const char *un) {
    server srvr;

    char server_path[BUFFER_LENGTH];
    //  Lecture de l'adresse du serveur
    if(!sp) {
        printf("Chemin du serveur : ");
        if (!fgets(server_path, BUFFER_LENGTH, stdin) || !strcmp(server_path, "\n")) {
            printf("Chemin invalide : %s\n", server_path);
            return 1;
        }
        server_path[strlen(server_path) - 1] = '\0';
    } else {
        strcpy(server_path, sp);
    }
    printf("%s\n", server_path);

    //  Tentative de connexion
    if (connect(&srvr, server_path))
        return 1;

    //  Lecture d'un nom d'utilisateur si aucun n'a été passé en argument
    char username[USERNAME_LENGTH];
    if(!un) {
        printf("Votre nom d'utilisateur : ");
        if (!fgets(username, USERNAME_LENGTH, stdin) || !strcmp(server_path, "\n")) {
            printf("Nom d'utilisateur invalide : %s\n", server_path);
            return 1;
        }
        username[strlen(username) - 1] = '\0';
    } else {
        strcpy(username, un);
    }

    //  Tentative de connexion
    if (join(clnt, username, &srvr))
        return 1;

    //  On fait vivre le serveur
    if (client_loop(clnt, &srvr))
        return 1;

    //  Nettoyage
    close(clnt->pipe);
    remove(clnt->pipepath);

    return 0;
}

int send_message(client *clnt, server *srvr, char *msg, char *dst) {
    char *buff = malloc(BUFFER_LENGTH);
    char *ptr = make_header(buff, MIN_REQUEST_LENGTH + 4 + (dst ? strlen(dst) : 0) + strlen(msg), dst ? CODE_PRIVATE : CODE_MESSAGE);
    ptr = add_number(ptr, clnt->id);
    if (dst)
        ptr = add_string(ptr, dst);
    add_string(ptr, msg);

    return write(srvr->pipe, buff, BUFFER_LENGTH) != BUFFER_LENGTH;
}
