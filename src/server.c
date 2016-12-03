#include "server.h"
#include "common.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

#define HOME_MESSAGE "Liste de commandes :\n\tstop\tarrête le serveur\n"

//  Affiche l'invite de commande
#define PROMPT() printf("> "); fflush(stdout);

int start_server(server *srvr) {
    printf("Démarrage du serveur\n");

    //  Initialiser la variable srvr
    srvr->clients = NULL;
    srvr->pipe = -1;
    srvr->pipe_path = (char*) malloc(PATH_LENGTH);
    strcpy(srvr->pipe_path, SERVER_PIPE_PATH);

    //  Ouvrir le répertoire racine
    if (access(ROOT_PATH, F_OK) && mkdir(ROOT_PATH, S_IRWXU | S_IRWXG)) {
        perror("Erreur lors de l'ouverture du dossier racine");

        return 1;
    }

    //  Eventuelle suppression du tube serveur déjà existant
    if (!access(SERVER_PIPE_PATH, F_OK) && remove(SERVER_PIPE_PATH)) {
        printf("Le tube serveur existe mais ne peut pas être supprimé\n");

        free(srvr->pipe_path);

        return 1;
    }

    //  Création du tube serveur
    if (mkfifoat(0, SERVER_PIPE_PATH, S_IRWXU | S_IWGRP)) {
        perror("Erreur lors de l'ouverture du tube serveur");

        free(srvr->pipe_path);

        return 1;
    }

    printf("Création du tube serveur à %s\n", SERVER_PIPE_PATH);

    //  Ouverture du tube serveur
    srvr->pipe = open(SERVER_PIPE_PATH, O_RDONLY | O_NONBLOCK);
    if (srvr->pipe < 0) {
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
    if (srvr->pipe >= 0) {
        if (close(srvr->pipe) || remove(srvr->pipe_path)) {
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

    printf(HOME_MESSAGE);

    //  Rendre la lecture sur stdin non bloquante
    fcntl(0, F_SETFL, O_NONBLOCK | fcntl(0, F_GETFL, 0));

    char *buff_pipe = malloc(BUFFER_LENGTH);
    char *buff_stdin = malloc(BUFFER_LENGTH);

    PROMPT();

    int alive = 1, read_ret;
    while (alive) {
        //  Lecture d'un message surle tube
        read_ret = read(srvr->pipe, buff_pipe, BUFFER_LENGTH);
        if (read_ret > 0) {//   Si on a lu quelque chose
            buff_pipe[read_ret] = '\0';
            printf("Message lu : \"%s\"\n", buff_pipe);
            PROMPT();
        } else if (read_ret < 0 && errno != EAGAIN) {// Si on a rencontré un errer différente de l'absence de données sur le tube
            perror("Erreur de lecture depuis le tube");
        }

        //  Traitement des commandes
        if ((read_ret = read(0, buff_stdin, BUFFER_LENGTH)) > 0) {
            buff_stdin[read_ret - 1] = '\0';
            int cmd = process_command(srvr, buff_stdin);
            if (cmd == STOP_SERVER) {
                alive = 0;
            } else {
                PROMPT();
            }
        }

        sleep(1);
    }

    close_server(srvr);

    return 0;
}

int process_command(server *srvr, char *line) {
    //  Commande d'arrêt du serveur
    if (!strcmp(line, "stop"))
        return STOP_SERVER;

    return 0;
}
