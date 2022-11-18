#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <mqueue.h>
#include <dlfcn.h>

#define IPC_QUEUE_NAME          "/ipc_dispatch"
#define ENV_IPC_MULTIPLEX_DEBUG "IPC_MULTIPLEX_DEBUG"
#define IPC_MESSAGE_MAX_SIZE    512
#define MESSAGE_PRIORITY        1
#define INVALID_QUEUE           -1
#define MESSAGE_TARGET_OFFSET   0
#define MESSAGE_ID_IPC_DISPATCH 1
