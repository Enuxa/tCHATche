#include "h/server.h"
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

#define HOME_MESSAGE "\nListe de commandes :\n- stop\tarrête le serveur\n"

//  Affiche l'invite de commande
#define PROMPT() printf("> "); fflush(stdout);

int process_request(server *srvr, char *buff, int length);
void monitor_request(char *buff, int length);

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
    if (mkfifo(SERVER_PIPE_PATH, S_IRWXU | S_IWGRP | S_IWOTH)) {
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

    int alive = 1, read_ret;
    while (alive) {
        //  Lecture d'un message surle tube
        read_ret = read(srvr->pipe, buff_pipe, BUFFER_LENGTH);
        if (read_ret > 0) {//   Si on a lu quelque chose
            buff_pipe[read_ret] = '\0';
            printf("\n");
            monitor_request(buff_pipe, read_ret);
            process_request(srvr, buff_pipe, read_ret);
        } else if (read_ret < 0 && errno != EAGAIN) {// Si on a rencontré un errer différente de l'absence de données sur le tube
            perror("Erreur de lecture depuis le tube");
        }

        //  Traitement des commandes
        if ((read_ret = read(0, buff_stdin, BUFFER_LENGTH)) > 0) {
            buff_stdin[read_ret - 1] = '\0';
            int cmd = process_command(srvr, buff_stdin);
            if (cmd == STOP_SERVER) {
                alive = 0;
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

int process_join(server *srvr, char *username, char *pipepath) {
    client_list *link = malloc(sizeof(client_list));
    char *buff = malloc(BUFFER_LENGTH);

    link->clnt.name = malloc(BUFFER_LENGTH);
    link->clnt.pipe_path = malloc(BUFFER_LENGTH);
    link->next = NULL;
    link->clnt.id = 0;
    strcpy(link->clnt.name, username);
    strcpy(link->clnt.pipe_path, pipepath);
    link->clnt.pipe = open(pipepath, O_WRONLY | O_NONBLOCK);

    if (link->clnt.pipe < 0) {
        printf("Impossible d'ouvrir le tube à l'adresse %s", pipepath);
        fflush(stdout);
        perror("");
        free(link->clnt.name);
        free(link->clnt.pipe_path);
        free(link);
        return 1;
    }

    int id = 0;

    //  S'il n'y a pas encore de clients connectés
    if (!srvr->clients)
        srvr->clients = link;
    else {
        client_list *list = srvr->clients;
        client_list *prev;
        while (list) {
            //  Si le nom d'utilisateur n'est pas disponible
            if (!strcmp(list->clnt.name, username)) {
                make_header(buff, 0, CODE_FAIL);
                write(link->clnt.pipe, buff, BUFFER_LENGTH);
                free(link->clnt.name);
                free(link->clnt.pipe_path);
                free(link);
                close(link->clnt.pipe);

                printf("Tentative de connexion avec le nom %s refusée\n", username);

                return 1;
            }

            prev = list;
            list = list->next;
        }
        id = prev->clnt.id + 1;
        prev->next = link;
    }

    make_header(buff, 0, CODE_SUCCESS);
    sprintf(buff + REQUEST_CONTENT_OFFSET, "%d", id);
    write(link->clnt.pipe, buff, BUFFER_LENGTH);

    printf("\033[1m%s[id=%d]\033[0m : connnecté sur %s\n", username, id, pipepath);

    return 0;
}

int process_request(server *srvr, char *buff, int length) {
    if (check_type(buff, CODE_JOIN)) {
        char *username = buff + REQUEST_CONTENT_OFFSET;
        char *pipepath = buff + 1 + REQUEST_CONTENT_OFFSET + strlen(buff + REQUEST_CONTENT_OFFSET);

        if (REQUEST_CONTENT_OFFSET >= length || 1 + REQUEST_CONTENT_OFFSET + strlen(buff + REQUEST_CONTENT_OFFSET) >= length)
            return 1;

        return process_join(srvr, username, pipepath);
    }

    return 1;
}

void monitor_request(char *buff, int length) {
    printf("\033[1mNombre\033[0m\t%s\n", buff);
    printf("\033[1mType\033[0m\t%s\n", buff + TYPE_OFFSET);
}
