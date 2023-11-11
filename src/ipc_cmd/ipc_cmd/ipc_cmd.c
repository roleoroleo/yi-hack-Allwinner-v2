/*
 * This file is part of libipc (https://github.com/TheCrypt0/libipc).
 * Copyright (c) 2019 roleo.
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
 * Send message to a IPC queue.
 */

#include "ipc_cmd.h"
#include "getopt.h"
#include "signal.h"

mqd_t ipc_mq;

int open_queue()
{
    ipc_mq = mq_open(IPC_QUEUE_NAME, O_RDWR);
    if(ipc_mq == -1) {
        fprintf(stderr, "Can't open mqueue %s\n", IPC_QUEUE_NAME);
        return -1;
    }
    return 0;
}

int ipc_start()
{
    int ret = -1;

    ret = open_queue();
    if(ret != 0) {
        return -1;
    }

    return 0;
}

void ipc_stop()
{
    if(ipc_mq > 0) {
        mq_close(ipc_mq);
    }
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [t ON/OFF] [-s SENS] [-l LED] [-v WHEN] [-i IR] [-r ROTATE] [-1] [-a AIHUMANDETECTION] [-E AIVEHICLEDETECTION] [-N AIANIMALDETECTION] [-O AIMOTIONDETECTION] [-c FACEDETECTION] [-o MOTIONTRACKING] [-I MIC] [-b SOUNDDETECTION] [-B BABYCRYING] [-n SOUNDSENSITIVITY] [-m MOVE] [-p NUM] [-P] [-R NUM] [-C MODE] [-f FILE] [-S] [-T] [-d]\n\n", progname);
    fprintf(stderr, "\t-t ON/OFF, --switch ON/OFF\n");
    fprintf(stderr, "\t\tswitch ON or OFF the cam\n");
    fprintf(stderr, "\t-s SENS, --sensitivity SENS\n");
    fprintf(stderr, "\t\tset sensitivity: LOW, MEDIUM or HIGH\n");
    fprintf(stderr, "\t-l LED, --led LED\n");
    fprintf(stderr, "\t\tset led: ON or OFF\n");
    fprintf(stderr, "\t-v WHEN, --save WHEN\n");
    fprintf(stderr, "\t\tset save mode: ALWAYS or DETECT\n");
    fprintf(stderr, "\t-i IR, --ir IR\n");
    fprintf(stderr, "\t\tset ir led: ON or OFF\n");
    fprintf(stderr, "\t-r ROTATE, --rotate ROTATE\n");
    fprintf(stderr, "\t\tset rotate: ON or OFF\n");
    fprintf(stderr, "\t-1, --setalertallowstate_human\n");
    fprintf(stderr, "\t\tset alert allow state to human\n");
    fprintf(stderr, "\t-a AIHUMANDETECTION, --aihumandetection AIHUMANDETECTION\n");
    fprintf(stderr, "\t\tset AI Human Detection: ON or OFF\n");
    fprintf(stderr, "\t-E AIVEHICLEDETECTION, --aivehicledetection AIVEHICLEDETECTION\n");
    fprintf(stderr, "\t\tset AI Vehicle Detection: ON or OFF\n");
    fprintf(stderr, "\t-N AIANIMALDETECTION, --aianimaldetection AIANIMALDETECTION\n");
    fprintf(stderr, "\t\tset AI Animal Detection: ON or OFF\n");
    fprintf(stderr, "\t-O AIMOTIONDETECTION, --aimotiondetection AIMOTIONDETECTION\n");
    fprintf(stderr, "\t\tset AI Motion Detection: ON or OFF (disable all other AI detections)\n");
    fprintf(stderr, "\t-c FACEDETECTION, --facedetection FACEDETECTION\n");
    fprintf(stderr, "\t\tset Face Detection: ON or OFF\n");
    fprintf(stderr, "\t-o MOTIONTRACKING, --motiontracking MOTIONTRACKING\n");
    fprintf(stderr, "\t\tset Motion Tracking sensor: ON or OFF\n");
    fprintf(stderr, "\t-I MIC, --mic MIC\n");
    fprintf(stderr, "\t\tset Microphone: ON or OFF\n");
    fprintf(stderr, "\t-b SOUNDDETECTION, --sounddetection SOUNDDETECTION\n");
    fprintf(stderr, "\t\tset Sound Detection: ON or OFF\n");
    fprintf(stderr, "\t-B BABYCRYING, --babycrying BABYCRYING\n");
    fprintf(stderr, "\t\tset baby crying detection: ON or OFF\n");
    fprintf(stderr, "\t-n SOUNDSENSITIVITY, --soundsensitivity SOUNDSENSITIVITY\n");
    fprintf(stderr, "\t\tset Sound Detection Sensitivity: 30 - 90\n");
    fprintf(stderr, "\t-m MOVE, --move MOVE\n");
    fprintf(stderr, "\t\tsend PTZ command: RIGHT, LEFT, DOWN, UP or STOP\n");
    fprintf(stderr, "\t-p TOKEN, --preset TOKEN\n");
    fprintf(stderr, "\t\tsend PTZ go to preset command: TOKEN = [0..7]\n");
    fprintf(stderr, "\t-P TOKEN, --add_preset TOKEN\n");
    fprintf(stderr, "\t\tadd PTZ preset with token TOKEN in the first available position\n");
    fprintf(stderr, "\t\tparameter TOKEN is ignored for Yi cam\n");
    fprintf(stderr, "\t-H, --set_home_position\n");
    fprintf(stderr, "\t\tset home position (preset 0 for Yi cam)\n");
    fprintf(stderr, "\t-R NUM, --remove_preset NUM\n");
    fprintf(stderr, "\t\tremove PTZ preset: NUM = [0..7] or \"all\"\n");
    fprintf(stderr, "\t-C MODE, --cruise MODE\n");
    fprintf(stderr, "\t\tset cruise mode: \"on\", \"off\", \"presets\" or \"360\"\n");
    fprintf(stderr, "\t-f FILE, --file FILE\n");
    fprintf(stderr, "\t\tread binary command from FILE\n");
    fprintf(stderr, "\t-S TIME, --start_motion TIME\n");
    fprintf(stderr, "\t\tstart a motion detection event that lasts TIME\n");
    fprintf(stderr, "\t\t(0<TIME<=300 seconds)\n");
    fprintf(stderr, "\t-T, --stop_motion\n");
    fprintf(stderr, "\t\tstop a motion detection event\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char ** argv)
{
    int errno;
    char *endptr;
    int c, ret, i;
    int switch_on = NONE;
    int sensitivity = NONE;
    int led = NONE;
    int save = NONE;
    int ir = NONE;
    int rotate = NONE;
    int setalertallowstate_human = NONE;
    int aihumandetection = NONE;
    int aivehicledetection = NONE;
    int aianimaldetection = NONE;
    int aimotiondetection = NONE;
    int facedetection = NONE;
    int motiontracking = NONE;
    int mic = NONE;
    int sounddetection = NONE;
    int babycrying = NONE;
    int soundsensitivity = NONE;
    int move = NONE;
    int preset = NONE;
    int add_preset = NONE;
    int set_home_position = NONE;
    int remove_preset = NONE;
    int cruise = NONE;
    int debug = 0;
    unsigned char preset_msg[20];
    int start = 0;
    int stop = 0;
    int time_to_stop = 60;
    char file[1024];
    unsigned char msg_file[1024];
    FILE *fIn;
    int nread = 0;
    int xxx_0 = 0;

    file[0] = '\0';

    while (1) {
        static struct option long_options[] =
        {
            {"switch",  required_argument, 0, 't'},
            {"sensitivity",  required_argument, 0, 's'},
            {"led",  required_argument, 0, 'l'},
            {"save",  required_argument, 0, 'v'},
            {"ir",  required_argument, 0, 'i'},
            {"rotate",  required_argument, 0, 'r'},
            {"setalertallowstate_human",  no_argument, 0, '1'},
            {"aihumandetection",  required_argument, 0, 'a'},
            {"aivehicledetection",  required_argument, 0, 'E'},
            {"aianimaldetection",  required_argument, 0, 'N'},
            {"aimotiondetection",  required_argument, 0, 'O'},
            {"facedetection",  required_argument, 0, 'c'},
            {"motiontracking",  required_argument, 0, 'o'},
            {"mic",  required_argument, 0, 'I'},
            {"sounddetection",  required_argument, 0, 'b'},
            {"babycrying",  required_argument, 0, 'B'},
            {"soundsensitivity",  required_argument, 0, 'n'},
            {"move",  required_argument, 0, 'm'},
            {"move-reverse",  required_argument, 0, 'M'},
            {"preset",  required_argument, 0, 'p'},
            {"add_preset",  required_argument, 0, 'P'},
            {"set_home_position",  no_argument, 0, 'H'},
            {"remove_preset",  required_argument, 0, 'R'},
            {"cruise",  required_argument, 0, 'C'},
            {"file", required_argument, 0, 'f'},
            {"start", required_argument, 0, 'S'},
            {"stop", no_argument, 0, 'T'},
            {"xxx", no_argument, 0, 'x'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "t:s:l:v:i:r:a:E:N:O:c:o:I:b:B:n:m:M:p:P:HR:C:f:S:TxXdh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 't':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                switch_on = SWITCH_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                switch_on = SWITCH_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 's':
            if (strcasecmp("low", optarg) == 0) {
                sensitivity = SENSITIVITY_LOW;
            } else if (strcasecmp("medium", optarg) == 0) {
                sensitivity = SENSITIVITY_MEDIUM;
            } else if (strcasecmp("high", optarg) == 0) {
                sensitivity = SENSITIVITY_HIGH;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'l':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                led = LED_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                led = LED_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'v':
            if ((strcasecmp("always", optarg) == 0) || (strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                save = SAVE_ALWAYS;
            } else if ((strcasecmp("detect", optarg) == 0) || (strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                save = SAVE_DETECT;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'i':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                ir = IR_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                ir = IR_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'r':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                rotate = ROTATE_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                rotate = ROTATE_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case '1':
            setalertallowstate_human = 1;
            break;

        case 'a':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                aihumandetection = AI_HUMAN_DETECTION_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                aihumandetection = AI_HUMAN_DETECTION_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'E':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                aivehicledetection = AI_VEHICLE_DETECTION_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                aivehicledetection = AI_VEHICLE_DETECTION_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'N':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                aianimaldetection = AI_ANIMAL_DETECTION_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                aianimaldetection = AI_ANIMAL_DETECTION_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'O':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                aimotiondetection = AI_MOTION_DETECTION_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                aimotiondetection = AI_MOTION_DETECTION_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'c':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                facedetection = FACE_DETECTION_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                facedetection = FACE_DETECTION_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'o':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                motiontracking = MOTION_TRACKING_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                motiontracking = MOTION_TRACKING_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'I':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                mic = MIC_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                mic = MIC_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'b':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                sounddetection = SOUND_DETECTION_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                sounddetection = SOUND_DETECTION_ON;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'B':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                babycrying = BABYCRYING_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                babycrying = BABYCRYING_ON;
            }
            break;

        case 'n':
            if (strcasecmp("30", optarg) == 0) {
                soundsensitivity = SOUND_SENS_30;
            } else if (strcasecmp("35", optarg) == 0) {
                soundsensitivity = SOUND_SENS_35;
            } else if (strcasecmp("40", optarg) == 0) {
                soundsensitivity = SOUND_SENS_40;
            } else if (strcasecmp("45", optarg) == 0) {
                soundsensitivity = SOUND_SENS_45;
            } else if (strcasecmp("50", optarg) == 0) {
                soundsensitivity = SOUND_SENS_50;
            } else if (strcasecmp("60", optarg) == 0) {
                soundsensitivity = SOUND_SENS_60;
            } else if (strcasecmp("70", optarg) == 0) {
                soundsensitivity = SOUND_SENS_70;
            } else if (strcasecmp("80", optarg) == 0) {
                soundsensitivity = SOUND_SENS_80;
            } else if (strcasecmp("90", optarg) == 0) {
                soundsensitivity = SOUND_SENS_90;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'm':
            if (strcasecmp("right", optarg) == 0) {
                move = MOVE_RIGHT;
            } else if (strcasecmp("left", optarg) == 0) {
                move = MOVE_LEFT;
            } else if (strcasecmp("down", optarg) == 0) {
                move = MOVE_DOWN;
            } else if (strcasecmp("up", optarg) == 0) {
                move = MOVE_UP;
            } else if (strcasecmp("stop", optarg) == 0) {
                move = MOVE_STOP;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'M':
            if (strcasecmp("right", optarg) == 0) {
                move = MOVE_LEFT;
            } else if (strcasecmp("left", optarg) == 0) {
                move = MOVE_RIGHT;
            } else if (strcasecmp("down", optarg) == 0) {
                move = MOVE_UP;
            } else if (strcasecmp("up", optarg) == 0) {
                move = MOVE_DOWN;
            } else if (strcasecmp("stop", optarg) == 0) {
                move = MOVE_STOP;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'p':
            errno = 0;    /* To distinguish success/failure after call */
            preset = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (preset == LONG_MAX || preset == LONG_MIN)) || (errno != 0 && preset == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'P':
            errno = 0;    /* To distinguish success/failure after call */
            add_preset = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (add_preset == LONG_MAX || add_preset == LONG_MIN)) || (errno != 0 && add_preset == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((add_preset < 0) || (add_preset > 7)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'H':
            set_home_position = 1;
            break;

        case 'R':
            errno = 0;    /* To distinguish success/failure after call */
            if (strcasecmp("all", optarg) == 0) {
                remove_preset = 255;
            } else {
                remove_preset = strtol(optarg, &endptr, 10);

                /* Check for various possible errors */
                if ((errno == ERANGE && (remove_preset == LONG_MAX || remove_preset == LONG_MIN)) || (errno != 0 && remove_preset == 0)) {
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                if (endptr == optarg) {
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
            }
            if ((remove_preset != 255) && ((remove_preset < 0) || (remove_preset > 7))) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'C':
            if ((strcasecmp("off", optarg) == 0) || (strcasecmp("no", optarg) == 0)) {
                cruise = CRUISE_OFF;
            } else if ((strcasecmp("on", optarg) == 0) || (strcasecmp("yes", optarg) == 0)) {
                cruise = CRUISE_ON;
            } else if (strcasecmp("presets", optarg) == 0) {
                cruise = CRUISE_PRESETS;
            } else if (strcasecmp("360", optarg) == 0) {
                cruise = CRUISE_360;
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'f':
            /* Check for various possible errors */
            if (strlen(optarg) < 1023) {
                strcpy(file, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'S':
            errno = 0;    /* To distinguish success/failure after call */
            time_to_stop = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (time_to_stop == LONG_MAX || time_to_stop == LONG_MIN)) || (errno != 0 && time_to_stop == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((time_to_stop <= 0) || (time_to_stop > 300)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            start = 1;
            break;

        case 'T':
            stop = 1;
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
            debug = 1;
            break;

        case 'x':
            xxx_0 = 1;
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

    ret=ipc_start();
    if(ret != 0) {
        exit(EXIT_FAILURE);
    }

    if (switch_on == SWITCH_ON) {
        mq_send(ipc_mq, IPC_SWITCH_ON, sizeof(IPC_SWITCH_ON) - 1, 0);
    } else if (switch_on == SWITCH_OFF) {
        mq_send(ipc_mq, IPC_SWITCH_OFF, sizeof(IPC_SWITCH_OFF) - 1, 0);
    }

    if (sensitivity == SENSITIVITY_LOW) {
        mq_send(ipc_mq, IPC_SENS_LOW, sizeof(IPC_SENS_LOW) - 1, 0);
    } else if (sensitivity == SENSITIVITY_MEDIUM) {
        mq_send(ipc_mq, IPC_SENS_MEDIUM, sizeof(IPC_SENS_MEDIUM) - 1, 0);
    } else if (sensitivity == SENSITIVITY_HIGH) {
        mq_send(ipc_mq, IPC_SENS_HIGH, sizeof(IPC_SENS_HIGH) - 1, 0);
    }

    if (led == LED_OFF) {
        mq_send(ipc_mq, IPC_LED_OFF, sizeof(IPC_LED_OFF) - 1, 0);
    } else if (led == LED_ON) {
        mq_send(ipc_mq, IPC_LED_ON, sizeof(IPC_LED_ON) - 1, 0);
    }

    if (save == SAVE_ALWAYS) {
        mq_send(ipc_mq, IPC_SAVE_ALWAYS, sizeof(IPC_SAVE_ALWAYS) - 1, 0);
    } else if (save == SAVE_DETECT) {
        mq_send(ipc_mq, IPC_SAVE_DETECT, sizeof(IPC_SAVE_DETECT) - 1, 0);
    }

    if (ir == IR_OFF) {
        mq_send(ipc_mq, IPC_IR_OFF, sizeof(IPC_IR_OFF) - 1, 0);
    } else if (ir == IR_ON) {
        mq_send(ipc_mq, IPC_IR_ON, sizeof(IPC_IR_ON) - 1, 0);
    }

    if (rotate == ROTATE_OFF) {
        mq_send(ipc_mq, IPC_ROTATE_OFF, sizeof(IPC_ROTATE_OFF) - 1, 0);
    } else if (rotate == ROTATE_ON) {
        mq_send(ipc_mq, IPC_ROTATE_ON, sizeof(IPC_ROTATE_ON) - 1, 0);
    }

    if (setalertallowstate_human == 1) {
        mq_send(ipc_mq, IPC_SET_ALERT_ALLOW_STATE_HUMAN, sizeof(IPC_SET_ALERT_ALLOW_STATE_HUMAN) - 1, 0);
    }

    if (aihumandetection == AI_HUMAN_DETECTION_OFF) {
        mq_send(ipc_mq, IPC_AI_HUMAN_DETECTION_OFF, sizeof(IPC_AI_HUMAN_DETECTION_OFF) - 1, 0);
    } else if (aihumandetection == AI_HUMAN_DETECTION_ON) {
        mq_send(ipc_mq, IPC_AI_HUMAN_DETECTION_ON, sizeof(IPC_AI_HUMAN_DETECTION_ON) - 1, 0);
    }

    if (aivehicledetection == AI_VEHICLE_DETECTION_OFF) {
        mq_send(ipc_mq, IPC_AI_VEHICLE_DETECTION_OFF, sizeof(IPC_AI_VEHICLE_DETECTION_OFF) - 1, 0);
    } else if (aivehicledetection == AI_VEHICLE_DETECTION_ON) {
        mq_send(ipc_mq, IPC_AI_VEHICLE_DETECTION_ON, sizeof(IPC_AI_VEHICLE_DETECTION_ON) - 1, 0);
    }

    if (aianimaldetection == AI_ANIMAL_DETECTION_OFF) {
        mq_send(ipc_mq, IPC_AI_ANIMAL_DETECTION_OFF, sizeof(IPC_AI_ANIMAL_DETECTION_OFF) - 1, 0);
    } else if (aianimaldetection == AI_ANIMAL_DETECTION_ON) {
        mq_send(ipc_mq, IPC_AI_ANIMAL_DETECTION_ON, sizeof(IPC_AI_ANIMAL_DETECTION_ON) - 1, 0);
    }

    if (aimotiondetection == AI_MOTION_DETECTION_OFF) {
        mq_send(ipc_mq, IPC_AI_MOTION_DETECTION_OFF, sizeof(IPC_AI_MOTION_DETECTION_OFF) - 1, 0);
    } else if (aimotiondetection == AI_MOTION_DETECTION_ON) {
        mq_send(ipc_mq, IPC_AI_MOTION_DETECTION_ON, sizeof(IPC_AI_MOTION_DETECTION_ON) - 1, 0);
    }

    if (facedetection == FACE_DETECTION_OFF) {
        mq_send(ipc_mq, IPC_FACE_DETECTION_OFF, sizeof(IPC_FACE_DETECTION_OFF) - 1, 0);
    } else if (facedetection == FACE_DETECTION_ON) {
        mq_send(ipc_mq, IPC_FACE_DETECTION_ON, sizeof(IPC_FACE_DETECTION_ON) - 1, 0);
    }

    if (motiontracking == MOTION_TRACKING_OFF) {
        mq_send(ipc_mq, IPC_MOTION_TRACKING_OFF, sizeof(IPC_MOTION_TRACKING_OFF) - 1, 0);
    } else if (motiontracking == MOTION_TRACKING_ON) {
        mq_send(ipc_mq, IPC_MOTION_TRACKING_ON, sizeof(IPC_MOTION_TRACKING_ON) - 1, 0);
    }

    if (mic == MIC_OFF) {
        mq_send(ipc_mq, IPC_MIC_OFF, sizeof(IPC_MIC_OFF) - 1, 0);
    } else if (mic == MIC_ON) {
        mq_send(ipc_mq, IPC_MIC_ON, sizeof(IPC_MIC_ON) - 1, 0);
    }

    if (sounddetection == SOUND_DETECTION_OFF) {
        mq_send(ipc_mq, IPC_SOUND_DETECTION_OFF, sizeof(IPC_SOUND_DETECTION_OFF) - 1, 0);
    } else if (sounddetection == SOUND_DETECTION_ON) {
        mq_send(ipc_mq, IPC_SOUND_DETECTION_ON, sizeof(IPC_SOUND_DETECTION_ON) - 1, 0);
    }

    if (babycrying == BABYCRYING_OFF) {
        mq_send(ipc_mq, IPC_BABYCRYING_OFF, sizeof(IPC_BABYCRYING_OFF) - 1, 0);
    } else if (babycrying == BABYCRYING_ON) {
        mq_send(ipc_mq, IPC_BABYCRYING_ON, sizeof(IPC_BABYCRYING_ON) - 1, 0);
    }

    if (soundsensitivity == SOUND_SENS_30) {
        mq_send(ipc_mq, IPC_SOUND_SENS_30, sizeof(IPC_SOUND_SENS_30) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_35) {
        mq_send(ipc_mq, IPC_SOUND_SENS_35, sizeof(IPC_SOUND_SENS_35) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_40) {
        mq_send(ipc_mq, IPC_SOUND_SENS_40, sizeof(IPC_SOUND_SENS_40) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_45) {
        mq_send(ipc_mq, IPC_SOUND_SENS_45, sizeof(IPC_SOUND_SENS_45) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_50) {
        mq_send(ipc_mq, IPC_SOUND_SENS_50, sizeof(IPC_SOUND_SENS_50) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_60) {
        mq_send(ipc_mq, IPC_SOUND_SENS_60, sizeof(IPC_SOUND_SENS_60) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_70) {
        mq_send(ipc_mq, IPC_SOUND_SENS_70, sizeof(IPC_SOUND_SENS_70) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_80) {
        mq_send(ipc_mq, IPC_SOUND_SENS_80, sizeof(IPC_SOUND_SENS_80) - 1, 0);
    } else if (soundsensitivity == SOUND_SENS_90) {
        mq_send(ipc_mq, IPC_SOUND_SENS_90, sizeof(IPC_SOUND_SENS_90) - 1, 0);
    }

    if (move == MOVE_RIGHT) {
        mq_send(ipc_mq, IPC_MOVE_RIGHT, sizeof(IPC_MOVE_RIGHT) - 1, 0);
    } else if (move == MOVE_LEFT) {
        mq_send(ipc_mq, IPC_MOVE_LEFT, sizeof(IPC_MOVE_LEFT) - 1, 0);
    } else if (move == MOVE_DOWN) {
        mq_send(ipc_mq, IPC_MOVE_DOWN, sizeof(IPC_MOVE_DOWN) - 1, 0);
    } else if (move == MOVE_UP) {
        mq_send(ipc_mq, IPC_MOVE_UP, sizeof(IPC_MOVE_UP) - 1, 0);
    } else if (move == MOVE_STOP) {
        mq_send(ipc_mq, IPC_MOVE_STOP, sizeof(IPC_MOVE_STOP) - 1, 0);
    }

    if (preset != NONE) {
        memcpy(preset_msg, IPC_GOTO_PRESET, sizeof(IPC_GOTO_PRESET) - 1);
        preset_msg[16] = preset & 0xff;
        mq_send(ipc_mq, preset_msg, sizeof(IPC_GOTO_PRESET) - 1, 0);
    }

    if (add_preset != NONE) {
        // Ignore token because Yi doesn't support it
        mq_send(ipc_mq, IPC_ADD_PRESET, sizeof(IPC_ADD_PRESET) - 1, 0);
    }

    if (set_home_position != NONE) {
        // Remove preset 0
        memcpy(preset_msg, IPC_REMOVE_PRESET, sizeof(IPC_REMOVE_PRESET) - 1);
        preset_msg[16] = 0;
        mq_send(ipc_mq, preset_msg, sizeof(IPC_REMOVE_PRESET) - 1, 0);
        // sleep 1 sec and add it again
        sleep(1);
        mq_send(ipc_mq, IPC_ADD_PRESET, sizeof(IPC_ADD_PRESET) - 1, 0);
    }

    if (remove_preset != NONE) {
        if (remove_preset == 255) {
            for (i = 0; i < 8; i++) {
                memcpy(preset_msg, IPC_REMOVE_PRESET, sizeof(IPC_REMOVE_PRESET) - 1);
                preset_msg[16] = i & 0xff;
                mq_send(ipc_mq, preset_msg, sizeof(IPC_REMOVE_PRESET) - 1, 0);
            }
        } else {
            memcpy(preset_msg, IPC_REMOVE_PRESET, sizeof(IPC_REMOVE_PRESET) - 1);
            preset_msg[16] = remove_preset & 0xff;
            mq_send(ipc_mq, preset_msg, sizeof(IPC_REMOVE_PRESET) - 1, 0);
        }
    }

    if (cruise != NONE) {
        if (cruise == CRUISE_OFF) {
            mq_send(ipc_mq, IPC_CRUISE_OFF, sizeof(IPC_CRUISE_OFF) - 1, 0);
        } else if (cruise == CRUISE_ON) {
            mq_send(ipc_mq, IPC_CRUISE_ON, sizeof(IPC_CRUISE_ON) - 1, 0);
        } else if (cruise == CRUISE_PRESETS) {
            mq_send(ipc_mq, IPC_CRUISE_ON, sizeof(IPC_CRUISE_ON) - 1, 0);
            usleep(100 * 1000);
            mq_send(ipc_mq, IPC_CRUISE_PRESETS, sizeof(IPC_CRUISE_PRESETS) - 1, 0);
        } else if (cruise == CRUISE_360) {
            mq_send(ipc_mq, IPC_CRUISE_ON, sizeof(IPC_CRUISE_ON) - 1, 0);
            usleep(100 * 1000);
            mq_send(ipc_mq, IPC_CRUISE_360, sizeof(IPC_CRUISE_360) - 1, 0);
        }
    }

    if (file[0] != '\0') {
        fIn = fopen(file, "r");
        if (fIn == NULL) {
            fprintf(stderr, "Error opening file %s\n", file);
            exit(EXIT_FAILURE);
        }

        // Tell size
        fseek(fIn, 0L, SEEK_END);
        nread = ftell(fIn);
        fseek(fIn, 0L, SEEK_SET);

        if (fread(msg_file, 1, nread, fIn) != nread) {
            fprintf(stderr, "Error reading file %s\n", file);
            fclose(fIn);
            exit(EXIT_FAILURE);
        }
        fclose(fIn);
        mq_send(ipc_mq, msg_file, nread, 0);
    }

    if (start == 1) {
        mq_send(ipc_mq, IPC_MOTION_START, sizeof(IPC_MOTION_START) - 1, 0);

        pid_t pid;

        /* Fork off the parent process */
        pid = fork();
        /* An error occurred */
        if (pid < 0)
            exit(EXIT_FAILURE);
         /* Success: Let the parent terminate */
        if (pid > 0)
            exit(EXIT_SUCCESS);
        /* On success: The child process becomes session leader */
        if (setsid() < 0)
            exit(EXIT_FAILURE);
        /* Catch, ignore and handle signals */
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        /* Fork off for the second time*/
        pid = fork();
        /* An error occurred */
        if (pid < 0)
            exit(EXIT_FAILURE);
        /* Success: Let the parent terminate */
        if (pid > 0)
            exit(EXIT_SUCCESS);
        /* Set new file permissions */
        umask(0);
        /* Change the working directory to the root directory */
        /* or another appropriated directory */
        chdir("/");
        /* Close all open file descriptors */
//        int x;
//        for (x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
//            close (x);
//        }

        sleep(time_to_stop);
        mq_send(ipc_mq, IPC_MOTION_STOP, sizeof(IPC_MOTION_STOP) - 1, 0);
    }

    if (stop == 1) {
        mq_send(ipc_mq, IPC_MOTION_STOP, sizeof(IPC_MOTION_STOP) - 1, 0);
    }

    if (xxx_0 == 1) {
        mq_send(ipc_mq, IPC_XXX_0, sizeof(IPC_XXX_0) - 1, 0);
    }

    ipc_stop();

    return 0;
}
