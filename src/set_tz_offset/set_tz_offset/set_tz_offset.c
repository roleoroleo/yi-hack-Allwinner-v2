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
#include <sys/stat.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include "set_tz_offset.h"

int tz_offset_osd_addr[34][2] = {
    { 0x4f4, 1 },    // Y203C
    { 0x4a0, 1 },    // Y23
    { 0x4f4, 1 },    // Y25
    { 0x4a0, 1 },    // Y30
    { 0x4f4, 1 },    // H201C
    { 0x514, 1 },    // H305R
    { 0x4a0, 1 },    // H307

    { 0x4a0, 1 },    // Y20GA_9
    { 0x4a4, 1 },    // Y20GA_12
    { 0x4a0, 1 },    // Y25GA
    { 0x4a0, 1 },    // Y30QA
    { 0x4a4, 1 },    // Y501GC

    { 0x4e0, 1 },    // H30GA_9
    { 0x56c, 1 },    // H30GA_11
    { 0x4e0, 1 },    // H51GA
    { 0x4e0, 1 },    // H52GA
    { 0x560, 1 },    // H60GA
    { 0x4e0, 1 },    // Q321BR_LSX
    { 0x4e0, 1 },    // Q705BR
    { 0x4e0, 1 },    // QG311R
    { 0x4e0, 1 },    // R30GB
    { 0x56c, 1 },    // R35GB
    { 0x560, 1 },    // R40GA
    { 0x4e0, 1 },    // Y211GA_9
    { 0x570, 1 },    // Y211GA_12
    { 0x56c, 1 },    // Y213
    { 0x4e0, 1 },    // Y21GA_9
    { 0x564, 1 },    // Y21GA_12
    { 0x4e0, 1 },    // Y28GA
    { 0x56c, 1 },    // Y291GA_9
    { 0x570, 1 },    // Y291GA_12
    { 0x4e0, 1 },    // Y29GA
    { 0x570, 1 },    // Y623
    { 0x000, 0 }     // B091QP       0x560 or 0x56c - skip this model
};

int debug;
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

