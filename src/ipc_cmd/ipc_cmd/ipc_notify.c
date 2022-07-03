#include "ipc_notify.h"

static mqd_t ipc_mq;
static char command_buffer[COMMAND_MAX_SIZE + 1];
static char message_buffer[(IPC_MESSAGE_MAX_SIZE * 2) + 1];
static int debug = 0;

static int open_queue();
static int parse_message(char *msg, char* cmd, ssize_t len);

int ipc_init()
{
    int ret;

    ret = open_queue();
    if(ret != 0)
        return -1;

    ret = clear_queue();
    if(ret != 0)
        return -2;

    return 0;
}

void ipc_stop()
{
    if(ipc_mq > 0)
        mq_close(ipc_mq);
}

static int open_queue()
{
    ipc_mq = mq_open(IPC_QUEUE_NAME, O_RDONLY);
    if(ipc_mq == -1) {
        fprintf(stderr, "Can't open mqueue %s. Error: %s\n", IPC_QUEUE_NAME, strerror(errno));
        return -1;
    }
    return 0;
}

static int clear_queue()
{
    struct mq_attr attr;
    char buffer[IPC_MESSAGE_MAX_SIZE + 1];

    if (mq_getattr(ipc_mq, &attr) == -1) {
        fprintf(stderr, "Can't get queue attributes\n");
        return -1;
    }

    while (attr.mq_curmsgs > 0) {
        if (debug) fprintf(stderr, "Clear message in queue...");
        mq_receive(ipc_mq, buffer, IPC_MESSAGE_MAX_SIZE, NULL);
        if (debug) fprintf(stderr, " done.\n");
        if (mq_getattr(ipc_mq, &attr) == -1) {
            fprintf(stderr, "Can't get queue attributes\n");
            return -1;
        }
    }

    return 0;
}

char lookup[16] = { 
    '0', '1', '2', '3', '4', '5', '6', '7', 
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' 
};

static int parse_message(char *buffer, char *cmd, ssize_t len)
{
    // Convert message to hex string
    for (int i = 0; i < len; i++) {
        message_buffer[i * 2]     = lookup[buffer[i] >>  4];
        message_buffer[i * 2 + 1] = lookup[buffer[i] & 0xF];
    }

    message_buffer[(len * 2) + 1] = '\0';

    // Prepare command string
    size_t buffer_size = sizeof(command_buffer);
    int bytes_written = snprintf(command_buffer, buffer_size, "%s \"%s\"", cmd, message_buffer);
    if (bytes_written >= buffer_size) {
        return E2BIG;
    }

    if (debug) {
        fprintf(stderr, "Executing \"%s\"\n", command_buffer);
    }

    // Execute command
    return system(command_buffer);
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s -n NUMBER -c COMMAND [-d]\n\n", progname);
    fprintf(stderr, "\t-c COMMAND, --command COMMAND\n");
    fprintf(stderr, "\t\tthe command to execute\n");
    fprintf(stderr, "\t-d,     --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h,     --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

void main(int argc, char ** argv)
{
    int  option;
    char *command = 0;
    
    while (1) {
        static struct option long_options[] =
        {
            {"command",  required_argument, 0, 'c'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;
        option = getopt_long (argc, argv, "c:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (option == -1)
            break;

        switch (option) {
        case 'c':
            command = optarg;
            break;

        case 'd':
            fprintf(stderr, "debug on\n");
            debug = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    // Initiliaze message queue
    if(ipc_init() != 0) {
        exit(EXIT_FAILURE);
    }

    char buffer[IPC_MESSAGE_MAX_SIZE];
    ssize_t bytes_read;
        
    // Read from message queue and execute user program. 
    while(1) {
        bytes_read = mq_receive(ipc_mq, buffer, IPC_MESSAGE_MAX_SIZE, NULL);
        if(bytes_read >= 0) {
            int status = parse_message(buffer, command, bytes_read);
            if (status == E2BIG) {
                fprintf(stderr, "The specified command is too long. Please shorten it.\n");
                break;
            } else if (status != 0) {
                fprintf(stderr, "Command returned error code %d.\n", status);
            }
        }
    }

    ipc_stop();
}
