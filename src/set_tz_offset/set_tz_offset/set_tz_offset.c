#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include "set_tz_offset.h"

mqd_t ipc_mq;

void dump_string(char *source_file, const char *func, int line, char *text, ...)
{
    time_t now;
    struct tm *s_now;
    struct timeval tv;

    if (text == '\0') {
        return;
    }

    time(&now);
    s_now = localtime(&now);
    if (s_now != 0) {
        gettimeofday((struct timeval *) &tv, NULL);
        printf("\n%s(%s-%d)[%02d:%02d:%02d.%03d]:", source_file, func, line, s_now->tm_hour,
                s_now->tm_min, s_now->tm_sec, tv.tv_usec >> 10);
        va_list args;
        va_start (args, text);
        vfprintf(stdout, text, args);
        va_end (args);
        putchar(10);
    }
    return;
}

int mqueue_send(mqd_t mqfd,char *send_buf, size_t send_len)
{
    int iRet;

    if (send_len > 512 || mqfd == -1) {
        return -1;
    }
    iRet = mq_send(mqfd, send_buf, send_len, 1);

    return iRet;
}

int cloud_send_msg(mqd_t mqfd, MSG_TYPE msg_type, char *payload, int payload_len)
{
    mqueue_msg_header_t MsgHead;
    char send_buf[1024] = {0};
    int send_len = 0;
    int fsMsgRet = 0;

    memset(&MsgHead, 0, sizeof(MsgHead));
    MsgHead.srcMid = MID_CLOUD;
    MsgHead.mainOp = msg_type;
    MsgHead.subOp = 1;
    MsgHead.msgLength = payload_len;
    MsgHead.dstMid = MID_DISPATCH;

    memcpy(send_buf, &MsgHead, sizeof(MsgHead));
    if(NULL!=payload && payload_len>0)
    {
        memcpy(send_buf + sizeof(MsgHead), payload, payload_len);
    }
    send_len = sizeof(MsgHead) + payload_len;

    fsMsgRet = mqueue_send(mqfd, send_buf, send_len);

    return fsMsgRet;
}

/* Only positive numbers */
int str2int(char *value)
{
    char *endptr;
    int errno;
    int ret;

    errno = 0;    /* To distinguish success/failure after call */

    ret = strtol(value, &endptr, 10);

    /* Check for various possible errors */
    if ((errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN)) || (errno != 0)) {
        return -1;
    }
    if (endptr == optarg) {
        return -1;
    }

    return ret;
}

int cloud_set_tz_offset(int tz_offset)
{
    if(cloud_send_msg(ipc_mq, DISPATCH_SET_TZ_OFFSET, (char *)&tz_offset, sizeof(tz_offset)) < 0)
    {
        dump_string(_F_, _FU_, _L_,  "cloud_set_tz_offset %d send_msg msg fail!\n", tz_offset);
    }
    return 0;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s OPTIONS\n\n", progname);
    fprintf(stderr, "\t-v VALUE, --value VALUE\n");
    fprintf(stderr, "\t\tvalue to set (s)\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv)
{
    int errno;
    char *endptr;
    int c;

    char value[1024] = {0};
    int ivalue;
    int debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"value",  required_argument, 0, 'v'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "v:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {

        case 'v':
            if (strlen(optarg) < sizeof(value) - 1) {
                strcpy(value, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
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

    if (argc == 1) {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
    }
    if (value[0] == '\0') {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    ipc_mq = mq_open("/ipc_dispatch", O_RDWR);
    if (ipc_mq == -1) {
        dump_string(_F_, _FU_, _L_, "open queue fail!\n");
        return -1;
    }

    ivalue = str2int(value) * 1000;
    if (ivalue == -1) {
        printf("Invalid value: %s\n", value);
        return -2;
    }
    printf("Setting timezone to %d ms\n", ivalue);
    cloud_set_tz_offset(ivalue);

    mq_close(ipc_mq);

    return 0;
}
