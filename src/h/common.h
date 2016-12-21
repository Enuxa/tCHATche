#ifndef COMMON_H

#define COMMON_H

#include <time.h>
#include "protocol.h"

#define ROOT_PATH "/tmp/tCHATche"
#define SERVER_PIPE_PATH "/tmp/tCHATche/server"
#define PATH_LENGTH 256
#define BUFFER_LENGTH 512
#define DATA_BLOCK_LENGTH 256
#define USERNAME_LENGTH 64

#define MIN(a, b) ((a) > (b) ? (b) : (a))

#endif
