#define  _POSIX_C_SOURCE 199309L

#include "client.h"
#include "client_file.h"
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
#include <libgen.h>

#define QUIT_COMMAND 1
#define PM_COMMAND 2
#define LIST_COMMAND 3
#define SHUT_COMMAND 4
#define SEND_COMMAND 5
#define INTERNAL_DEBUG_COMMAND 6
#define HELP_COMMAND 7
#define DEBUG_COMMAND 8
#define QUIT_SEQUENCE "/quit"
#define PM_SEQUENCE "/pm"
#define LIST_SEQUENCE "/list"
#define SHUT_SEQUENCE "/shut"
#define SEND_SEQUENCE "/send"
#define INTERNAL_DEBUG_SEQUENCE "/indbg"
#define HELP_SEQUENCE "/help"
#define DEBUG_SEQUENCE "/dbg"

extern struct timespec sleep_time;

int internal_debug = 0;

int process_command(char *buff) {
    if (!strcmp(buff, QUIT_SEQUENCE))
        return QUIT_COMMAND;
    else if(!strcmp(buff, LIST_SEQUENCE))
        return LIST_COMMAND;
    else if (strstr(buff, PM_SEQUENCE) == buff) {   //  Si la chaîne commence par la séquence de message privé
        if (strlen(buff) < strlen(PM_SEQUENCE) + 2) {   //  Il faut que la sequence soit suivie d'un espace et du nom du destinataire (au moins deux caractères au total)
            printf("Commande incomplète\n");
            return -1;
        }
        if (buff[strlen(PM_SEQUENCE)] != ' ')
            return 0;
        return PM_COMMAND;
    } else if (!strcmp(buff, SHUT_SEQUENCE)) {
      return SHUT_COMMAND;
    } else if (strstr(buff, SEND_SEQUENCE) == buff) {
      if (strlen(buff) < strlen(SEND_SEQUENCE) + 2) {   //  Il faut que la sequence soit suivie d'un espace et du nom du destinataire (au moins deux caractères au total)
          printf("Commande incomplète\n");
          return -1;
      }
      if (buff[strlen(SEND_SEQUENCE)] != ' ')
          return 0;
        return SEND_COMMAND;
    } else if (!strcmp(buff, INTERNAL_DEBUG_SEQUENCE)) {
        internal_debug = (internal_debug + 1) % 2;
        printf("Débogage interne %sactivé\n", internal_debug ? "" : "dés");
        return INTERNAL_DEBUG_COMMAND;
    } else if (!strcmp(buff, HELP_SEQUENCE)) {
      printf("%s\tDéconnecte du serveur\n", QUIT_SEQUENCE);
      printf("%s dst msg\t\tEnvoie msg uniquement à dst\n", PM_SEQUENCE);
      printf("%s\t\tAffiche la liste des utilisateurs connectés au serveur\n", LIST_SEQUENCE);
      printf("%s\t\tFerme le serveur\n", SHUT_SEQUENCE);
      printf("%s dst file\t\tEnvoie le fichier file à dst\n", SEND_SEQUENCE);
      printf("%s\t\tActive le débogage du client\n", INTERNAL_DEBUG_SEQUENCE);
      printf("%s\t\tDemande au serveur d'afficher des informations de débogage\n", DEBUG_SEQUENCE);
      printf("%s\t\tAffiche la liste des comandes\n", HELP_SEQUENCE);

      return HELP_COMMAND;
    } else if (!strcmp(buff, DEBUG_SEQUENCE)) {
      return DEBUG_COMMAND;
    }

    return 0;
}

