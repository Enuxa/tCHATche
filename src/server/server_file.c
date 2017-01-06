#define  _POSIX_C_SOURCE 199309L

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
#include <stdio.h>
#include <time.h>

extern const struct timespec sleep_time;

void free_file_transfert(file_transfert *ft) {
    free(ft->filename);
    free(ft);
}

void terminate_file_transferts(server *srvr) {
    while (srvr->ft_list) {
        file_transfert *ft = srvr->ft_list->next;
        free_file_transfert(srvr->ft_list);
        srvr->ft_list = ft;
    }
}

int process_new_file_transfert(request *req, server *srvr, int *sndr_pipe, int *transId) {
    char *username, *filename;
    long length;
    int id;
    char *ptr = read_number(req->content + 4, req->length - 4, &id);
    if (!ptr) {
        printf("Requête invalide\n");
        return -1;
    }
    ptr = read_string(ptr, &username, req->length - (ptr - req->content));
    if (!ptr) {
        printf("Requête invalide\n");
        return -1;
    }
    ptr = read_lnumber(ptr, req->length - (ptr - req->content), &length);
    if (!ptr) {
        free(username);
        printf("Requête invalide\n");
        return -1;
    }
    ptr = read_string(ptr, &filename, req->length - (ptr - req->content));
    if (!ptr) {
        free(username);
        printf("Requête invalide\n");
        return -1;
    }
    char *buff = calloc(BUFFER_LENGTH, 1);
    ptr = make_header(buff, MIN_REQUEST_LENGTH + (4) + (4) + (8) + (4 + strlen(filename)), CODE_FILE);
    ptr = add_number(ptr, 0);
    ptr = add_number(ptr, srvr->transfert_id_count);
    ptr = add_lnumber(ptr, length);
    ptr = add_string(ptr, filename);

    client_list *dst, *sndr;
    dst = find_client_by_username(username, srvr->clients, &dst);
    sndr = find_client_by_id(id, srvr->clients, &sndr);

    if (!sndr) {
        printf("Erreur : l'envoyeur n'est pas un utilisateur du serveur\n");
        free(username);
        free(filename);
        free(buff);

        return -1;
    }

    *sndr_pipe = sndr->clnt.pipe;

    if (!dst) {
        printf("Erreur : client inconnu\n");
        free(username);
        free(filename);
        free(buff);

        return 1;
    }

    //  Si le client essaye de s'envoyer le fichier à lui même
    if (dst->clnt.id == sndr->clnt.id) {
      free(username);
      free(filename);
      free(buff);

      return 1;
    }

    printf("Transfert[transid=%d] -- Demande d'envoi de %s[id=%d] vers %s[id=%d] du fichier '%s' (%ld octets)\n",
            srvr->transfert_id_count,
            sndr->clnt.name, sndr->clnt.id, dst->clnt.name, dst->clnt.id,
            filename, length
    );

    if (write(dst->clnt.pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
        printf("Erreur lors de la transmission au récepteur de la notification de transfert\n");

        free(buff);
        free(username);
        free(filename);
        return 1;
    }

    printf("Envoyé : %s\n", buff);

    file_transfert *ft = calloc(1, sizeof(file_transfert));
    ft->next = srvr->ft_list;
    ft->id = srvr->transfert_id_count;
    ft->sndr = &sndr->clnt;
    ft->dst = &dst->clnt;
    ft->filename = filename;
    ft->length = length;
    ft->remaining_length = length;
    srvr->ft_list = ft;

    *transId = srvr->transfert_id_count;

    srvr->transfert_id_count++;

    free(buff);
    free(username);

    return 0;
}

int process_existing_file_transfert(char *dat, int serie, int remaining, server *srvr, int transId) {
    file_transfert *ft = srvr->ft_list;
    while (ft && ft->id != transId) {
        ft = ft->next;
    }
    //  Si on n'a pas trouvé le transfert correspondant
    if (!ft)
      return 1;

    char *buff = calloc(BUFFER_LENGTH, 1);
    char *ptr = make_header(buff, MIN_REQUEST_LENGTH + (4) + (4) + remaining, CODE_FILE);
    ptr = add_number(ptr, serie);
    ptr = add_number(ptr, transId);
    memcpy(ptr, dat, remaining);

    if (write(ft->dst->pipe, buff, BUFFER_LENGTH) < BUFFER_LENGTH) {
        printf("Erreur durant le transfert du reste du fichier à %s\n", ft->dst->name);
        free(buff);
        return 1;
    }

    free(buff);

    ft->remaining_length -= remaining;

    if (ft->remaining_length <= 0) {
        if (srvr->ft_list == ft)
            srvr->ft_list = ft->next;
        else {
          file_transfert *tmp = srvr->ft_list;
          while (tmp->next != ft) {
              tmp->next = ft->next;
          }
        }
        free_file_transfert(ft);
    }

    return 0;
}

int process_file_transfert(request *req, server *srvr) {
    int serie;
    char *ptr = read_number(req->content, req->length, &serie);
    if (!ptr) {
        printf("Requête invalide\n");
        return 1;
    }
    //  Si on continue un transfert
    if (serie) {
        int transId;
        if (!(ptr = read_number(ptr, req->length - (ptr - req->content), &transId))) {
            printf("Requête invalide\n");
            return 1;
        }

        return process_existing_file_transfert(ptr, serie, req->length - (ptr - req->content), srvr, transId);
    } else {//  Si on commence un nouveau transfert
        int sndr_pipe, transId;
        char *buff = calloc(BUFFER_LENGTH, 1);
        int ret;
        if ((ret = process_new_file_transfert(req, srvr, &sndr_pipe, &transId)) == 1) {
            make_header(buff, MIN_REQUEST_LENGTH, CODE_FAIL);
            write(sndr_pipe, buff, BUFFER_LENGTH);

            free(buff);

            return 1;
        } else if (ret == 0) {
            char *ptr = make_header(buff, MIN_REQUEST_LENGTH + 4, CODE_SUCCESS);
            add_number(ptr, transId);
            write(sndr_pipe, buff, BUFFER_LENGTH);

            free(buff);

            return 0;
        }

        free(buff);
    }

    return 0;
}
