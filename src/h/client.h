#ifndef CLIENT_H

#define CLIENT_H

#include "common.h"

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
    int pipe;       //  Le tube serveur
} server;

/*  Envoie un message msg via un serveur srvr.
 *  Si dstId = -1, le message est public, si dstId vaut
 * l'identifiant d'un client, ce dernier est le destinataire
*/
int send_message(client *clnt, server *srvr, char *msg, char *dst);

//  Envoie un message privé buff préfixé du nom d'utilisateur du destinataire via un serveur srvr.
int send_private_message(client *clnt, server *srvr, char *buff);

//  Déconnecte du serveur
int disconnect(client *clnt, server *srvr);

/*  Tente de connecter ce client au serveur
 *  En cas d'echec, le champs pipepath de clnt a été libéré
 *  et le tube client a été fermé et supprimé.
*/
int join(client *clnt, char *username, server *srvr);

/*  Initialise une variable serveur
 *  En cas d'echec, le champ pipepath de srvr a été libéré
*/
int connect(server *srvr, char *path);

/*  Fais vivre le client
 *  A la fin de son execution, le tube client est fermé,
 *  les champs pipepath et username de clnt sont libérés
 *  et le tube client est supprimé
*/
int run_client(client *clnt, const char *sp, const char *un);

/*  Traite une commande client
 *  Renvoie 0 si la commande n'a pas été reconnue
 *  Renvoie -1 si la commande a été reconnue mais est incorrecte
 *  Renvoie l'identifiant de la commande si elle a été reconnue et est correcte
*/
int process_command(char *buff);

//  Traite une requête de type message
int process_message(request *req);

//  Fais vivre le client
int client_loop(client *clnt, server *srvr);

#endif