int set_tz_offset_osd(int enable, int addr, int val)
{
    int mmap_info_fd;
//    FILE *mmap_info_fs;
    long mmap_info_size;
    unsigned char *mmap_info_ptr;
    int *mmap_info_ptri;

    struct stat st;
    stat(MMAP_INFO, &st);
    mmap_info_size = st.st_size;
    if (debug & 1) fprintf(stderr, "The size of the buffer is %d\n", mmap_info_size);

    // Opening an existing file
    mmap_info_fd = open(MMAP_INFO, O_RDWR);
    if (mmap_info_fd == -1) {
        fprintf(stderr, "Error - could not open file %s\n", MMAP_INFO) ;
        return -1;
    }

    // Map file to memory
    mmap_info_ptr = (unsigned char*) mmap(NULL, mmap_info_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_info_fd, 0);
    if (mmap_info_ptr == MAP_FAILED) {
        fprintf(stderr, "Error - mapping file %s\n", MMAP_INFO);
        close(mmap_info_fd);
        return -2;
    }
    if (debug & 1) fprintf(stderr, "Mapping file %s, size %d, to %08x\n", MMAP_INFO, mmap_info_size, (unsigned int) mmap_info_ptr);

    // Closing the file
    if (debug & 1) fprintf(stderr, "Closing the file %s\n", MMAP_INFO);
    close(mmap_info_fd);

    if (enable == 1) {
        mmap_info_ptr[0] = 1;
        mmap_info_ptr[1] = 0;
        mmap_info_ptr[2] = 0;
        mmap_info_ptr[3] = 0;
        mmap_info_ptr[8] = 0;
        mmap_info_ptr[9] = 0;
        mmap_info_ptr[10] = 0;
        mmap_info_ptr[11] = 0;
    } else if (enable == 0) {
        mmap_info_ptr[0] = 0x10;
        mmap_info_ptr[1] = 0;
        mmap_info_ptr[2] = 0;
        mmap_info_ptr[3] = 0;
        mmap_info_ptr[8] = 3;
        mmap_info_ptr[9] = 0;
        mmap_info_ptr[10] = 0;
        mmap_info_ptr[11] = 0;
    }

    if (addr > 0) {
        mmap_info_ptri = (int *) &mmap_info_ptr[addr];
        if (*mmap_info_ptri != val)
            *mmap_info_ptri = val;
        else
            if (debug & 1) fprintf(stderr, "Value already set\n");
    }

    if (munmap(mmap_info_ptr, mmap_info_size) == -1) {
        fprintf(stderr, "Error - unmapping file\n");
    } else {
        if (debug & 1) fprintf(stderr, "Unmapping file %s, size %d, from %08x\n", MMAP_INFO, mmap_info_size, (unsigned int) mmap_info_ptr);
    }

    return 0;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s OPTIONS\n\n", progname);
    fprintf(stderr, "\t-c COMMAND, --cmd COMMAND\n");
    fprintf(stderr, "\t\tcommand to run: \"tz_offset\", \"tz_offset_osd\" or \"osd\"\n");
    fprintf(stderr, "\t\t\"tz_offset\" - set timezone offset for yi processes\n");
    fprintf(stderr, "\t\t\"tz_offset_osd\" - set timezone offset for osd, if different (requires -m and -f options)\n");
    fprintf(stderr, "\t\t\"osd\" - enable/disable osd (requires -o option)\n");
    fprintf(stderr, "\t-v VALUE, --value VALUE\n");
    fprintf(stderr, "\t\ttimezone offset value to set (s)\n");
    fprintf(stderr, "\t-m MODEL, --model MODEL\n");
    fprintf(stderr, "\t\tset model: y203c, y23, y25, y30, h201c, h305r, h307\n");
    fprintf(stderr, "\t\tset model: y20ga, y25ga, y30qa, y501gc\n");
    fprintf(stderr, "\t\tset model: h30ga, h51ga, h52ga, h60ga, q321br_lsx, qg311r, q705br r30gb, r35gb, r40ga, y211ga, y213ga, y21ga, y28ga, y291ga, y29ga, y623 or b091qp (default y21ga)\n");
    fprintf(stderr, "\t\t(required if tz_offset_osd command is specified)\n");
    fprintf(stderr, "\t-f FIRMWARE_VERSION, --fwver FIRMWARE_VERSION\n");
    fprintf(stderr, "\t\tfirmware version (required if tz_offset_osd command is specified)\n");
    fprintf(stderr, "\t-o, --osd\n");
    fprintf(stderr, "\t\tenable/disable osd: \"on\" or \"off\"\n");
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

    int cmd = -1;
    char value[1024] = {0};
    int model = -1;
    int model_addr = -1;
    int to_set = 1;
    int fw = -1;
    int osd = -1;
    int ivalue;

    debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"cmd",  required_argument, 0, 'c'},
            {"value",  required_argument, 0, 'v'},
            {"model",  required_argument, 0, 'm'},
            {"fwver",  required_argument, 0, 'f'},
            {"osd",  required_argument, 0, 'o'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "c:v:m:f:o:dh",
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

        case 'c':
            if (strcasecmp("tz_offset", optarg) == 0) {
                cmd = 0;
            } else if (strcasecmp("tz_offset_osd", optarg) == 0) {
                cmd = 1;
            } else if (strcasecmp("osd", optarg) == 0) {
                cmd = 2;
            }
            break;

        case 'm':
            if (strcasecmp("y203c", optarg) == 0) {
                model = Y203C;
            } else if (strcasecmp("y23", optarg) == 0) {
                model = Y23;
            } else if (strcasecmp("y25", optarg) == 0) {
                model = Y25;
            } else if (strcasecmp("y30", optarg) == 0) {
                model = Y30;
            } else if (strcasecmp("h201c", optarg) == 0) {
                model = H201C;
            } else if (strcasecmp("h305r", optarg) == 0) {
                model = H305R;
            } else if (strcasecmp("h307", optarg) == 0) {
                model = H307;
            } else if (strcasecmp("y20ga", optarg) == 0) {
                model = Y20GA;
            } else if (strcasecmp("y25ga", optarg) == 0) {
                model = Y25GA;
            } else if (strcasecmp("y30qa", optarg) == 0) {
                model = Y30QA;
            } else if (strcasecmp("y501gc", optarg) == 0) {
                model = Y501GC;
            } else if (strcasecmp("h30ga", optarg) == 0) {
                model = H30GA;
            } else if (strcasecmp("h51ga", optarg) == 0) {
                model = H51GA;
            } else if (strcasecmp("h52ga", optarg) == 0) {
                model = H52GA;
            } else if (strcasecmp("h60ga", optarg) == 0) {
                model = H60GA;
            } else if (strcasecmp("q321br_lsx", optarg) == 0) {
                model = Q321BR_LSX;
            } else if (strcasecmp("q705br", optarg) == 0) {
                model = Q705BR;
            } else if (strcasecmp("qg311r", optarg) == 0) {
                model = QG311R;
            } else if (strcasecmp("r30gb", optarg) == 0) {
                model = R30GB;
            } else if (strcasecmp("r35gb", optarg) == 0) {
                model = R35GB;
            } else if (strcasecmp("r40ga", optarg) == 0) {
                model = R40GA;
            } else if (strcasecmp("y211ga", optarg) == 0) {
                model = Y211GA;
            } else if (strcasecmp("y213ga", optarg) == 0) {
                model = Y213GA;
            } else if (strcasecmp("y21ga", optarg) == 0) {
                model = Y21GA;
            } else if (strcasecmp("y28ga", optarg) == 0) {
                model = Y28GA;
            } else if (strcasecmp("y291ga", optarg) == 0) {
                model = Y291GA;
            } else if (strcasecmp("y29ga", optarg) == 0) {
                model = Y29GA;
            } else if (strcasecmp("y623", optarg) == 0) {
                model = Y623;
            } else if (strcasecmp("b091qp", optarg) == 0) {
                model = B091QP;
            }
            break;

        case 'f':
            if (strncasecmp("12", optarg, 2) == 0) {
                fw = 12;
            } else if (strncasecmp("11", optarg, 2) == 0) {
                fw = 11;
            } else if (strlen(optarg) > 0) {
                fw = optarg[0] - 48;
                if ((fw < 0) || (fw > 9))
                    fw = -1;
            }
            break;

        case 'o':
            if (strcasecmp("on", optarg) == 0) {
                osd = 1;
            } else if (strcasecmp("off", optarg) == 0) {
                osd = 0;
            } else {
                osd = -1;
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


    if (cmd == 0) {

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

    } else if (cmd == 1) {

        if (value[0] == '\0') {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }

        if ((model == -1) || (fw == -1)) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }

        if (fw > 10) {
            model_addr = tz_offset_osd_addr[model + 1][0];
            to_set = tz_offset_osd_addr[model + 1][1];
        } else {
            model_addr = tz_offset_osd_addr[model][0];
            to_set = tz_offset_osd_addr[model][1];
        }
        if ((model_addr > 0) && (to_set == 1)) {
            set_tz_offset_osd(-1, model_addr, str2int(value));
        }
    } else if (cmd == 2) {

        set_tz_offset_osd(osd, -1, -1);

    }

    return 0;
}
