#ifndef SERVER_H
#define SERVER_H

#include "protocol.h"

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
    int id_counter;     //  Le compteur des identifiants
    int client_count;   //  Le nombre de clients
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

//  Redistribue un message
int broadcast_message(server *srvr, request *req);

//  Récupère le chaînon contenant le client d'identifiant id, ou NULL s'il n'existe pas
client_list *find_client_by_id(int id, client_list *list, client_list **previous);

#endif
