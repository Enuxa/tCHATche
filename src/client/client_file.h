#ifndef CLIENT_FILE_H

#include "client.h"

//  Initialise l'envoi d'un fichier
int request_send_file(char *buff, client *clnt, server *srvr);

//  Supprime un transfert de fichier d'identifiant id de la liste de transferts en cours
void remove_file_transfert(int id, client *clnt);

//  Libère un transfert de fichier
void free_file_transfert(file_transfert *ft);

//  Met fin et libère les transferts de fichiers inachevés
void terminate_file_transferts(client *clnt);

//  Continue d'envoyer les fichiers en cours d'échange
void send_files(server *srvr, client *clnt);

//  Traite la réception d'un fichier
int receive_file(client *clnt, request *req);

#define  CLIENT_FILE_H
#endif