int connect(server *srvr, char *path) {
    //  Si le serveur n'existe pas
    if (access(path, F_OK)) {
        perror("Impossible de se connecter au serveur ");
        return 1;
    }

    srvr->pipepath = calloc(PATH_LENGTH, 1);
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
    //  Créer (si nécessaire) le répertoire racine
    if (access(ROOT_PATH, F_OK) && mkdir(ROOT_PATH, S_IRWXU | S_IRWXG)) {
        perror("\033[31mErreur lors de l'ouverture du dossier racine\033[0m");

        return 1;
    }

    //  Ouverture du tube
    clnt->pipepath = calloc(BUFFER_LENGTH, 1);
    clnt->username = calloc(USERNAME_LENGTH, 1);
    strcpy(clnt->username, username);

    int i = 1;
    do {
        sprintf(clnt->pipepath, "%s/pipe%d", ROOT_PATH, i);
        i++;
    } while (!access(clnt->pipepath, F_OK));    //  Tant qu'on n'a pas trouvé de nom disponible

    printf("Ouverture d'un tube à l'adresse %s\n", clnt->pipepath);
    fflush(stdout);

    //  Création du tube client
    if (mkfifo(clnt->pipepath, S_IRWXU | S_IWGRP)) {
        printf("Impossible d'ouvrir le tube à l'adresse %s", clnt->pipepath);
        fflush(stdout);
        perror("");
        free(clnt->pipepath);
        free(clnt->username);
        return 1;
    }
    clnt->pipe = open(clnt->pipepath, O_RDONLY | O_NONBLOCK);

    if (clnt->pipe < 0) {
        printf("Erreur lors de l'ouverture du tube à l'adresse %s", clnt->pipepath);
        fflush(stdout);
        perror("");
        free(clnt->pipepath);
        free(clnt->username);
        return 1;
    }

    //  Construction du message
    int length = MIN_REQUEST_LENGTH + 4 + strlen(username) + 4 + strlen(clnt->pipepath);

    if (length > BUFFER_LENGTH) {
        printf("Erreur : l'adresse du serveur ou votre nom d'utilisateur est trop long %s\n", clnt->pipepath);
        fflush(stdout);
        close(clnt->pipe);
        free(clnt->pipepath);
        free(clnt->username);
        return 1;
    }

    //  Construction du message de demande de connexion
    char *buff = calloc(BUFFER_LENGTH, 1);
    char *ptr = make_header(buff, MIN_REQUEST_LENGTH + 4 + strlen(username) + 4 + strlen(clnt->pipepath), CODE_JOIN);
    ptr = add_string(ptr, username);
    ptr = add_string(ptr, clnt->pipepath);

    //  Envoie le message
    if (write(srvr->pipe, buff, BUFFER_LENGTH) < 0) {
        perror("Erreur durant la demande de connexion");

        free(buff);
        close(clnt->pipe);
        remove(clnt->pipepath);
        free(clnt->pipepath);
        free(clnt->username);

        return 1;
    }

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

        free(buff);
        close(clnt->pipe);
        remove(clnt->pipepath);
        free(clnt->pipepath);
        free(clnt->username);

        return 1;
    }
    buff[read_ret - 1] = '\0';

    printf("\n");
    //  Traite la réponse du serveur
    int ret_val;
    request *req = read_request(buff);
    if (!strcmp(req->type, CODE_SUCCESS)) {
        if (!read_number(req->content, req->length, &clnt->id)) {
            printf("Requête invalide\n");
            ret_val = 1;
        }
        printf("Connexion réussie !\n");
        ret_val = 0;
    } else {
        printf("Echec de connexion !\n");

        close(clnt->pipe);
        remove(clnt->pipepath);
        free(clnt->pipepath);
        free(clnt->username);

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
    char *username, *msg;
    char *ptr = read_string(req->content, &username, req->length);
    if (!ptr) {
        printf("Message invalide\n");
        return 1;
    }
    read_string(ptr, &msg, req->length - (ptr - req->content));
    if (!msg) {
        free(username);
        printf("Message invalide\n");
        return 1;
    }

    printf("\033[%d;1m%s%s\033[0m: %s\n", color_code(username), username, strcmp(req->type, CODE_PRIVATE) ? "" : "[mp]", msg);

    free(msg);
    free(username);

    return 0;
}

