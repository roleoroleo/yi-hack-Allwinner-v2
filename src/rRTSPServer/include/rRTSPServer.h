/*
 * Copyright (c) 2024 roleo.
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
 * Dump h264, h265 and aac content from /dev/shm/fshare_frame_buffer and
 * copy it to a queue.
 * Then send the queue to live555.
 */

#ifndef _R_RTSP_SERVER_H
#define _R_RTSP_SERVER_H

//#define _GNU_SOURCE

#include <queue>
#include <vector>

#include <sys/types.h>

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define BUFFER_SHM "fshare_frame_buf"
#define READ_LOCK_FILE "fshare_read_lock"
#define WRITE_LOCK_FILE "fshare_write_lock"

#define Y20GA 0
#define Y25GA 1
#define Y30QA 2
#define Y501GC 3
#define Y21GA 4
#define Y211GA 5
#define Y211BA 6
#define Y213GA 7
#define Y291GA 8
#define H30GA 9
#define R30GB 10
#define R35GB 11
#define R37GB 12
#define R40GA 13
#define H51GA 14
#define H52GA 15
#define H60GA 16
#define Y28GA 17
#define Y29GA 18
#define Y623 19
#define Q321BR_LSX 20
#define QG311R 21
#define B091QP 22

#define FRAME_HEADER_SIZE_AUTODETECT 0

#define BUF_OFFSET_Y20GA 300
#define FRAME_HEADER_SIZE_Y20GA 22

#define BUF_OFFSET_Y25GA 300
#define FRAME_HEADER_SIZE_Y25GA 22

#define BUF_OFFSET_Y30QA 300
#define FRAME_HEADER_SIZE_Y30QA 22

#define BUF_OFFSET_Y501GC 368
#define FRAME_HEADER_SIZE_Y501GC 24

#define BUF_OFFSET_Y21GA 368
#define FRAME_HEADER_SIZE_Y21GA 28

#define BUF_OFFSET_Y211GA 368
#define FRAME_HEADER_SIZE_Y211GA 28

#define BUF_OFFSET_Y211BA 368
#define FRAME_HEADER_SIZE_Y211BA 28

#define BUF_OFFSET_Y213GA 368
#define FRAME_HEADER_SIZE_Y213GA 28

#define BUF_OFFSET_Y291GA 368
#define FRAME_HEADER_SIZE_Y291GA 28

#define BUF_OFFSET_H30GA 368
#define FRAME_HEADER_SIZE_H30GA 28

#define BUF_OFFSET_R30GB 300
//#define FRAME_HEADER_SIZE_R30GB 22
#define FRAME_HEADER_SIZE_R30GB 0

#define BUF_OFFSET_R35GB 300
#define FRAME_HEADER_SIZE_R35GB 26

#define BUF_OFFSET_R37GB 368
#define FRAME_HEADER_SIZE_R37GB 28

#define BUF_OFFSET_R40GA 300
#define FRAME_HEADER_SIZE_R40GA 26

#define BUF_OFFSET_H51GA 368
#define FRAME_HEADER_SIZE_H51GA 28

#define BUF_OFFSET_H52GA 368
#define FRAME_HEADER_SIZE_H52GA 28

#define BUF_OFFSET_H60GA 368
#define FRAME_HEADER_SIZE_H60GA 28

#define BUF_OFFSET_Y28GA 368
#define FRAME_HEADER_SIZE_Y28GA 28

#define BUF_OFFSET_Y29GA 368
#define FRAME_HEADER_SIZE_Y29GA 28

#define BUF_OFFSET_Y623 368
#define FRAME_HEADER_SIZE_Y623 28

#define BUF_OFFSET_Q321BR_LSX 300
#define FRAME_HEADER_SIZE_Q321BR_LSX 26

#define BUF_OFFSET_QG311R 300
#define FRAME_HEADER_SIZE_QG311R 26

#define BUF_OFFSET_B091QP 300
#define FRAME_HEADER_SIZE_B091QP 26

#define MILLIS_10 10000
#define MILLIS_25 25000

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define TYPE_NONE 0
#define TYPE_LOW  360
#define TYPE_HIGH 1080
#define TYPE_AAC 65521

#define RESOLUTION_FHD  1080
#define RESOLUTION_3K   1296

#define OUTPUT_BUFFER_SIZE_LOW  131072
#define OUTPUT_BUFFER_SIZE_HIGH 524288
#define OUTPUT_BUFFER_SIZE_AUDIO 32768

#define CODEC_NONE 0
#define CODEC_H264 264
#define CODEC_H265 265

typedef struct
{
    unsigned char *buffer;                  // pointer to the base of the input buffer
    char filename[256];                     // name of the buffer file
    unsigned int size;                      // size of the buffer file
    unsigned int offset;                    // offset where stream starts
    unsigned char *read_index;              // read absolute index
} cb_input_buffer;

typedef struct
{
    std::vector<unsigned char> frame;
    uint32_t time;
    int counter;
} output_frame;

typedef struct
{
    std::queue<output_frame> frame_queue;
    pthread_mutex_t mutex;
    unsigned int type;
} output_queue;

struct __attribute__((__packed__)) frame_header {
    uint32_t len;
    uint32_t counter;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
};

struct __attribute__((__packed__)) frame_header_22 {
    uint32_t len;
    uint32_t counter;
    uint32_t u1;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t u4;
};

struct __attribute__((__packed__)) frame_header_24 {
    uint32_t len;
    uint32_t counter;
    uint32_t u1;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t u4;
    uint16_t u5;
};

struct __attribute__((__packed__)) frame_header_26 {
    uint32_t len;
    uint32_t counter;
    uint32_t u1;
    uint32_t u2;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t u4;
};

struct __attribute__((__packed__)) frame_header_28 {
    uint32_t len;
    uint32_t counter;
    uint32_t u1;
    uint32_t u2;
    uint32_t time;
    uint16_t type;
    uint16_t stream_counter;
    uint32_t u4;
};

struct stream_type_s {
    int codec_low;
    int codec_high;
    int sps_type_low;
    int sps_type_high;
    int vps_type_low;
    int vps_type_high;
};

long long current_timestamp();

#endif
