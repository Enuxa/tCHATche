#include <string.h>
#include <stdio.h>

#include "h/protocol.h"

int check_type(char *buff, char *type) {
    return !strcmp(buff + 5, type);
}

void make_header(char *buff, int number, char *type) {
    sprintf(buff, "%4d", number);
    strcpy(buff + TYPE_OFFSET, type);
}
