#include <server.h>
#include <common.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>

int start_server(server *srvr) {
    printf("Démarrage du serveur\n");

    //  Initialiser la variable srvr
    srvr->clients = NULL;
    srvr->pipe = NULL;
    srvr->pipe_path = (char*) malloc(PATH_LENGTH);
    strcpy(srvr->pipe_path, SERVER_PIPE_PATH);

    //  Ouvrir le tube
    if (access(ROOT_PATH, F_OK) && mkdir(ROOT_PATH, S_IRWXU | S_IRWXG)) {
        perror("Erreur lors de l'ouverture du dossier racine");

        return 1;
    }

    if (mkfifoat(0, SERVER_PIPE_PATH, S_IRWXU | S_IWGRP)) {
        perror("Erreur lors de l'ouverture du tube serveur");

        return 1;
    }

    printf("Création du tube serveur à %s\n", SERVER_PIPE_PATH);
    srvr->pipe = fopen(SERVER_PIPE_PATH, "r");
    if (!srvr->pipe) {
        perror("Echec d'ouverture du tube serveur");
        return 1;
    }

    printf("Serveur démarré avec succès !\n");

    return 0;
}

int close_server(server *srvr) {
    //  Déconnecter les clients
    client_list *list = srvr->clients, *link;
    while (list) {
        link = list;
        list = list->next;

        //TODO : déconnecter le client link->clnt
        free(link);
    }
    //  Fermer et supprimer le tube
    if (srvr->pipe) {
        if (fclose(srvr->pipe) || remove(srvr->pipe_path)) {
            perror("Erreur lors de la fermeture/suppression du tube serveur");
            return 1;
        } else printf("Tube serveur fermé et supprimé avec succès !\n");
    }
    //  Libérer la mémoire restante
    free(srvr->pipe_path);

    return 0;
}

int run_server(server *srvr) {
    if (start_server(srvr)) {
        close_server(srvr);
        return 1;
    }

    char *buff = (char*) malloc(BUFFER_LENGTH);

    int alive = 1;
    while (alive) {
        if (fgets(buff, BUFFER_LENGTH, srvr->pipe))
            printf("Message lu : %s\n", buff);

        printf("> ");
        //  Traitement des messages
        if (fgets(buff, BUFFER_LENGTH, stdin)) {
            int cmd = process_command(srvr, buff);
            if (cmd == STOP_SERVER) {
                alive = 0;
            }
        }
    }

    close_server(srvr);
}

int process_command(server *srvr, char *line) {
    if (!strcmp(line, "stop\n"))
        return STOP_SERVER;

    return 0;
}
