#ifndef SET_TZ_OFFSET_H
#define SET_TZ_OFFSET_H

#include <stdlib.h>
#include <mqueue.h>

#define _F_	__FILE__
#define _FU_	__FUNCTION__
#define _L_	__LINE__

#define IPC_QUEUE_NAME "/ipc_dispatch"

#define MID_P2P 1
#define MID_RMM 2
#define MID_CLOUD 4
#define MID_DISPATCH 4
#define MID_RCD 0x10

typedef enum {
    DISPATCH_SET_TZ_OFFSET = 0x8e,
} MSG_TYPE;

typedef struct
{
    int dstMid;
    int srcMid;
    short mainOp;
    short subOp;
    int msgLength;
} mqueue_msg_header_t;

void dump_string(char *source_file, const char *func, int line, char *text, ...);
int cloud_send_msg(mqd_t mqfd, MSG_TYPE msg_type, char *payload, int payload_len);
int cloud_set_tz_offset(int tz_offset);

#endif
