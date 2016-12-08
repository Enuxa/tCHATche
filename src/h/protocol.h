#ifndef PROTOCOL_H

#define PROTOCOL_H

#define CODE_SUCCESS "OKOK"     //  Succès
#define CODE_FAIL "BADD"        //  Echec

#define CODE_JOIN "HELO"        //  Demande de connexion
#define CODE_DISCONNECT "BYEE"  //  Demande de deconnexion

#define CODE_MESSAGE "BCST"     //  Transfert d'un message
#define CODE_PRIVATE "PRVT"     //  Transfert d'un message privé
#define CODE_FILE "FILE"        //  Transfert d'un fichier

#define CODE_LIST "LIST"        //  Transfert de la liste d'utilisateurs
#define CODE_SHUTDOWN "SHUT"    //  Demande de fermeture du serveur

#define CODE_DEBUG "DEBG"       //  Débogage

/*  Structure d'un message :
 *  [0-3]   :   Nombre
 *  [4-7]   :   Type
 *  [8-*]  :   Corps de message
*/

#define MIN_REQUEST_LENGTH 8

typedef struct _request {
    int length;
    char *type;
    char *content;
} request;

//  Vérifie le type d'une requête
int check_type(char *buff, char *type);

//  Fabrique l'en-tête d'une requête
char* make_header(char *buff, int length, char *type);

/*  Ajoute un nombre à une requête
 * Renvoie un pointeur vers la suite de la requête
*/
char* add_number(char *buff, int n);

/*  Ajoute une chaîne à une requête
 * Renvoie un pointeur vers la suite de la requête
*/
char* add_string(char *buff, char *str);

//  Décompose l'en-tête d'une requête
request* read_request(char *buff);

//  Lit une chaîne depuis une requête
char *read_string(char *buff, int remaining);

//  Lit un nombre depuis une requête
int read_number(char *buff, int remaining, int *n);

//  Libère les ressources utilisées par une requête
void free_request(request *req);

//  Affiche les informations relatives à une requête
void monitor_request(request *req);

#endif
