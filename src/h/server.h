#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>

#define STOP_SERVER 1

//  La représentation côté serveur d'un client
typedef struct _client {
    int id;             //  L'identifiant du client
    char *name;         //  Le nom du client
    char *pipe_path;    //  Le chemin du tube du client
    int pipe;           //  Le descripteur du tube client
} client;

//  Liste de clients
typedef struct _client_list {
    client clnt;
    struct _client_list *next;
} client_list;

//  La représentation côté serveur d'un serveur
typedef struct _server {
    char *pipe_path;    //  Le chemin vers le tube du serveur
    int pipe;           //  Le descripteur du tube serveur
    client_list *clients;// La liste des clients
} server;

//  Fait vivre le serveur
int run_server(server *srvr);
//  Démarre le serveur
int start_server(server *srvr);
//  Ferme le serveur
int close_server(server *srvr);
/*  Traite les commandes saisies dans le terminal
*   STOP_SERVER :   Demande de fermeture du serveur
*/
int process_command(server *srvr, char *line);

#endif
