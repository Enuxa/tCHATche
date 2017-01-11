#define  _POSIX_C_SOURCE 199309L

#include "client_file.h"
#include "client.h"
#include "common.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <libgen.h>
#include <errno.h>

extern struct timespec sleep_time;

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
        free_file_transfert(st);
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

    request *req = read_request(buff_request);
    if (!req) {
        printf("Réponse invalide\n");
        free(buff_request);
        free_file_transfert(st);
        return 1;
    }
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
      printf("Transfert Initialisé [id=%d]\n", st->id);
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
    }
    free(ft->filepath);
    free(ft->filename);
    free(ft);
}

void terminate_file_transferts(client *clnt) {
  file_transfert *ft = clnt->ft_list;
  while (ft) {
      free_file_transfert(ft);
      ft = ft->next;
  }
}

//  Renvoie 1 si le transfert est terminé et 0 s'il ne l'est pas
int send_file(server *srvr, file_transfert *ft) {
    char *buff = calloc(BUFFER_LENGTH, 1);
    int block_size = MIN(DATA_BLOCK_LENGTH, ft->remaining_length);
    char *ptr = make_header(buff, MIN_REQUEST_LENGTH + (4) + (4) + block_size, CODE_FILE);
    ptr = add_number(ptr, ft->serie);
    ptr = add_number(ptr, ft->id);
    long position = lseek(ft->file, 0, SEEK_CUR);
    read(ft->file, ptr, block_size);
    ft->serie++;
    ft->remaining_length -= block_size;

    int k;
    if ((k = write(srvr->pipe, buff, BUFFER_LENGTH)) < BUFFER_LENGTH) {
      if (errno == EAGAIN) {  //  Si le tube est plein et doit d'abord être vidé
        ft->serie--;
        ft->remaining_length += block_size;
        lseek(ft->file, position, SEEK_SET);
      } else {
        printf("Erreur lors de la transmission du paquet no %d à %s (%d/%d octets écrits) : ", ft->serie, ft->username, k, block_size);
        perror("");
      }
      fflush(stdout);
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
        if (ft->remaining_length <= 0) {
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
