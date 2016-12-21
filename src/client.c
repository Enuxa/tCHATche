#define  _POSIX_C_SOURCE 199309L

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
#include <libgen.h>

#define QUIT_COMMAND 1
#define PM_COMMAND 2
#define LIST_COMMAND 3
#define SHUT_COMMAND 4
#define SEND_COMMAND 5
#define QUIT_SEQUENCE "/quit"
#define PM_SEQUENCE "/pm"
#define LIST_SEQUENCE "/list"
#define SHUT_SEQUENCE "/shut"
#define SEND_SEQUENCE "/send"

extern struct timespec sleep_time;

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

    if (strlen(username) > USERNAME_LENGTH || length > BUFFER_LENGTH) {
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
    char *buff_pipe = calloc(BUFFER_LENGTH, 1);
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
            } else if (!cmd)
                printf("Erreur : commande inconnue \"%s\"\n", buff_stdin);
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
            } else if(!strcmp(req->type, CODE_LIST)) {
                int n;
                char *ptr, *username;
                if((ptr = read_number(req->content, req->length, &n)) && read_string(ptr, &username, req->length)) {
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
        username[strlen(username)] = '\0';
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
    printf("Envoi de %s\n", msg);
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

int request_send_file(char *buff, client *clnt, server *srvr) {
    char *username = calloc(USERNAME_LENGTH, 1);
    sscanf(buff, "%s", username);
    char *path = buff + strlen(username) + 1;
    while (path[0] == ' ') { //  On se positionne au premier caractère qui n'est pas un espace
      path++;
    }

    if (access(path, R_OK)) {
      printf("Impossible d'ouvrir le fichier à l'adresse %s\n", path);
      free(username);
      return 1;
    }

    file_transfert *st = calloc(1, sizeof(file_transfert));

    st->id = -1;
    st->serie = 1;
    st->sending = 1;
    st->username = username;
    st->file = open(path, O_RDONLY);
    st->remaining_length = st->length = lseek(st->file, 0, SEEK_END);
    st->filename = calloc(PATH_LENGTH, 1);
    st->next = NULL;
    st->filepath = calloc(PATH_LENGTH, 1);
    strcpy(st->filepath, path);
    strcpy(st->filename, basename(path));

    lseek(st->file, 0, SEEK_SET);

    char *buff_request = calloc(BUFFER_LENGTH, 1);
    char *ptr = make_header(buff_request, MIN_REQUEST_LENGTH + (4) + (4) + (4 + strlen(username)) + 8 + (4 + strlen(st->filename)), CODE_FILE);
    ptr = add_number(ptr, 0);
    ptr = add_number(ptr, clnt->id);
    ptr = add_string(ptr, username);
    ptr = add_lnumber(ptr, st->length);
    ptr = add_string(ptr, st->filename);

    if (write(srvr->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH) {
        printf("Erreur lors de l'envoi de demande de transfert\n");
        free(st);
        free(buff_request);
        return 1;
    }

    //  Réception de la réponse du serveur
    printf("Attente d'une réponse");
    while (read(clnt->pipe, buff_request, BUFFER_LENGTH) < BUFFER_LENGTH) {
          nanosleep(&sleep_time, NULL);
          putchar('.');
          fflush(stdout);
    }
    printf("\n");
    request *req = read_request(buff_request);
    if (!strcmp(req->type, CODE_FAIL)) {
      printf("Le transfert de fichier a été refusé par le serveur\n");

      free(buff_request);
      free_request(req);
      free_file_transfert(st);
      return 1;
    } else {
      if (!read_number(req->content, req->length, &st->id)) {
          printf("Réponse invalide de la part du serveur :\n");
          monitor_request(req);
          free(buff_request);
          free_request(req);
          free_file_transfert(st);
          return 1;
      }
      printf("Transfert Initialisé\n");
    }

    if (!clnt->ft_list) {
        clnt->ft_list = st;
    } else {
        file_transfert *list = clnt->ft_list;
        while (list->next) {
          list = list->next;
        }
        list->next = st;
    }

    free(buff_request);
    free_request(req);

    return 0;
}

void free_file_transfert(file_transfert *ft) {
    close(ft->file);
    if (ft->sending) {
      free(ft->username);
      free(ft->filepath);
    }
    free(ft->filename);
    free(ft);
}

void terminate_file_transferts(client *clnt) {
  while (clnt->ft_list) {
      file_transfert *ft = clnt->ft_list->next;
      free_file_transfert(clnt->ft_list);
      clnt->ft_list = ft;
  }
}

//  Renvoie 1 si le transfert est terminé et 0 s'il ne l'est pas
int send_file(server *srvr, file_transfert *ft) {
    char *buff = calloc(BUFFER_LENGTH, 1);
    int block_size = MIN(DATA_BLOCK_LENGTH, ft->remaining_length);
    char *ptr = make_header(buff, MIN_REQUEST_LENGTH + (4) + (4) + block_size, CODE_FILE);
    ptr = add_number(ptr, ft->serie);
    ptr = add_number(ptr, ft->id);
    read(ft->file, ptr, block_size);
    ft->serie++;
    ft->remaining_length -= block_size;

    if (write(srvr->pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
      printf("Erreur lors de la transmission du paquet no %d à %s\n", ft->serie, ft->username);
      free(buff);

      return -1;
    }

    free(buff);

    if (ft->remaining_length)
        return 0;
    else
        return 1;
}

void send_files(server *srvr, client *clnt) {
    file_transfert *list = clnt->ft_list, *prev;
    while (list) {
        if (list->sending) {
            if (send_file(srvr, list) == 1) {
                if (list == clnt->ft_list)
                    clnt->ft_list = NULL;
                else
                    prev->next = list->next;
                file_transfert *tmp = list;
                list = list->next;
                printf("Transfert de %s terminé\n", tmp->filename);
                free_file_transfert(tmp);
                continue;
            }
        }

        prev = list;
        list = list->next;
    }
}

int receive_file(client *clnt, request *req) {
    int serie, transId;
    char *ptr;
    if (!(ptr = read_number(req->content, req->length, &serie)) ||
        !(ptr = read_number(ptr, req->length - (ptr - req->content), &transId))) {
          printf("Requête invalide\n");
          return 1;
    }
    //  Si on doit continuer à recevoir un fichier
    if (serie) {
        printf("Reception d'un paquet de %d\n", transId);
        file_transfert *ft = clnt->ft_list, *prev = NULL;
        int block_size = req->length - 4 - 4;
        while (ft && ft->id != transId) {
            prev = ft;
            ft = ft->next;
        }
        if (!ft) {
          printf("Transfert d'identifiant %d introuvable\n", transId);
          return 1;
        }
        if (write(ft->file, ptr, block_size) < block_size) {
          printf("Impossible de copier le paquet reçu dans le fichier\n");
          return 1;
        }
        ft->remaining_length -= block_size;
        if (!ft->remaining_length) {
          file_transfert *tmp = ft;
          if (prev)
            prev->next = ft->next;
          else
            clnt->ft_list = NULL;
          printf("Transfert de '%s' terminé\n", tmp->filename);
          free_file_transfert(tmp);
        }
    //  Si on doit commencer à recevoir un fichier
    } else {
        long length;
        char *filename;
        ptr = read_lnumber(ptr, req->length - (ptr - req->content), &length);
        if (!ptr) {
            printf("Requête invalide\n");
            return 1;
        }
        if (!read_string(ptr, &filename, req->length - (ptr - req->content))) {
          printf("Requête invalide\n");
          return 1;
        }
        char *path = calloc(PATH_LENGTH, 1);
        if (access(filename, F_OK)) {
            strcpy(path, filename);
        } else {
          int k = 1;
          do {
              sprintf(path, "%s_%d", filename, k);
              k++;
          } while (!access(path, F_OK));
        }
        file_transfert *ft = calloc(1, sizeof(file_transfert));
        ft->id = transId;
        ft->file = open(path, O_WRONLY | O_CREAT, S_IRWXU);
        if (ft->file < 0) {
          printf("Impossible de créer de fichier pour recevoir '%s'\n", filename);
          free(ft);
          return 1;
        }
        ft->filename = filename;
        ft->filepath = path;
        ft->length = length;
        ft->serie = 1;
        ft->sending = 0;
        ft->username = NULL;
        ft->remaining_length = length;
        ft->next = clnt->ft_list;
        clnt->ft_list = ft;

        printf("Réception de '%s' initialisée (id=%d)\n", ft->filename, ft->id);
    }

    return 0;
}
