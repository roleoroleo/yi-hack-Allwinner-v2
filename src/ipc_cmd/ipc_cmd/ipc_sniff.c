#include "ipc_sniff.h"

// May be set to true using the "IPC_SNIFF_DEBUG" environment
// variable for debugging purposes.
bool debug = false;

// True if message queue and function pointer were initialized.
bool is_initialized = false;

// The message queue onto which all messages will be forwarded.
mqd_t ipc_sniff = -1;

// The original mq_receive function.
ssize_t (*original_mq_receive)(mqd_t, char*, size_t, unsigned int*);

/**
* Initializes the ipc_sniff message queue and looks up the
* original mq_receive function.
**/
void ipc_sniff_initialize() {

    // Enable debug mode if requested
    if (getenv(ENV_IPC_SNIFF_DEBUG)) {
        debug = true;
    }

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 512,
        .mq_msgsize = IPC_MESSAGE_MAX_SIZE,
        .mq_curmsgs = 0
    };
    
    if (debug) fprintf(stderr, "*** [IPC_SNIFF] opening %s message queue.\n", IPC_QUEUE_NAME);

    // Open the message queue or create a new one if it does not exist
    ipc_sniff = mq_open(IPC_QUEUE_NAME, O_RDWR | O_CREAT | O_NONBLOCK, 0644, &attr);
    if(ipc_sniff == -1) {
        fprintf(stderr, "*** [IPC_SNIFF] Can't open mqueue %s. Error: %s\n", IPC_QUEUE_NAME, strerror(errno));
    }

    if (debug) fprintf(stderr, "*** [IPC_SNIFF] opened %s message queue.\n", IPC_QUEUE_NAME);

    if (debug) fprintf(stderr, "*** [IPC_SNIFF] storing original mq_receive function.\n");

    // Find original mq_receive symbol and store it for later usage
    original_mq_receive = dlsym(RTLD_NEXT, "mq_receive");

    if (debug) fprintf(stderr, "*** [IPC_SNIFF] stored original mq_receive function.\n");

    // Remember this function was called
    is_initialized = true;
    if (debug) fprintf(stderr, "*** [IPC_SNIFF] Initialization complete.\n");
}

/**
* First, calls the original mq_receive function and then forwards the received
* message onto the ipc_sniff message queue.
**/
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio) {

    if (is_initialized == false) {
        ipc_sniff_initialize();
    }

    // Call original function to preserve behaviour
    ssize_t bytes_read = original_mq_receive(mqdes, msg_ptr, msg_len, msg_prio);

    if (debug) {
        fprintf(stderr, "*** [IPC_SNIFF] Resending message to ipc_sniff queue.\n");
        fprintf(stderr, "*** [IPC_SNIFF] ");
        for(int i = 0; i < bytes_read; i++)
            fprintf(stderr, "%02x ", msg_ptr[i]);
        fprintf(stderr, "\n");
    }

    // Resend the received message to the sniffer queue
    int status = mq_send(ipc_sniff, msg_ptr, bytes_read, MESSAGE_PRIORITY);
    if (status == 0) {
        if (debug) fprintf(stderr, "*** [IPC_SNIFF] Resent message to ipc_sniff queue.\n");
    } else {
        fprintf(stderr, "*** [IPC_SNIFF] Resending message to ipc_sniff queue failed. error = %s\n", strerror(errno));
    }

    // Return like the original function would do
    return bytes_read;
}