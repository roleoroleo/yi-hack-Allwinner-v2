#include "ipc_multiplexer.h"

mqd_t ipc_mq[10];
int debug = 1;

int ipc_init()
{
    int ret, i;

    ret=open_queue(0);
    if(ret != 0)
        return -1;

    for (i = 1; i < 10; i++) {
        open_queue(i);
    }

    return 0;
}

void ipc_stop()
{
    int i;

    for (i = 0; i < 10; i++) {
        if(ipc_mq[i] > 0)
            mq_close(ipc_mq[i]);
    }
}

int open_queue(int n_queue)
{
    char queue_name[256];
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = IPC_MESSAGE_MAX_SIZE;
    attr.mq_curmsgs = 0;

    if (n_queue == 0) {
        sprintf(queue_name, "%s", IPC_QUEUE_NAME);
        ipc_mq[n_queue]=mq_open(queue_name, O_RDWR);

        if(ipc_mq[n_queue]==-1)
        {
            fprintf(stderr, "Can't open mqueue %s. Error: %s\n", queue_name,
                    strerror(errno));
            return -1;
        }
    } else {
        sprintf(queue_name, "%s_%d", IPC_QUEUE_NAME, n_queue);
        ipc_mq[n_queue]=mq_open(queue_name, O_RDWR | O_CREAT | O_NONBLOCK, 0644, &attr);
        if(ipc_mq[n_queue]==-1)
        {
            fprintf(stderr, "Can't open mqueue %s. Error: %s\n", queue_name,
                    strerror(errno));
        }
    }
    return 0;
}

int parse_message(char *msg, ssize_t len)
{
    int i;
    char fwd_msg[256];

    if (debug) fprintf(stderr, "Parsing message.\n");

    fwd_msg[0] = '\0';

    for(i = 0; i < len; i++)
        if (debug) fprintf(stderr, "%02x ", msg[i]);
    if (debug) fprintf(stderr, "\n");

    if((len >= sizeof(IPC_MOTION_START) - 1) && (memcmp(msg, IPC_MOTION_START, sizeof(IPC_MOTION_START) - 1)==0))
    {
        strcpy(fwd_msg, "MOTION_START");
    }
    else if((len >= sizeof(IPC_MOTION_STOP) - 1) && (memcmp(msg, IPC_MOTION_STOP, sizeof(IPC_MOTION_STOP) - 1)==0))
    {
        strcpy(fwd_msg, "MOTION_STOP");
    }
    else if((len >= sizeof(IPC_AI_HUMAN_DETECTION) - 1) && (memcmp(msg, IPC_AI_HUMAN_DETECTION, sizeof(IPC_AI_HUMAN_DETECTION) - 1)==0))
    {
        strcpy(fwd_msg, "AI_HUMAN_DETECTION");
    }
    else if((len >= sizeof(IPC_BABY_CRYING) - 1) && (memcmp(msg, IPC_BABY_CRYING, sizeof(IPC_BABY_CRYING) - 1)==0))
    {
        strcpy(fwd_msg, "BABY_CRYING");
    }
    else if((len >= sizeof(IPC_SOUND_DETECTION) - 1) && (memcmp(msg, IPC_SOUND_DETECTION, sizeof(IPC_SOUND_DETECTION) - 1)==0))
    {
        strcpy(fwd_msg, "SOUND_DETECTION");
    }

    if (fwd_msg[0] == '\0') return 0;

    for (i = 1; i < 10; i++) {
        if (ipc_mq[i] > 0) {
            if (debug) fprintf(stderr, "Sending %s to queue #%d\n", fwd_msg, i);
            mq_send(ipc_mq[i], fwd_msg, strlen(fwd_msg), 0);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret;
    ssize_t bytes_read;
    char buffer[IPC_MESSAGE_MAX_SIZE];

    ret = ipc_init();
    if (ret == -1)
        exit(EXIT_FAILURE);

    while(1) {
        bytes_read = mq_receive(ipc_mq[0], buffer, IPC_MESSAGE_MAX_SIZE, NULL);

        if (debug) fprintf(stderr, "IPC message. Len: %d. Status: %s!\n", bytes_read,
                  strerror(errno));

        if(bytes_read >= 0) {
            parse_message(buffer, bytes_read);
            // make sure to re-send the message
            mq_send(ipc_mq[0], buffer, bytes_read, 0);
        }
        // Wait 10ms to not get back the message we sent
        usleep(10*1000);
    }

    ipc_stop();
}
