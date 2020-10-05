#include "ipc_read.h"

static mqd_t ipc_mq;
int queue_number;
int debug;

static int open_queue();
static int parse_message(char *msg, ssize_t len);

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
    char queue_name[256];

    sprintf(queue_name, "%s_%d", IPC_QUEUE_NAME, queue_number);
    ipc_mq = mq_open(queue_name, O_RDONLY);
    if(ipc_mq == -1) {
        fprintf(stderr, "Can't open mqueue %s. Error: %s\n", queue_name, strerror(errno));
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

static int parse_message(char *msg, ssize_t len)
{
    int i;

    if (debug) fprintf(stderr, "Parsing message\n");

    for(i=0; i<len; i++)
        if (debug) fprintf(stderr, "%02x ", msg[i]);
    if (debug) fprintf(stderr, "\n");

    msg[len] = '\0';
    printf("%s\n", msg);

    return 0;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s -n NUMBER [-d]\n\n", progname);
    fprintf(stderr, "\t-n NUMBER, --number NUMBER\n");
    fprintf(stderr, "\t\tnumber of the queue (1 to 9)\n");
    fprintf(stderr, "\t-d,     --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h,     --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

void main(int argc, char ** argv)
{
    int ret, c;
    int errno;
    char *endptr;

    ssize_t bytes_read;
    char buffer[IPC_MESSAGE_MAX_SIZE + 1];

    queue_number = -1;
    debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"number",  required_argument, 0, 'n'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "n:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'n':
            errno = 0;    /* To distinguish success/failure after call */
            queue_number = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (queue_number == LONG_MAX || queue_number == LONG_MIN)) || (errno != 0 && queue_number == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (queue_number <= 0 || queue_number >= 10) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
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

    if (queue_number == -1) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    ret = ipc_init();
    if(ret != 0)
        exit(EXIT_FAILURE);

    while(1) {
        bytes_read = mq_receive(ipc_mq, buffer, IPC_MESSAGE_MAX_SIZE, NULL);

        if (debug) fprintf(stderr, "IPC message. Len: %d. Status: %s!\n", bytes_read, strerror(errno));

        if(bytes_read >= 0) {
            parse_message(buffer, bytes_read);
        }

        usleep(500 * 1000);
    }

    ipc_stop();
}
