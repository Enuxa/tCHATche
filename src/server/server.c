#define  _POSIX_C_SOURCE 199309L

#include "server.h"
#include "server_file.h"
#include "common.h"
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

//  Affiche l'invite de commande
#define PROMPT() printf("\n> "); fflush(stdout);

int process_request(server *srvr, request *req, int *alive);

int start_server(server *srvr) {
    printf("Démarrage du serveur\n");

    //  Initialiser la variable srvr
    srvr->clients = NULL;
    srvr->pipe = -1;
    srvr->id_counter = 0;
    srvr->pipe_path = calloc(PATH_LENGTH, 1);
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

void debug_display(server *srvr) {
  client_list *list = srvr->clients;
  if (!list)
    printf("\033[1mAucun utilisateur connecté\033[0m\n");
  while (list) {
    printf("\033[1mUtilisateur %s(id=%d)\tpipe=%d@%s\033[0m\n", list->clnt.name, list->clnt.id, list->clnt.pipe, list->clnt.pipe_path);
    list = list->next;
  }

  file_transfert *link = srvr->ft_list;
  if (!link)
    printf("\033[1mAucun transfert de fichier en cours\033[0m\n");
  while (link) {
    printf("\033[1mTranfert de %s(id=%d)\tde %s à %s (%ld/%ld octets transmis)\n", link->filename, link->id, link->sndr->name, link->dst->name, link->remaining_length, link->length);
    link = link->next;
  }
}

int close_server(server *srvr) {
    //  Déconnecter les clients
    client_list *list = srvr->clients, *link;
    char *buff = calloc(BUFFER_LENGTH, 1);
    char *ptr = make_header(buff, MIN_REQUEST_LENGTH + 4, CODE_SHUTDOWN);
    add_number(ptr, 0);
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
    if (close(srvr->pipe) || remove(srvr->pipe_path)) {
        perror("\033[31mErreur lors de la fermeture/suppression du tube serveur\033[0m");
        return 1;
    } else printf("Tube serveur fermé et supprimé avec succès !\n");
    //  Libérer la mémoire restante
    free(srvr->pipe_path);
    free(buff);

    terminate_file_transferts(srvr);

    return 0;
}

int run_server(server *srvr) {
    srvr->ft_list = NULL;
    srvr->transfert_id_count = 0;

    if (start_server(srvr)) {
        close_server(srvr);
        return 1;
    }

    //  Rendre la lecture sur stdin non bloquante
    fcntl(0, F_SETFL, O_NONBLOCK | fcntl(0, F_GETFL, 0));

    char *buff_pipe = calloc(BUFFER_LENGTH + 1, 1);
    char *buff_stdin = calloc(BUFFER_LENGTH, 1);

    PROMPT();

    int alive = 1, read_ret;
    while (alive) {
        //  Lecture d'un message sur le tube
        read_ret = read(srvr->pipe, buff_pipe, BUFFER_LENGTH);
        if (read_ret >= MIN_REQUEST_LENGTH) {//   Si on a lu quelque chose
            buff_pipe[read_ret] = '\0';
            request *req = read_request(buff_pipe);
            monitor_request(req);
            process_request(srvr, req, &alive);
            free_request(req);
            PROMPT();
        } else if (read_ret < 0 && errno != EAGAIN) {// Si on a rencontré un erreur différente de l'absence de données sur le tube
            printf("%d\n", read_ret);
            fflush(stdout);
            perror("\033[31mErreur de lecture depuis le tube\033[0m");
        }

        //  Traitement des commandes
        if ((read_ret = read(0, buff_stdin, BUFFER_LENGTH)) > 0) {
            buff_stdin[read_ret - 1] = '\0';
            int cmd = process_command(srvr, buff_stdin);
            if (cmd == STOP_COMMAND) {
                alive = 0;
            } else if (cmd == DEBUG_COMMAND) {
                debug_display(srvr);
            } else if (cmd == 0) {
                printf("\033[31mCommande inconnue\033[0m\n");
            }
            PROMPT();
        }

        memset(buff_pipe, 0, BUFFER_LENGTH);

        nanosleep(&sleep_time, NULL);
    }

    close_server(srvr);

    free(buff_pipe);
    free(buff_stdin);

    return 0;
}

int process_command(server *srvr, char *line) {
    //  Commande d'arrêt du serveur
    if (!strcmp(line, "stop"))
        return STOP_COMMAND;
    //  Débogage du serveur
    else if (!strcmp(line, "debug"))
        return DEBUG_COMMAND;
    //  Liste des commandes
    else if (!strcmp(line, "help")) {
      printf("\033[1mstop\tFerme le serveur\033[0m\n");
      printf("\033[1mdebug\tAffiche des informations de débogage\033[0m\n");
      printf("\033[1mhelp\tAffiche les commandes\033[0m\n");
      return HELP_COMMAND;
    }

    return 0;
}

int process_join(server *srvr, char *username, char *pipepath) {
    client_list *link = calloc(1, sizeof(client_list));
    char *buff = calloc(BUFFER_LENGTH, 1);

    link->clnt.name = calloc(USERNAME_LENGTH, 1);
    link->clnt.pipe_path = calloc(PATH_LENGTH, 1);
    link->next = NULL;
    link->clnt.id = 0;
    strcpy(link->clnt.name, username);
    strcpy(link->clnt.pipe_path, pipepath);
    link->clnt.pipe = open(pipepath, O_WRONLY | O_NONBLOCK);

    //  Si on a échoué à ouvrir le tube client ou que le nom d'utilisateur est trop long
    if (link->clnt.pipe < 0) {
        if (link->clnt.pipe < 0) {
            printf("\033[31mImpossible d'ouvrir le tube à l'adresse %s\033[0m", pipepath);
            fflush(stdout);
            perror("");
        } else
            printf("\033[31mUn client a tenté de se connecter avec un pseudonyme de %d caractères (%d de trop)\033[0m", (int) strlen(username), USERNAME_LENGTH - (int) strlen(username));
        free(link->clnt.name);
        free(link->clnt.pipe_path);
        free(link);
        return 1;
    }

    //  On vérifie que le nom est correct
    int username_valid = (strlen(username) > 0 && strlen(username) <= USERNAME_LENGTH);
    for (int i = 0; username[i] != '\0' && username_valid; i++) {
        if (username[i] == ' ' || username[i] == '\t' || username[i] == '\n')
            username_valid = 0;
    }

    //  S'il n'y a pas encore de clients connectés
    if (username_valid && !srvr->clients) {
        srvr->clients = link;
    } else if (username_valid) {
        client_list *list = srvr->clients;
        client_list *prev;
        while (list && username_valid) {
            //  Si le nom d'utilisateur n'est pas disponible
            if (!strcmp(list->clnt.name, username))
                username_valid = 0;

            prev = list;
            list = list->next;
        }
        prev->next = link;
    }

    if (!username_valid) {
        make_header(buff, 8, CODE_FAIL);
        if (write(link->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
            perror("\033[31mErreur lors de l'envoi d'echec de connexion\033[0m");
            return 1;
        }
        free(link->clnt.name);
        free(link->clnt.pipe_path);
        close(link->clnt.pipe);
        free(link);
        free(buff);

        printf("\033[31mTentative de connexion avec le nom '%s' refusée\033[0m\n", username);

        return 1;
    }

    link->clnt.id = srvr->id_counter;
    srvr->id_counter++;

    char *ptr = make_header(buff, 12, CODE_SUCCESS);
    add_number(ptr, link->clnt.id);
    if (write(link->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
        perror("\033[31mErreur lors de l'envoi de la confirmation de connexion\033[0m");
        return 1;
    }

    printf("\033[1m%s[id=%d]\033[0m : connecté sur %s\n", username, link->clnt.id, pipepath);

    free(buff);

    return 0;
}

int process_disconnect(server *srvr, int id) {
    client_list *prev = NULL;
    client_list *clnt = find_client_by_id(id, srvr->clients, &prev);
    if(clnt) {
        if (!prev)
            srvr->clients = clnt->next;
        else
            prev->next = clnt->next;

        printf("%s déconnecté avec succès\n", clnt->clnt.name);

        free(clnt->clnt.name);
        free(clnt->clnt.pipe_path);
        free(clnt);

        return 0;
    }

    printf("\033[31mImpossible de déconnecter le client d'identifiant %d\033[0m\n", id);
    return 1;
}

int broadcast_message(server *srvr, request *req) {
    int id;
    char *msg;
    char *ptr = read_number(req->content, req->length, &id);
    if (!ptr)
        return 1;

    char *username = NULL;
    if (!strcmp(CODE_PRIVATE, req->type))
        ptr = read_string(ptr, &username, req->length - (ptr - req->content));

    if (!ptr) {
          printf("Requête invalide\n");
          return 1;
    }
    if (!read_string(ptr, &msg, req->length - (ptr - req->content))) {
        printf("Requête invalide\n");
        return 1;
    }
    if (!msg) {
        free(username);
        return 1;
    }

    client_list *list = srvr->clients;
    char *buff = NULL;
    client_list *clnt;
    clnt = find_client_by_id(id, list, &clnt);
    if(clnt) {
        buff = calloc(BUFFER_LENGTH, 1);
        ptr = make_header(buff, MIN_REQUEST_LENGTH + 4 + strlen(clnt->clnt.name) + 4 + strlen(msg), req->type);
        ptr = add_string(ptr, clnt->clnt.name);
        add_string(ptr, msg);
    } else {
        printf("\033[31mLe message reçu ne provient pas d'un utilisateur connecté\033[0m\n");
        return 1;
    }

    int ret_val = 0;
    list = srvr->clients;
    while (list) {
        //  On s'assure de ne pas retransmettre le mesage à l'émetteur
        if (list->clnt.id != id && (strcmp(req->type, CODE_PRIVATE) || !strcmp(username, list->clnt.name))) {
            if (write(list->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
                printf("\033[31mErreur lors de la transmission du message à %s\033[0m", list->clnt.name);
                perror("");
                ret_val = 1;
            }
        }
        list = list->next;
    }

    free(buff);
    free(msg);

    if (username)
        free(username);

    return ret_val;
}

int process_list(server *srvr, request *req) {
    int id;
    if (!read_number(req->content, req->length, &id)) {
        printf("Requête invalide\n");
        return 1;
    }
    client_list *clnt;
    clnt = find_client_by_id(id, srvr->clients, &clnt);
    if(!clnt) {
        printf("Client inconnu\n");
        return 1;
    }
    client_list *link = srvr->clients;
    char *buff = calloc(BUFFER_LENGTH, 1);
    while (link) {
        char *ptr = make_header(buff, MIN_REQUEST_LENGTH + 4 + 4 + strlen(link->clnt.name), CODE_LIST);
        ptr = add_number(ptr, srvr->client_count);
        add_string(ptr, link->clnt.name);

        if(write(clnt->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH)
            printf("Erreur d'envoi de l'utilisateur %s à %s\n", link->clnt.name, clnt->clnt.name);

        link = link->next;
    }

    free(buff);

    return 0;
}

int process_request(server *srvr, request *req, int *alive) {
    //  Connexion d'un utilisateur
    if (!strcmp(req->type, CODE_JOIN)) {
        char *username = NULL, *pipepath = NULL, *ptr;
        if (!(ptr = read_string(req->content, &username, req->length)) ||
            !(read_string(ptr, &pipepath, req->length - (ptr - req->content)))) {

            printf("Requête invalide\n");

            if (username)
                free(username);

            return 1;
        }

        int ret = process_join(srvr, username, pipepath);

        free(username);
        free(pipepath);

        return ret;
    //  Déconnexion d'un utilisateur
    } else if (!strcmp(req->type, CODE_DISCONNECT)) {
        int id;
        if (!read_number(req->content, req->length, &id)) {
            printf("Requête invalide\n");
            return 1;
        }

        return process_disconnect(srvr, id);
    }
    //  Réception d'un message
    else if (!strcmp(req->type, CODE_MESSAGE) || !strcmp(req->type, CODE_PRIVATE)) {
        return broadcast_message(srvr, req);
    //  Demande de liste
    } else if(!strcmp(req->type, CODE_LIST)) {
        return process_list(srvr, req);
    //  Demande de fermeture du serveur
    } else if (!strcmp(req->type, CODE_SHUTDOWN)) {
        *alive = 0;
    //  Echange de fichiers
    } else if (!strcmp(req->type, CODE_FILE)) {
        process_file_transfert(req, srvr);
    } else if (!strcmp(req->type, CODE_DEBUG)) {
        debug_display(srvr);
    }

    return 1;
}

client_list *find_client_by_id(int id, client_list *list, client_list **previous) {
    client_list *link = list;
    *previous = NULL;
    while (link) {
        if (link->clnt.id == id) {
            return link;
        }

        *previous = link;
        link = link->next;
    }

    return NULL;
}

client_list *find_client_by_username(char *username, client_list *list, client_list **previous) {
    client_list *link = list;
    *previous = NULL;
    while (link) {
        if (!strcmp(username, link->clnt.name)) {
            return link;
        }

        *previous = link;
        link = link->next;
    }

    return NULL;
}
