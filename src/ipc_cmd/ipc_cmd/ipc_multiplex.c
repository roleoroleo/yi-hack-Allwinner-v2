#include "ipc_multiplex.h"

// May be set to true using the "IPC_MULTIPLEX_DEBUG" environment
// variable for debugging purposes.
bool debug = false;

// True if message queue and function pointer were initialized.
bool is_initialized = false;

// The message queue onto which all messages will be forwarded.
mqd_t ipc_mq[10];

// The original mq_receive function.
ssize_t (*original_mq_receive)(mqd_t, char*, size_t, unsigned int*);

// A struct storing a message name belonging to a specific id.
typedef struct msg_info {
    unsigned short id;
    char           name[MESSAGE_NAME_MAX_LENGTH];
    unsigned int   name_length;
} msg_info_t;

msg_info_t *message_infos = NULL;
unsigned int message_infos_size = 0;

/**
* Comparator used for sorting and searching arrays of msg_info_t.
**/
int compare_msg_info(const void *a, const void *b) {
    return ((msg_info_t*) a)->id - ((msg_info_t*) b)->id;
}

/**
* Reads and parses the ipc configuration file used for mapping
* message type ids to message names.
**/
void initialize_mapping(char *config_path) {

    // Open config file and check for errors.
    FILE *config_file = fopen(config_path, "r");
    if (config_file == NULL) {
        perror("Opening ipc configuration file failed");
        exit(EXIT_FAILURE);
    }

    // Initialize message info array.
    unsigned int size = 8;
    message_infos = (msg_info_t*) malloc(sizeof(msg_info_t) * size);
    
    unsigned int index = 0;
    unsigned int line_number = 0;
    char line_buffer[MAX_LINE_LENGTH];
    char dummy[MAX_LINE_LENGTH];
    while (fgets(line_buffer, sizeof(line_buffer), config_file)) {
        line_number++;

        // Skip blank lines.
        if (sscanf(line_buffer, " %s", dummy) == EOF) {
            continue;
        }

        // Skip comments.
        if (sscanf(line_buffer, " %[#]", dummy) == 1) {
            continue;
        }

        // Grow array if needed.
        if (index == size) {
            size *= 2;
            message_infos = (msg_info_t*) realloc(message_infos, sizeof(msg_info_t) * size);
        }

        // Try to scan next config line.
        if (sscanf(line_buffer, " %hX = %s", &(message_infos[index].id), message_infos[index].name) == 2) {

            // Remember length for later use.
            message_infos[index].name_length = strlen(message_infos[index].name);

            index++;
            continue;
        }

        fprintf(stderr, "*** [IPC_MULTIPLEX] Error parsing line %d in config file : \"$s\"\n", line_number, line_buffer);
    }

    // Remember array size.
    message_infos_size = size;

    // Sort mapping for faster access using binary search.
    qsort(message_infos, message_infos_size, sizeof(msg_info_t), compare_msg_info);
}

/**
* Initializes the ipc_dispatch_x message queues and looks up the
* original mq_receive function.
**/
void ipc_multiplex_initialize() {

    // Enable debug mode if requested
    if (getenv(ENV_IPC_MULTIPLEX_DEBUG)) {
        debug = true;
    }

    // Prepare attributes for opening message queues.
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 64,
        .mq_msgsize = IPC_MESSAGE_MAX_SIZE,
        .mq_curmsgs = 0
    };

    char queue_name[64];
    for (int i = 1; i < 10; i++) {
        sprintf(queue_name, "%s_%d", IPC_QUEUE_NAME, i);

        // Open the message queue or create a new one if it does not exist
        ipc_mq[i] = mq_open(queue_name, O_RDWR | O_CREAT | O_NONBLOCK, 0644, &attr);
        if(ipc_mq[i] == INVALID_QUEUE) {
            fprintf(stderr, "*** [IPC_MULTIPLEX] Can't open mqueue %s. Error: %s\n", queue_name, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    
    // Find original mq_receive symbol and store it for later usage
    original_mq_receive = dlsym(RTLD_NEXT, "mq_receive");

    // Initialize message mappings.
    initialize_mapping(CONFIG_FILE_PATH);

    // Remember this function was called.
    is_initialized = true;
}

/**
* Extracts the message's target.
*/
inline unsigned int get_message_target(char *msg_ptr) {
    return *((unsigned int*) &(msg_ptr[MESSAGE_TARGET_OFFSET]));
}

/**
* Extracts the message's type.
*/
inline unsigned short get_message_type(char *msg_ptr) {
    return *((unsigned short*) &(msg_ptr[MESSAGE_TYPE_OFFSET]));
}

/**
* First, calls the original mq_receive function and then forwards the received
* message onto the ipc_dispatch_x message queues.
**/
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio) {

    // Initialize resources on first call.
    if (is_initialized == false) {
        ipc_multiplex_initialize();
    }

    // Call original function to preserve behaviour
    ssize_t bytes_read = original_mq_receive(mqdes, msg_ptr, msg_len, msg_prio);

    if (debug) {
        fprintf(stderr, "*** [IPC_MULTIPLEX] ");
        for(int i = 0; i < bytes_read; i++)
            fprintf(stderr, "%02x ", msg_ptr[i]);
        fprintf(stderr, "\n");
    }

    // Filter out messages not targeted at the ipc_dispatch queue.
    if (get_message_target(msg_ptr) != MESSAGE_ID_IPC_DISPATCH) {
        return bytes_read;
    }

    // Lookup message name
    unsigned short message_type = get_message_type(msg_ptr);
    msg_info_t *found = bsearch(&message_type, message_infos, message_infos_size, sizeof(msg_info_t), compare_msg_info);

    // Skip unknown messages
    if (found == NULL) {
        return bytes_read;
    }

    if (debug) {
        fprintf(stderr, "*** [IPC_MULTIPLEX] Resending message \"%s\".\n", found->name);
    }

    // Resend the received message to the dispatch queues
    for (int i = 1; i < 10; i++) {

        // mq_send will fail with EAGAIN whenever the target message queue is full.
        if (mq_send(ipc_mq[i], found->name, found->name_length, MESSAGE_PRIORITY) != 0 && errno != EAGAIN) {
            fprintf(stderr, "*** [IPC_MULTIPLEX] Resending message to %s_%d queue failed. error = %s\n", IPC_QUEUE_NAME, i, strerror(errno));
        };
    }

    // Return like the original function would do
    return bytes_read;
}