#ifndef FILE_SERVER_H

#define  FILE_SERVER_H

#include "server.h"

//  Traite un échange de fichier
int process_file_transfert(request *req, server *srvr);

//  Supprime un transfert de fichier d'identifiant id de la liste de transferts en cours
void remove_file_transfert(int id, server *srvr);

//  Libère un transfert de fichier
void free_file_transfert(file_transfert *ft);

//  Met fin et libère les transferts de fichiers inachevés
void terminate_file_transferts(server *srvr);

#endif