int client_loop(client *clnt, server *srvr) {
    //  Rendre la lecture sur stdin non bloquante
    fcntl(0, F_SETFL, O_NONBLOCK | fcntl(0, F_GETFL, 0));

    char *buff_stdin = calloc(BUFFER_LENGTH, 1);
    char *buff_pipe = calloc(BUFFER_LENGTH + 1, 1);
    char *buff_request = calloc(BUFFER_LENGTH, 1);

    int alive = 1;
    while (alive) {
        int read_ret = read(0, buff_stdin, BUFFER_LENGTH);
        //  Si un message a été saisi

        if (read_ret > 0){
            buff_stdin[read_ret - 1] = '\0';
            int cmd = process_command(buff_stdin);
            if (buff_stdin[0] != '/') {
                send_message(clnt, srvr, buff_stdin, NULL);
            //  Si une commande a été saisie
            } else if (cmd == QUIT_COMMAND) {
                alive = 0;
                char *ptr = make_header(buff_request, MIN_REQUEST_LENGTH + 4, CODE_DISCONNECT);
                add_number(ptr, clnt->id);
                if (write(srvr->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH)
                    perror("Erreur lors de la demande de deconnexion");
            } else if (cmd == PM_COMMAND) {
                send_private_message(clnt, srvr, buff_stdin + strlen(PM_SEQUENCE));
            } else if(cmd == LIST_COMMAND) {
                char *ptr = make_header(buff_request, MIN_REQUEST_LENGTH + 4, CODE_LIST);
                add_number(ptr, clnt->id);
                if (write(srvr->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH)
                    perror("Erreur lors de la demande de liste");
            } else if (cmd == SHUT_COMMAND) {
                char *ptr = make_header(buff_request, MIN_REQUEST_LENGTH + 4, CODE_SHUTDOWN);
                add_number(ptr, clnt->id);
                if (write(srvr->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH)
                    perror("Erreur lors de la demande de fermeture du serveur");
            } else if (cmd == SEND_COMMAND) {
                request_send_file(buff_stdin + strlen(SEND_SEQUENCE) + 1, clnt, srvr);
            } else if (cmd == DEBUG_COMMAND) {
                make_header(buff_request, MIN_REQUEST_LENGTH, CODE_DEBUG);
                if (write(srvr->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH)
                    perror("Erreur lors de la demande de fermeture du serveur");
            } else if (!cmd)
                printf("Erreur : commande inconnue \"%s\"\n", buff_stdin);
        }

        //  Lecture depuis le tube client
        read_ret = read(clnt->pipe, buff_pipe, BUFFER_LENGTH);
        if (read_ret >= 0)
          buff_pipe[read_ret] = '\0'; //  Pour pouvoir afficher le contenu du tube
        if (read_ret < MIN_REQUEST_LENGTH && errno != EAGAIN) {
            perror("Erreur de lecture depuis le tube");
            printf("%d", srvr->pipe);
            fflush(stdout);
        } else if (read_ret >= MIN_REQUEST_LENGTH) {
            request *req = read_request(buff_pipe);
            if (internal_debug)
              printf("\033[1mLu sur le tube : \033[0m\033[33m%s\033[0m\n", buff_pipe);
            if (!req) {
              printf("Requête invalide\n");
              continue;
            } else if (internal_debug)
              monitor_request(req);
            if (!strcmp(req->type, CODE_SHUTDOWN)) {
                printf("Le serveur a été fermé\n");
                alive = 0;
            } else if (!strcmp(req->type, CODE_MESSAGE) || !strcmp(req->type, CODE_PRIVATE)) {
                process_message(req);
            } else if(!strcmp(req->type, CODE_LIST)) {
                int n;
                char *ptr, *username;
                if((ptr = read_number(req->content, req->length, &n)) && read_string(ptr, &username, req->length - (ptr - req->content))) {
                    printf("Utilisateur : %s\n", username);
                    free(username);
                } else printf("Erreur de réception de la liste\n");
            } else if (!strcmp(req->type, CODE_FILE)) {
                receive_file(clnt, req);
            }
            free_request(req);
        }

        //  Partage de fichiers
        send_files(srvr, clnt);
    }

    //  Nettoyage
    free(buff_request);
    free(buff_pipe);
    free(buff_stdin);

    return 0;
}

int run_client(client *clnt, const char *sp, const char *un) {
    server srvr;
    clnt->ft_list = NULL;

    char *server_path = calloc(BUFFER_LENGTH, 1);
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
    if (connect(&srvr, server_path)) {
        free(server_path);
        return 1;
    }

    char *username = calloc(USERNAME_LENGTH + 2, 1);   //  On alloue plus de mémoire que nécessaire pour pouvoir détecter un pseudo trop long
    //  Lecture d'un nom d'utilisateur si aucun n'a été passé en argument
    if(!un) {
        printf("Votre nom d'utilisateur : ");
        if (!fgets(username, USERNAME_LENGTH + 2, stdin) || !strcmp(server_path, "\n")) {
            printf("Nom d'utilisateur invalide : %s\n", server_path);
            return 1;
        }
        username[strlen(username) - 1] = '\0';
    } else {
        strncpy(username, un, USERNAME_LENGTH + 2);
        username[USERNAME_LENGTH + 1] = '\0';
    }

    //  Tentative de connexion
    if (join(clnt, username, &srvr)) {
        free(server_path);
        free(username);
        free(srvr.pipepath);
        return 1;
    }

    int ret = 0;

    //  On fait vivre le serveur
    ret = client_loop(clnt, &srvr);

    //  Nettoyage
    terminate_file_transferts(clnt);
    close(clnt->pipe);
    remove(clnt->pipepath);
    free(clnt->pipepath);
    free(username);
    free(clnt->username);
    free(server_path);
    free(srvr.pipepath);

    return ret;
}

int send_message(client *clnt, server *srvr, char *msg, char *dst) {
    char *buff = calloc(BUFFER_LENGTH, 1);
    int length = MIN_REQUEST_LENGTH +
                 (4) +
                ( dst ? (4 + strlen(dst)) : (0)) +
                 (4 + strlen(msg));
    char *ptr = make_header(buff, length, dst ? CODE_PRIVATE : CODE_MESSAGE);
    ptr = add_number(ptr, clnt->id);
    if (dst)
        ptr = add_string(ptr, dst);
    add_string(ptr, msg);

    int ret = write(srvr->pipe, buff, BUFFER_LENGTH) != BUFFER_LENGTH;

    free(buff);

    return ret;
}

int send_private_message(client *clnt, server *srvr, char *buff) {
    char username[USERNAME_LENGTH];
    if (sscanf(buff, "%s", username) != 1) {
        printf("Erreur : impossible de lire le nom d'utilisateur\n");
        return 1;
    }
    if (strlen(PM_SEQUENCE) + 1 + strlen(username) + 2 > BUFFER_LENGTH) {
        printf("Erreur : le nom d'utilisateur est trop long\n");
        return 1;
    }

    return send_message(clnt, srvr, buff + strlen(username) + 1, username);
}
