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
 *  [4]     :   '\0'
 *  [5-8]   :   Type
 *  [9]     :   '\0'
 *  [10-*]  :   Corps de message
*/

#define TYPE_OFFSET 5
#define REQUEST_CONTENT_OFFSET 10

//  Vérifie le type d'une requête
int check_type(char *buff, char *type);

//  Fabrique l'en-tête d'une requête
void make_header(char *buff, int number, char *type);

#endif
