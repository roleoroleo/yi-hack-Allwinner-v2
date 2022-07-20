#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <getopt.h>
#include <mqueue.h>
#include <pthread.h>

#define IPC_QUEUE_NAME          "/ipc_dispatch"
#define IPC_MESSAGE_MAX_SIZE    512
#define COMMAND_MAX_SIZE        512

int ipc_init();
void ipc_stop();
static int open_queue();
static int clear_queue();
static int parse_message(char *msg, char *cmd, ssize_t len);
void print_usage(char *progname);
