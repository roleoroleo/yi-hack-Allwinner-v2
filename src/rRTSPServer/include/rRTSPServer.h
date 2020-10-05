/*
 * Copyright (c) 2020 roleo.
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
 * Dump h264 content from /dev/shm/fshare_frame_buffer and copy it to
 * a circular buffer.
 * Then send the circular buffer to live555.
 */

#ifndef _R_RTSP_SERVER_H
#define _R_RTSP_SERVER_H

//#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <getopt.h>

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define BUF_OFFSET 300
#define BUF_SIZE 1786156
#define FRAME_HEADER_SIZE 22

#define MILLIS_10 10000
#define MILLIS_25 25000

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define OUTPUT_BUFFER_SIZE_LOW  49152
#define OUTPUT_BUFFER_SIZE_HIGH 196608

typedef struct
{
    unsigned char *buffer;
    char filename[256];
    unsigned int size;
    unsigned int offset;
    unsigned char *read_index;               // absolute index
} cb_input_buffer;

typedef struct
{
    unsigned char *buffer;
    unsigned int size;
    int resolution;
    unsigned char *write_index;             // absolute index
    unsigned char *read_index;              // absolute index
    pthread_mutex_t mutex;
} cb_output_buffer;

#endif
