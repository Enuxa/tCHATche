#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXEC_NAME "tchatche_client"

int main(int argc, char const *argv[]) {
    if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf("Utilisation : %s [server_path [username]] où serverpath est le chemin du tube serveur et username le nom d'utilisateur\n", EXEC_NAME);
        return 1;
    }

    client clnt;
    run_client(&clnt, (argc > 1) ? argv[1] : NULL, (argc > 2) ? argv[2] : NULL);

    return 0;
}
