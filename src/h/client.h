#ifndef CLIENT_H

#define CLIENT_H

//  La représentation côté client d'un client
typedef struct _client {
    int id;         //  L'identifiant universel du client
    char *username; //  Le pseudonyme de l'utilisateur
    char *pipepath; //  Le chemin du tube client
    int pipe;       //  Le tube client
} client;

//  La représentation côté client d'un serveur
typedef struct _server {
    char *pipepath; //  Le chemin du tube serveur
    int pipe;       //  Le tbe serveur
} server;

/*  Envoie un message msg via un serveur srvr.
 *  Si dstId = 0, le message est public, si dstId vaut
 * l'identifiant d'un client, ce dernier est le destinataire
*/
int send_message(client *clnt, server *srvr, char *msg, int dstId);

//  Déconnecte du serveur
int disconnect(client *clnt, server *srvr);

//  Tente de rejoindre la discussion du serveur
int join(client *clnt, char *username, server *srvr);

//  Tente de se connecter à un serveur d'adresse path
int connect(server *srvr, char *path);

//  Fais vivre le client
int run_client(client *clnt);

#endif
