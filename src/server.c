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

extern const struct timespec sleep_time;

#define HOME_MESSAGE "\nListe de commandes :\n- stop\tarrête le serveur\n"

//  Affiche l'invite de commande
#define PROMPT() printf("> "); fflush(stdout);

int process_request(server *srvr, request *req);

int start_server(server *srvr) {
    printf("Démarrage du serveur\n");

    //  Initialiser la variable srvr
    srvr->clients = NULL;
    srvr->pipe = -1;
    srvr->id_counter = 0;
    srvr->pipe_path = (char*) malloc(PATH_LENGTH);
    strcpy(srvr->pipe_path, SERVER_PIPE_PATH);

    //  Créer (si nécessaire) le répertoire racine
    if (access(ROOT_PATH, F_OK) && mkdir(ROOT_PATH, S_IRWXU | S_IRWXG)) {
        perror("\033[31mErreur lors de l'ouverture du dossier racine\033[0m");

        return 1;
    }

    //  Eventuelle suppression du tube serveur déjà existant
    if (!access(SERVER_PIPE_PATH, F_OK) && remove(SERVER_PIPE_PATH)) {
        printf("\033[0mLe tube serveur existe mais ne peut pas être supprimé\033[0m\n");

        free(srvr->pipe_path);

        return 1;
    }

    //  Création du tube serveur
    if (mkfifo(SERVER_PIPE_PATH, S_IRWXU | S_IWGRP | S_IWOTH)) {
        perror("\033[31mErreur lors de l'ouverture du tube serveur\033[0m");

        free(srvr->pipe_path);

        return 1;
    }

    printf("Création du tube serveur à %s\n", SERVER_PIPE_PATH);

    //  Ouverture du tube serveur
    srvr->pipe = open(SERVER_PIPE_PATH, O_RDONLY | O_NONBLOCK);
    if (srvr->pipe < 0) {
        perror("\033[31mEchec d'ouverture du tube serveur\033[0m");
        return 1;
    }

    printf("Serveur démarré avec succès !\n");

    return 0;
}

int close_server(server *srvr) {
    //  Déconnecter les clients
    client_list *list = srvr->clients, *link;
    char *buff = malloc(BUFFER_LENGTH);
    make_header(buff, MIN_REQUEST_LENGTH + 4, CODE_SHUTDOWN);
    add_number(buff + MIN_REQUEST_LENGTH, 0);
    while (list) {
        link = list;
        list = list->next;

        if (write(link->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
            printf("\033[31mImpossible d'informer %s que le serveur ferme\033[0m\n", link->clnt.name);
            perror("");
        }

        close(link->clnt.pipe);
        free(link->clnt.pipe_path);
        free(link->clnt.name);

        free(link);
    }
    //  Fermer et supprimer le tube
    if (srvr->pipe >= 0) {
        if (close(srvr->pipe) || remove(srvr->pipe_path)) {
            perror("\033[31mErreur lors de la fermeture/suppression du tube serveur\033[0m");
            return 1;
        } else printf("Tube serveur fermé et supprimé avec succès !\n");
    }
    //  Libérer la mémoire restante
    free(srvr->pipe_path);
    free(buff);

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
        if (read_ret >= MIN_REQUEST_LENGTH) {//   Si on a lu quelque chose
            request *req = read_request(buff_pipe);
            monitor_request(req);
            process_request(srvr, req);
            free_request(req);
        } else if (read_ret < 0 && errno != EAGAIN) {// Si on a rencontré un erreur différente de l'absence de données sur le tube
            printf("%d\n", read_ret);
            fflush(stdout);
            perror("\033[31mErreur de lecture depuis le tube\033[0m");
        }

        //  Traitement des commandes
        if ((read_ret = read(0, buff_stdin, BUFFER_LENGTH)) > 0) {
            buff_stdin[read_ret - 1] = '\0';
            int cmd = process_command(srvr, buff_stdin);
            if (cmd == STOP_SERVER) {
                alive = 0;
            }
        }

        nanosleep(&sleep_time, NULL);
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
        printf("\033[31mImpossible d'ouvrir le tube à l'adresse %s\033[0m", pipepath);
        fflush(stdout);
        perror("");
        free(link->clnt.name);
        free(link->clnt.pipe_path);
        free(link);
        return 1;
    }

    //  S'il n'y a pas encore de clients connectés
    if (!srvr->clients)
        srvr->clients = link;
    else {
        client_list *list = srvr->clients;
        client_list *prev;
        while (list) {
            //  Si le nom d'utilisateur n'est pas disponible
            if (!strcmp(list->clnt.name, username)) {
                make_header(buff, 8, CODE_FAIL);
                if (write(link->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
                    perror("\033[31mErreur lors de l'envoi d'echec de connexion\033[0m");
                    return 1;
                }
                free(link->clnt.name);
                free(link->clnt.pipe_path);
                free(link);
                close(link->clnt.pipe);

                printf("\033[31mTentative de connexion avec le nom %s refusée\033[0m\n", username);

                return 1;
            }

            prev = list;
            list = list->next;
        }
        prev->next = link;
    }

    link->clnt.id = srvr->id_counter;
    srvr->id_counter++;

    char *ptr = make_header(buff, 12, CODE_SUCCESS);
    add_number(ptr, link->clnt.id);
    if (write(link->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
        perror("\033[31mErreur lors de l'envoi de la confirmation de connexion\033[0m");
        return 1;
    }

    printf("\033[1m%s[id=%d]\033[0m : connnecté sur %s\n", username, link->clnt.id, pipepath);

    return 0;
}

int process_disconnect(server *srvr, int id) {
    client_list *list = srvr->clients;
    client_list *prev = NULL;
    while (list) {
        if (list->clnt.id == id) {
            if (!prev)
                srvr->clients = list->next;
            else
                prev->next = list->next;

            printf("%s déconnecté avec succès\n", list->clnt.name);

            free(list->clnt.name);
            free(list->clnt.pipe_path);
            free(list);

            return 0;
        }

        prev = list;
        list = list->next;
    }

    printf("\033[31mImpossible de déconnecter le client d'identifiant %d\033[0m\n", id);
    return 1;
}

int process_request(server *srvr, request *req) {
    if (!strcmp(req->type, CODE_JOIN)) {
        char *username, *pipepath;
        if (!(username = read_string(req->content, req->length)) ||
            !(pipepath = read_string(req->content + strlen(username) + 4, req->length - strlen(username) - 4))) {
            printf("Requête invalide\n");
            free_request(req);
            return 1;
        }

        return process_join(srvr, username, pipepath);
    } else if (!strcmp(req->type, CODE_DISCONNECT)) {
        int id;
        if (read_number(req->content, req->length, &id)) {
            printf("Requête invalide\n");
            return 1;
        }

        return process_disconnect(srvr, id);
    }

    return 1;
}
