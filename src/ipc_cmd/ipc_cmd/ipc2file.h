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

#ifndef IPC2FILE_H
#define IPC2FILE_H

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

// IPC events
#define IPC_MOTION_START                "\x01\x00\x00\x00\x02\x00\x00\x00\x7c\x00\x7c\x00\x00\x00\x00\x00"
#define IPC_MOTION_START_C              "\x04\x00\x00\x00\x02\x00\x00\x00\x09\x70\x09\x70\x00\x00\x00\x00"
#define IPC_MOTION_STOP                 "\x01\x00\x00\x00\x02\x00\x00\x00\x7d\x00\x7d\x00\x00\x00\x00\x00"
#define IPC_AI_HUMAN_DETECTION          "\x01\x00\x00\x00\x02\x00\x00\x00\xed\x00\xed\x00\x00\x00\x00\x00"
#define IPC_AI_BODY_DETECTION_C         "\x04\x00\x00\x00\x02\x00\x00\x00\x06\x70\x06\x70\x00\x00\x00\x00"
#define IPC_AI_VEHICLE_DETECTION_C      "\x04\x00\x00\x00\x02\x00\x00\x00\x07\x70\x07\x70\x00\x00\x00\x00"
#define IPC_AI_ANIMAL_DETECTION_C       "\x04\x00\x00\x00\x02\x00\x00\x00\x08\x70\x08\x70\x00\x00\x00\x00"
#define IPC_BABY_CRYING                 "\x04\x00\x00\x00\x02\x00\x00\x00\x02\x60\x02\x60\x00\x00\x00\x00"
#define IPC_SOUND_DETECTION             "\x04\x00\x00\x00\x02\x00\x00\x00\x04\x60\x04\x60\x00\x00\x00\x00"

#define PARENT_DIR                      "/tmp/onvif_notify_server"
#define FILE_MOTION_START               "/tmp/onvif_notify_server/motion_alarm"
#define FILE_AI_HUMAN_DETECTION         "/tmp/onvif_notify_server/human_detection"
#define FILE_AI_VEHICLE_DETECTION       "/tmp/onvif_notify_server/vehicle_detection"
#define FILE_AI_ANIMAL_DETECTION        "/tmp/onvif_notify_server/animal_detection"
#define FILE_BABY_CRYING                "/tmp/onvif_notify_server/baby_crying"
#define FILE_SOUND_DETECTION            "/tmp/onvif_notify_server/sound_detection"

typedef enum
{
    IPC_MSG_UNRECOGNIZED,
    IPC_MSG_MOTION_START,
    IPC_MSG_MOTION_START_C,
    IPC_MSG_MOTION_STOP,
    IPC_MSG_AI_HUMAN_DETECTION,
    IPC_MSG_AI_VEHICLE_DETECTION,
    IPC_MSG_AI_ANIMAL_DETECTION,
    IPC_MSG_BABY_CRYING,
    IPC_MSG_SOUND_DETECTION,
    IPC_MSG_COMMAND,
    IPC_MSG_LAST
} IPC_MESSAGE_TYPE;

int ipc_init();
void ipc_stop();
static int open_queue();
static int clear_queue();
static void handle_ipc_unrecognized();
static void handle_ipc_motion_generic(int detect);
int ipc_set_callback(IPC_MESSAGE_TYPE type, void (*f)());
static void call_callback(IPC_MESSAGE_TYPE type);
int parse_message(char *msg, ssize_t len);
int daemonize(int flags);
int check_pid(char *file_name);
int create_pid(char *file_name);
void signal_handler(int signal);
void *sound_stop_thread(void *arg);
void process_event(int *alarm);
void callback_motion_generic(void *arg);
void print_usage(char *progname);

#endif //IPC2FILE_H
