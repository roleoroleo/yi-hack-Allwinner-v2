/*
 * Copyright (c) 2023 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Convert events received via IPC queue to tmp files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>

#include "ipc2file.h"

#define DEFAULT_PID_FILE "/var/run/ipc2file.pid"

#define DEFAULT_QUEUE_NUMBER 2

#define BD_NO_CHDIR          01
#define BD_NO_CLOSE_FILES    02
#define BD_NO_REOPEN_STD_FDS 04

#define BD_NO_UMASK0        010
#define BD_MAX_CLOSE       8192

#define PID_SIZE 32

mqd_t ipc_mq;
int queue_number;
int debug;

static int open_queue();
static int clear_queue();
static void call_callback(IPC_MESSAGE_TYPE type);

typedef void(*func_ptr_t)(void* arg);
func_ptr_t *ipc_callbacks;

int exit_main = 0;
int last_alarm = -1;

int ipc_init()
{
    int ret;

    ret = open_queue();
    if(ret != 0)
        return -1;

    ret = clear_queue();
    if(ret != 0)
        return -2;

    ipc_callbacks = malloc((sizeof(func_ptr_t)) * IPC_MSG_LAST);

    return 0;
}

void ipc_stop()
{
    free(ipc_callbacks);

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
        if (debug) fprintf(stderr, "Clear message in queue...\n");
        mq_receive(ipc_mq, buffer, IPC_MESSAGE_MAX_SIZE, NULL);
        if (debug) fprintf(stderr, "...done.\n");
        if (mq_getattr(ipc_mq, &attr) == -1) {
            fprintf(stderr, "Can't get queue attributes\n");
            return -1;
        }
    }

    return 0;
}

static void handle_ipc_unrecognized()
{
    if (debug) fprintf(stderr, "GOT UNRECOGNIZED MESSAGE\n");
//    call_callback(IPC_MSG_UNRECOGNIZED);
}

static void handle_ipc_motion_generic(int detect)
{
    fprintf(stderr, "GOT GENERIC MOTION\n");
    call_callback(detect);
}

int ipc_set_callback(IPC_MESSAGE_TYPE type, void (*f)())
{
    if(type>=IPC_MSG_LAST)
        return -1;

    ipc_callbacks[(int)type]=f;

    return 0;
}

static void call_callback(IPC_MESSAGE_TYPE type)
{
    func_ptr_t f;
    f=ipc_callbacks[(int)type];
    if(f!=NULL)
        (*f)(&type);
}

int parse_message(char *msg, ssize_t len)
{
    int i;

    if (debug) fprintf(stderr, "Parsing message\n");

    for (i = 0; i < len; i++)
        if (debug) fprintf(stderr, "%02x ", msg[i]);
    if (debug) fprintf(stderr, "Parsing message completed\n");

    if((len >= sizeof(IPC_MOTION_START) - 1) && (memcmp(msg, IPC_MOTION_START, sizeof(IPC_MOTION_START) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_MOTION_START);
        return 0;
    }
    if((len >= sizeof(IPC_MOTION_START_C) - 1) && (memcmp(msg, IPC_MOTION_START_C, sizeof(IPC_MOTION_START_C) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_MOTION_START);
        return 0;
    }
    else if((len >= sizeof(IPC_MOTION_STOP) - 1) && (memcmp(msg, IPC_MOTION_STOP, sizeof(IPC_MOTION_STOP) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_MOTION_STOP);
        return 0;
    }
    else if((len >= sizeof(IPC_AI_HUMAN_DETECTION) - 1) && (memcmp(msg, IPC_AI_HUMAN_DETECTION, sizeof(IPC_AI_HUMAN_DETECTION) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_AI_HUMAN_DETECTION);
        return 0;
    }
    else if((len >= sizeof(IPC_AI_BODY_DETECTION_C) - 1) && (memcmp(msg, IPC_AI_BODY_DETECTION_C, sizeof(IPC_AI_BODY_DETECTION_C) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_AI_HUMAN_DETECTION);
        return 0;
    }
    else if((len >= sizeof(IPC_AI_VEHICLE_DETECTION_C) - 1) && (memcmp(msg, IPC_AI_VEHICLE_DETECTION_C, sizeof(IPC_AI_VEHICLE_DETECTION_C) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_AI_VEHICLE_DETECTION);
        return 0;
    }
    else if((len >= sizeof(IPC_AI_ANIMAL_DETECTION_C) - 1) && (memcmp(msg, IPC_AI_ANIMAL_DETECTION_C, sizeof(IPC_AI_ANIMAL_DETECTION_C) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_AI_ANIMAL_DETECTION);
        return 0;
    }
    else if((len >= sizeof(IPC_BABY_CRYING) - 1) && (memcmp(msg, IPC_BABY_CRYING, sizeof(IPC_BABY_CRYING) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_BABY_CRYING);
        return 0;
    }
    else if((len >= sizeof(IPC_SOUND_DETECTION) - 1) && (memcmp(msg, IPC_SOUND_DETECTION, sizeof(IPC_SOUND_DETECTION) - 1)==0))
    {
        handle_ipc_motion_generic(IPC_MSG_SOUND_DETECTION);
        return 0;
    }
    handle_ipc_unrecognized();

    return 0;
}

int daemonize(int flags)
{
    int maxfd, fd;

    switch(fork()) {
        case -1: return -1;
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if(setsid() == -1)
        return -1;

    switch(fork()) {
        case -1: return -1;
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if(!(flags & BD_NO_UMASK0))
        umask(0);

    if(!(flags & BD_NO_CHDIR))
        chdir("/");

    if(!(flags & BD_NO_CLOSE_FILES)) {
        maxfd = sysconf(_SC_OPEN_MAX);
        if(maxfd == -1)
            maxfd = BD_MAX_CLOSE;
        for(fd = 0; fd < maxfd; fd++)
            close(fd);
    }

    if(!(flags & BD_NO_REOPEN_STD_FDS)) {
        close(STDIN_FILENO);

        fd = open("/dev/null", O_RDWR);
        if(fd != STDIN_FILENO)
            return -1;
        if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -2;
        if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -3;
    }

    return 0;
}

int check_pid(char *file_name)
{
    FILE *f;
    long pid;
    char pid_buffer[PID_SIZE];

    f = fopen(file_name, "r");
    if(f == NULL)
        return 0;

    if (fgets(pid_buffer, PID_SIZE, f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);

    if (sscanf(pid_buffer, "%ld", &pid) != 1) {
        return 0;
    }

    if (kill(pid, 0) == 0) {
        return 1;
    }

    return 0;
}

int create_pid(char *file_name)
{
    FILE *f;
    char pid_buffer[PID_SIZE];

    f = fopen(file_name, "w");
    if (f == NULL)
        return -1;

    memset(pid_buffer, '\0', PID_SIZE);
    sprintf(pid_buffer, "%ld\n", (long) getpid());
    if (fwrite(pid_buffer, strlen(pid_buffer), 1, f) != 1) {
        fclose(f);
        return -2;
    }
    fclose(f);

    return 0;
}

void signal_handler(int signal)
{
    // Exit from main loop
    exit_main = 1;
}

void *sound_stop_thread(void *arg)
{
    int i;

    sleep(60);
    remove(FILE_SOUND_DETECTION);

    return NULL;
}

void process_event(int *alarm)
{
    FILE *fp;
    int i;
    int alarm_index = -1;
    char value[8];

    time_t now;

    switch (*alarm) {
        case IPC_MSG_MOTION_START:
            fp = fopen(FILE_MOTION_START, "w");
            if (fp == NULL) {
                fprintf(stderr, "Couldn't open file\n");
                return;
            }
            fclose(fp);
            break;
        case IPC_MSG_AI_HUMAN_DETECTION:
            fp = fopen(FILE_AI_HUMAN_DETECTION, "w");
            if (fp == NULL) {
                fprintf(stderr, "Couldn't open file\n");
                return;
            }
            fclose(fp);
            break;
        case IPC_MSG_AI_VEHICLE_DETECTION:
            fp = fopen(FILE_AI_VEHICLE_DETECTION, "w");
            if (fp == NULL) {
                fprintf(stderr, "Couldn't open file\n");
                return;
            }
            fclose(fp);
            break;
        case IPC_MSG_AI_ANIMAL_DETECTION:
            fp = fopen(FILE_AI_ANIMAL_DETECTION, "w");
            if (fp == NULL) {
                fprintf(stderr, "Couldn't open file\n");
                return;
            }
            fclose(fp);
            break;
        case IPC_MSG_BABY_CRYING:
            fp = fopen(FILE_BABY_CRYING, "w");
            if (fp == NULL) {
                fprintf(stderr, "Couldn't open file\n");
                return;
            }
            fclose(fp);
            break;
        case IPC_MSG_SOUND_DETECTION:
            fp = fopen(FILE_SOUND_DETECTION, "w");
            if (fp == NULL) {
                fprintf(stderr, "Couldn't open file\n");
                return;
            }
            fclose(fp);

            pthread_t sound_stop_pthread;
            pthread_create(&sound_stop_pthread, NULL, sound_stop_thread, NULL);
            pthread_detach(sound_stop_pthread);
            break;
        case IPC_MSG_MOTION_STOP:
            switch (last_alarm) {
                case IPC_MSG_MOTION_START:
                    remove(FILE_MOTION_START);
                    break;
                case IPC_MSG_AI_HUMAN_DETECTION:
                    remove(FILE_AI_HUMAN_DETECTION);
                    break;
                case IPC_MSG_AI_VEHICLE_DETECTION:
                    remove(FILE_AI_VEHICLE_DETECTION);
                    break;
                case IPC_MSG_AI_ANIMAL_DETECTION:
                    remove(FILE_AI_ANIMAL_DETECTION);
                    break;
                case IPC_MSG_BABY_CRYING:
                    remove(FILE_BABY_CRYING);
                    break;
            }
            break;
    }

    if (*alarm != IPC_MSG_SOUND_DETECTION)
        last_alarm = *alarm;

    if (*alarm == IPC_MSG_MOTION_STOP) {
        fprintf(stderr, "CALLBACK MOTION %d STOP\n", *alarm);
    } else {
        fprintf(stderr, "CALLBACK MOTION %d\n", *alarm);
    }
}

void callback_motion_generic(void *arg)
{
    process_event((int *) arg);
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-p PID_FILE] [-q NUM] [-f] [-d LEVEL]\n\n", progname);
    fprintf(stderr, "\t-p PID_FILE, --pid_file PID_FILE\n");
    fprintf(stderr, "\t\tpid file\n");
    fprintf(stderr, "\t-q NUM, --queue_number NUM\n");
    fprintf(stderr, "\t\tid of the ipc queue\n");
    fprintf(stderr, "\t-f, --foreground\n");
    fprintf(stderr, "\t\tdon't daemonize\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv)
{
    int errno;
    char *endptr;
    int c, ret;
    char pid_file[1024];
    int foreground;

    ssize_t bytes_read;
    char buffer[IPC_MESSAGE_MAX_SIZE + 1];
    long size;

    strcpy(pid_file, DEFAULT_PID_FILE);
    queue_number = DEFAULT_QUEUE_NUMBER;
    foreground = 0;
    debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"pid_file",  required_argument, 0, 'p'},
            {"queue_number",  required_argument, 0, 'q'},
            {"foreground",  no_argument, 0, 'f'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "p:q:fdh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'p':
            if (strlen(optarg) < 1024)
                strcpy(pid_file, optarg);
            break;

        case 'q':
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

        case 'f':
            foreground = 1;
            break;

        case 'd':
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

    // Set signal handler
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    if (foreground == 0) {
        ret = daemonize(0);
        if (ret) {
            fprintf(stderr, "Error starting daemon.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Don't daemonize\n");
    }

    fprintf(stderr, "Starting program.\n");

    if (debug) fprintf(stderr, "pid_file = %s\n", pid_file);
    if (debug) fprintf(stderr, "queue_number = %d\n", queue_number);

    // Checking pid file
    if (check_pid(pid_file) == 1) {
        fprintf(stderr, "Program is already running.\n");
        exit(EXIT_FAILURE);
    }
    if (create_pid(pid_file) < 0) {
        fprintf(stderr, "Error creating pid file %s\n", pid_file);
        exit(EXIT_FAILURE);
    }

    struct stat st = {0};
    if (stat(PARENT_DIR, &st) == -1) {
        mkdir(PARENT_DIR, 0755);
    }

    ret = ipc_init();
    if(ret != 0) {
        ipc_stop();
        exit(EXIT_FAILURE);
    }

    ipc_set_callback(IPC_MSG_MOTION_START, &callback_motion_generic);
    ipc_set_callback(IPC_MSG_MOTION_STOP, &callback_motion_generic);
    ipc_set_callback(IPC_MSG_AI_HUMAN_DETECTION, &callback_motion_generic);
    ipc_set_callback(IPC_MSG_AI_VEHICLE_DETECTION, &callback_motion_generic);
    ipc_set_callback(IPC_MSG_AI_ANIMAL_DETECTION, &callback_motion_generic);
    ipc_set_callback(IPC_MSG_BABY_CRYING, &callback_motion_generic);
    ipc_set_callback(IPC_MSG_SOUND_DETECTION, &callback_motion_generic);

    int i;
    while(exit_main != 1) {
        bytes_read = mq_receive(ipc_mq, buffer, IPC_MESSAGE_MAX_SIZE, NULL);

        if (debug) fprintf(stderr, "IPC message. Len: %d. Status: %s!\n", bytes_read, strerror(errno));

        if(bytes_read >= 0) {
            parse_message(buffer, bytes_read);
        }

        usleep(500 * 1000);
    }

    ipc_stop();

    unlink(pid_file);
    fprintf(stderr, "Terminating program.\n");

    return 0;
}
