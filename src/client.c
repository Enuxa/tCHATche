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
    char *buff = malloc(BUFFER_LENGTH);

    //  Construction du message
    make_header(buff, 0, CODE_JOIN);

    strcpy(buff + REQUEST_CONTENT_OFFSET, username);

    clnt->pipepath = malloc(BUFFER_LENGTH);

    int i = 1;
    do {
        sprintf(clnt->pipepath, "%s/pipe%d", ROOT_PATH, i);
        i++;
    } while (!access(clnt->pipepath, F_OK));    //  Tant qu'on n'a pas trouvé de nom disponible

    printf("Ouverture d'un tube à l'adresse %s\n", clnt->pipepath);
    fflush(stdout);

    //  Création du tube client
    mkfifo(clnt->pipepath, S_IRWXU | S_IWGRP);
    clnt->pipe = open(clnt->pipepath, O_RDONLY | O_NONBLOCK);
    strcpy(buff + strlen(username) + REQUEST_CONTENT_OFFSET + 1, clnt->pipepath);

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
        sleep(1);
        putchar('.');
        fflush(stdout);
    } while ((read_ret = read(clnt->pipe, buff, BUFFER_LENGTH)) <= 0);
    buff[read_ret - 1] = '\0';

    printf("\n");
    //  Traite la réponse du serveur
    int ret_val;
    if (check_type(buff, CODE_SUCCESS)) {
        printf("Connexion réussie !\n");
        ret_val = 0;
    } else {
        printf("Echec de connexion !\n");
        ret_val = 1;
    }

    free(buff);
    close(clnt->pipe);
    remove(clnt->pipepath);
    free(clnt->pipepath);

    return ret_val;
}

int run_client(client *clnt) {
    server srvr;

    //  Lecture de l'adresse du serveur
    char server_path[BUFFER_LENGTH];
    printf("Chemin du serveur : ");
    if (!fgets(server_path, BUFFER_LENGTH, stdin) || !strcmp(server_path, "\n")) {
        printf("Chemin invalide : %s\n", server_path);
        return 1;
    };

    server_path[strlen(server_path) - 1] = '\0';
    printf("%s\n", server_path);

    if (connect(&srvr, server_path))
        return 1;

    char username[USERNAME_LENGTH];
    printf("Votre nom d'utilisateur : ");
    if (!fgets(username, USERNAME_LENGTH, stdin) || !strcmp(server_path, "\n")) {
        printf("Nom d'utilisateur invalide : %s\n", server_path);
        return 1;
    }

    username[strlen(username) - 1] = '\0';
    if (join(clnt, username, &srvr))
        return 1;

    return 0;
}
