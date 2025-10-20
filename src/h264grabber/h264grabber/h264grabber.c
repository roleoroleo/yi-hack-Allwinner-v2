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
 * Dump h264 and aac content from /dev/shm/fshare_frame_buffer to stdout or fifo
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>

//#define USE_SEMAPHORE 1
#ifdef USE_SEMAPHORE
#include <semaphore.h>
#endif

#define FRAME_OFFSET_AUTODETECT 0
#define FRAME_OFFSET_TRY_1 300
#define FRAME_OFFSET_TRY_2 368
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

//#define BUF_OFFSET_R30GB 300
#define BUF_OFFSET_R30GB 0
//#define FRAME_HEADER_SIZE_R30GB 22
#define FRAME_HEADER_SIZE_R30GB 0

//#define BUF_OFFSET_R35GB 300
#define BUF_OFFSET_R35GB 0
//#define FRAME_HEADER_SIZE_R35GB 26
#define FRAME_HEADER_SIZE_R35GB 0

//#define BUF_OFFSET_R37GB 368
#define BUF_OFFSET_R37GB 0
//#define FRAME_HEADER_SIZE_R37GB 28
#define FRAME_HEADER_SIZE_R37GB 0

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

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define TYPE_NONE 0
#define TYPE_LOW  360
#define TYPE_HIGH 1080
#define TYPE_AAC 65521

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define BUFFER_SHM "fshare_frame_buf"
#define READ_LOCK_FILE "fshare_read_lock"
#define WRITE_LOCK_FILE "fshare_write_lock"

#define FIFO_NAME_LOW "/tmp/h264_low_fifo"
#define FIFO_NAME_HIGH "/tmp/h264_high_fifo"
#define FIFO_NAME_AAC  "/tmp/aac_audio_fifo"

#define CODEC_NONE 0
#define CODEC_H264 264
#define CODEC_H265 265

/*
Type:
bit 0	IDR
bit 1	SPS
bit 2	PPS
bit 3	VPS
bit 4	?
bit 5	FPS, Width and Height prefix
bit 6 	HEVC 265
bit 7	?

bit 8	AAC 
bit 9 	?
bit 10 	main (high) 
bit 11 	sub  (low) 
bit 12	fast 
bit 13	?
bit 14	?
bit 15	?
*/

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

int buf_offset;
int buf_size;
int frame_header_size;
struct stream_type_s stream_type;

unsigned char IDR4[]                = {0x65, 0xB8};
unsigned char NALx_START[]          = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR4_START[]          = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char IDR5_START[]          = {0x00, 0x00, 0x00, 0x01, 0x26};
unsigned char PFR4_START[]          = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char PFR5_START[]          = {0x00, 0x00, 0x00, 0x01, 0x02};
unsigned char SPS4_START[]          = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char SPS5_START[]          = {0x00, 0x00, 0x00, 0x01, 0x42};
unsigned char PPS4_START[]          = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char PPS5_START[]          = {0x00, 0x00, 0x00, 0x01, 0x44};
unsigned char VPS5_START[]          = {0x00, 0x00, 0x00, 0x01, 0x40};

unsigned char PPS4_HEADER[]         = {0x08, 0x00, 0x00, 0x00};

unsigned char SPS4_640X360[]        = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                       0x01, 0x01, 0x02};
unsigned char SPS4_640X360_TI[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                       0x01, 0x01, 0x40, 0x00, 0x00, 0x7D, 0x00, 0x00,
                                       0x13, 0x88, 0x21};
unsigned char SPS4_1920X1080[]      = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                       0x04, 0x04, 0x04, 0x08};
unsigned char SPS4_1920X1080_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                       0x04, 0x04, 0x05, 0x00, 0x00, 0x03, 0x01, 0xF4,
                                       0x00, 0x00, 0x4E, 0x20, 0x84};
unsigned char SPS4_2304X1296[]      = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x01, 0x20, 0x05, 0x19, 0x37, 0x01,
                                       0x01, 0x01, 0x02};
unsigned char SPS4_2304X1296_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x01, 0x20, 0x05, 0x19, 0x37, 0x01,
                                       0x00, 0x00, 0x40, 0x00, 0x00, 0x7D, 0x00, 0x00,
                                       0x13, 0x88, 0x21};
unsigned char SPS4_2_640X360[]      = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                       0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6E,
                                       0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_2_640X360_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                       0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6E,
                                       0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                       0x00, 0x27, 0x10, 0x42};
unsigned char SPS4_2_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                       0xB8, 0x08, 0x08, 0x08, 0x10};
unsigned char SPS4_2_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                       0xB8, 0x08, 0x08, 0x0A, 0x00, 0x00, 0x03, 0x03,
                                       0xE8, 0x00, 0x00, 0x9C, 0x41, 0x08};
unsigned char SPS4_2_2304X1296[]    = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x02, 0x40, 0x0A, 0x32, 0x6E,
                                       0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_2_2304X1296_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x02, 0x40, 0x0A, 0x32, 0x6E,
                                       0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                       0x00, 0x27, 0x10, 0x42};
unsigned char SPS4_3_640X360[]      = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                       0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6A,
                                       0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_3_640X360_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x14,
                                       0xAC, 0x2C, 0xA8, 0x0A, 0x02, 0xF7, 0x96, 0x6A,
                                       0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                       0x00, 0x27, 0x10, 0x42};
unsigned char SPS4_3_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                       0xA8, 0x08, 0x08, 0x08, 0x10};
unsigned char SPS4_3_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
                                       0xA8, 0x08, 0x08, 0x0A, 0x00, 0x00, 0x03, 0x03,
                                       0xE8, 0x00, 0x00, 0x9C, 0x41, 0x08};
unsigned char SPS4_3_2304X1296[]    = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x02, 0x40, 0x0A, 0x32, 0x6A,
                                       0x02, 0x02, 0x02, 0x04};
unsigned char SPS4_3_2304X1296_TI[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
                                       0xAC, 0x2C, 0xA8, 0x02, 0x40, 0x0A, 0x32, 0x6A,
                                       0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0xFA, 0x00,
                                       0x00, 0x27, 0x10, 0x42};
unsigned char SPS5_1920X1080[]      = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x7B, 0xA0,
                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x08};
//unsigned char SPS5_1920X1080_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
//                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
//                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x7B, 0xA0,
//                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
//                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
//                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
//                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x40,
//                                       0x00, 0x00, 0x7D, 0x00, 0x00, 0x13, 0x88, 0x02};
unsigned char SPS5_2_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xBA, 0xA0,
                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x08};
//unsigned char SPS5_2_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
//                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
//                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xBA, 0xA0,
//                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
//                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
//                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
//                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x40,
//                                       0x00, 0x00, 0x7D, 0x00, 0x00, 0x13, 0x88, 0x02};
unsigned char SPS5_3_2304X1296[]    = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xBA, 0xA0,
                                       0x01, 0x20, 0x20, 0x05, 0x11, 0xFE, 0x5A, 0xEE,
                                       0x44, 0x88, 0x8B, 0xF2, 0xDC, 0xD4, 0x04, 0x04,
                                       0x04, 0x02};
//unsigned char SPS5_3_2304X1296_TI[] = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
//                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
//                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xBA, 0xA0,
//                                       0x01, 0x20, 0x20, 0x05, 0x11, 0xFE, 0x5A, 0xEE,
//                                       0x44, 0x88, 0x8B, 0xF2, 0xDC, 0xD4, 0x04, 0x04,
//                                       0x04, 0x10, 0x00, 0x00, 0x1F, 0x40, 0x00, 0x04,
//                                       0xE2, 0x00, 0x80};
unsigned char VPS5_1920X1080[]      = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0x7B, 0xAC, 0x09};
unsigned char VPS5_1920X1080_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0x7B, 0xAC, 0x0C, 0x00, 0x00, 0x07, 0xD0,
                                       0x00, 0x01, 0x38, 0x81, 0x40};
unsigned char VPS5_2_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0xBA, 0xAC, 0x09};
unsigned char VPS5_2_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0xBA, 0xAC, 0x0C, 0x00, 0x00, 0x07, 0xD0,
                                       0x00, 0x01, 0x38, 0x81, 0x40};
unsigned char VPS5_3_2304X1296[]    = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0xBA, 0xAC, 0x09};
unsigned char VPS5_3_2304X1296_TI[] = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0xBA, 0xAC, 0x0C, 0x00, 0x00, 0x07, 0xD0,
                                       0x00, 0x01, 0x38, 0x81, 0x40};

unsigned char *addr;                      /* Pointer to shared memory region (header) */
int resolution;
int audio;
int sps_timing_info;
int fifo;
int debug;

#ifdef USE_SEMAPHORE
sem_t *sem_fshare_read_lock = SEM_FAILED;
sem_t *sem_fshare_write_lock = SEM_FAILED;
#endif

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

/* Locate a string in the circular buffer */
unsigned char *cb_memmem(unsigned char *src, int src_len, unsigned char *what, int what_len)
{
    unsigned char *p;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, addr + buf_size - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(addr + buf_offset, src + src_len - (addr + buf_offset), what, what_len);
        }
    }
    return p;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > addr + buf_size))
        buf -= (buf_size - buf_offset);
    if ((offset < 0) && (buf < addr + buf_offset))
        buf += (buf_size - buf_offset);

    return buf;
}

// The second argument is the circular buffer
int cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
{
    int ret;

    if (str2 + n > addr + buf_size) {
        ret = memcmp(str1, str2, addr + buf_size - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (addr + buf_size - str2), addr + buf_offset, n - (addr + buf_size - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

// The second argument is the circular buffer
void cb2s_memcpy(unsigned char *dest, unsigned char *src, size_t n)
{
    if (src + n > addr + buf_size) {
        memcpy(dest, src, addr + buf_size - src);
        memcpy(dest + (addr + buf_size - src), addr + buf_offset, n - (addr + buf_size - src));
    } else {
        memcpy(dest, src, n);
    }
}

// The second argument is the circular buffer
void cb2s_headercpy(unsigned char *dest, unsigned char *src, size_t n)
{
    struct frame_header *fh = (struct frame_header *) dest;
    struct frame_header_22 fh22;
    struct frame_header_24 fh24;
    struct frame_header_26 fh26;
    struct frame_header_28 fh28;
    unsigned char *fp = NULL;

    if (n == sizeof(fh22)) {
        fp = (unsigned char *) &fh22;
    } else if (n == sizeof(fh24)) {
        fp = (unsigned char *) &fh24;
    } else if (n == sizeof(fh26)) {
        fp = (unsigned char *) &fh26;
    } else if (n == sizeof(fh28)) {
        fp = (unsigned char *) &fh28;
    }
    if (fp == NULL) return;

    if (src + n > addr + buf_size) {
        memcpy(fp, src, addr + buf_size - src);
        memcpy(fp + (addr + buf_size - src), addr + buf_offset, n - (addr + buf_size - src));
    } else {
        memcpy(fp, src, n);
    }
    if (n == sizeof(fh22)) {
        fh->len = fh22.len;
        fh->counter = fh22.counter;
        fh->time = fh22.time;
        fh->type = fh22.type;
        fh->stream_counter = fh22.stream_counter;
    } else if (n == sizeof(fh24)) {
        fh->len = fh24.len;
        fh->counter = fh24.counter;
        fh->time = fh24.time;
        fh->type = fh24.type;
        fh->stream_counter = fh24.stream_counter;
    } else if (n == sizeof(fh26)) {
        fh->len = fh26.len;
        fh->counter = fh26.counter;
        fh->time = fh26.time;
        fh->type = fh26.type;
        fh->stream_counter = fh26.stream_counter;
    } else if (n == sizeof(fh28)) {
        fh->len = fh28.len;
        fh->counter = fh28.counter;
        fh->time = fh28.time;
        fh->type = fh28.type;
        fh->stream_counter = fh28.stream_counter;
    }
}

#ifdef USE_SEMAPHORE
int sem_fshare_open()
{
    sem_fshare_read_lock = sem_open(READ_LOCK_FILE, O_RDWR);
    if (sem_fshare_read_lock == SEM_FAILED) {
        fprintf(stderr, "error opening %s\n", READ_LOCK_FILE);
        return -1;
    }
    sem_fshare_write_lock = sem_open(WRITE_LOCK_FILE, O_RDWR);
    if (sem_fshare_write_lock == SEM_FAILED) {
        fprintf(stderr, "error opening %s\n", WRITE_LOCK_FILE);
        return -2;
    }
    return 0;
}

void sem_fshare_close()

{
    if (sem_fshare_write_lock != SEM_FAILED) {
        sem_close(sem_fshare_write_lock);
        sem_fshare_write_lock = SEM_FAILED;
    }
    if (sem_fshare_read_lock != SEM_FAILED) {
        sem_close(sem_fshare_read_lock);
        sem_fshare_read_lock = SEM_FAILED;
    }
    return;
}

void sem_write_lock()
{
    int wl, ret = 0;
    int *fshare_frame_buf_start = (int *) addr;

    while (ret == 0) {
        sem_wait(sem_fshare_read_lock);
        wl = *fshare_frame_buf_start;
        if (wl == 0) {
            ret = 1;
        } else {
            sem_post(sem_fshare_read_lock);
            usleep(1000);
        }
    }
    return;
}

void sem_write_unlock()
{
    sem_post(sem_fshare_read_lock);
    return;
}
#endif

void sigpipe_handler(int unused)
{
    // Do nothing
}

void* unlock_fifo_thread(void *data)
{
    int fd;
    char *fifo_name = data;
    unsigned char buffer_fifo[1024];

    fd = open(fifo_name, O_RDONLY);
    read(fd, buffer_fifo, 1024);
    close(fd);

    return NULL;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-m MODEL] [-r RES] [-s] [-f] [-d]\n\n", progname);
    fprintf(stderr, "\t-m MODEL, --model MODEL\n");
    fprintf(stderr, "\t\tset model: y20ga, y25ga, y30qa or y501gc (Allwinner)\n");
    fprintf(stderr, "\t\tset model: y21ga, y211ga, y211ba, y213ga, y291ga, h30ga, r30gb, r35gb, r37gb, r40ga, h51ga, h52ga, h60ga, y28ga, y29ga, y623, q321br_lsx, qg311r or b091qp (Allwinner-v2: default y21ga)\n");
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: LOW, HIGH, BOTH or NONE (default HIGH)\n");
    fprintf(stderr, "\t-a, --audio\n");
    fprintf(stderr, "\t\tenable audio\n");
    fprintf(stderr, "\t-s, --sti\n");
    fprintf(stderr, "\t\tdon't overwrite SPS timing info (default overwrite)\n");
    fprintf(stderr, "\t-f, --fifo\n");
    fprintf(stderr, "\t\tenable fifo output\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
}

int main(int argc, char **argv) {
    unsigned char *buf_idx, *buf_idx_cur, *buf_idx_end, *buf_idx_end_prev;
    unsigned char *buf_idx_start = NULL;
    unsigned char *header_a1, *header_a2;
    char *stdoutbuf;
    FILE *fFS, *fOut, *fOutLow = NULL, *fOutHigh = NULL, *fOutAac = NULL;
    int fshm;
    mode_t mode = 0755;

    int frame_type = TYPE_NONE;
    int frame_len = 0;
    int frame_counter = -1;
    int frame_counter_last_valid_low = -1;
    int frame_counter_last_valid_high = -1;
    int frame_counter_last_valid_audio = -1;

    int i, n, c;
    int write_enable = 0;
    int frame_sync = 0;

    struct frame_header fhs[10];
    unsigned char* fhs_addr[10];
    uint32_t last_counter;

    resolution = RESOLUTION_HIGH;
    audio = 0;
    sps_timing_info = 1;
    fifo = 0;
    debug = 0;

    buf_offset = BUF_OFFSET_Y21GA;
    frame_header_size = FRAME_HEADER_SIZE_Y21GA;

    while (1) {
        static struct option long_options[] =
        {
            {"model",  required_argument, 0, 'm'},
            {"resolution",  required_argument, 0, 'r'},
            {"audio",  no_argument, 0, 'a'},
            {"sti",  no_argument, 0, 's'},
            {"fifo",  no_argument, 0, 'f'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "m:r:afsdh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            if (strcasecmp("y20ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y20GA;
                frame_header_size = FRAME_HEADER_SIZE_Y20GA;
            } else if (strcasecmp("y25ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y25GA;
                frame_header_size = FRAME_HEADER_SIZE_Y25GA;
            } else if (strcasecmp("y30qa", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y30QA;
                frame_header_size = FRAME_HEADER_SIZE_Y30QA;
            } else if (strcasecmp("y501gc", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y501GC;
                frame_header_size = FRAME_HEADER_SIZE_Y501GC;
            } else if (strcasecmp("y21ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y21GA;
                frame_header_size = FRAME_HEADER_SIZE_Y21GA;
            } else if (strcasecmp("y211ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y211GA;
                frame_header_size = FRAME_HEADER_SIZE_Y211GA;
            } else if (strcasecmp("y211ba", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y211BA;
                frame_header_size = FRAME_HEADER_SIZE_Y211BA;
            } else if (strcasecmp("y213ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y213GA;
                frame_header_size = FRAME_HEADER_SIZE_Y213GA;
            } else if (strcasecmp("y291ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y291GA;
                frame_header_size = FRAME_HEADER_SIZE_Y291GA;
            } else if (strcasecmp("h30ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H30GA;
                frame_header_size = FRAME_HEADER_SIZE_H30GA;
            } else if (strcasecmp("r30gb", optarg) == 0) {
                buf_offset = BUF_OFFSET_R30GB;
                frame_header_size = FRAME_HEADER_SIZE_R30GB;
            } else if (strcasecmp("r35gb", optarg) == 0) {
                buf_offset = BUF_OFFSET_R35GB;
                frame_header_size = FRAME_HEADER_SIZE_R35GB;
            } else if (strcasecmp("r37gb", optarg) == 0) {
                buf_offset = BUF_OFFSET_R37GB;
                frame_header_size = FRAME_HEADER_SIZE_R37GB;
            } else if (strcasecmp("r40ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_R40GA;
                frame_header_size = FRAME_HEADER_SIZE_R40GA;
            } else if (strcasecmp("h51ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H51GA;
                frame_header_size = FRAME_HEADER_SIZE_H51GA;
            } else if (strcasecmp("h52ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H52GA;
                frame_header_size = FRAME_HEADER_SIZE_H52GA;
            } else if (strcasecmp("h60ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_H60GA;
                frame_header_size = FRAME_HEADER_SIZE_H60GA;
            } else if (strcasecmp("y28ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y28GA;
                frame_header_size = FRAME_HEADER_SIZE_Y28GA;
            } else if (strcasecmp("y29ga", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y29GA;
                frame_header_size = FRAME_HEADER_SIZE_Y29GA;
            } else if (strcasecmp("y623", optarg) == 0) {
                buf_offset = BUF_OFFSET_Y623;
                frame_header_size = FRAME_HEADER_SIZE_Y623;
            } else if (strcasecmp("q321br_lsx", optarg) == 0) {
                buf_offset = BUF_OFFSET_Q321BR_LSX;
                frame_header_size = FRAME_HEADER_SIZE_Q321BR_LSX;
            } else if (strcasecmp("qg311r", optarg) == 0) {
                buf_offset = BUF_OFFSET_QG311R;
                frame_header_size = FRAME_HEADER_SIZE_QG311R;
            } else if (strcasecmp("b091qp", optarg) == 0) {
                buf_offset = BUF_OFFSET_B091QP;
                frame_header_size = FRAME_HEADER_SIZE_B091QP;
            }
            break;

        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            } else if (strcasecmp("both", optarg) == 0) {
                resolution = RESOLUTION_BOTH;
            } else if (strcasecmp("none", optarg) == 0) {
                resolution = RESOLUTION_NONE;
            }
            break;

        case 'a':
            audio = 1;
            break;

        case 's':
            sps_timing_info = 0;
            break;

        case 'f':
            fprintf (stderr, "using fifo as output\n");
            fifo = 1;
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
            debug = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            return -1;
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    if (fifo == 0) {
        if (resolution == RESOLUTION_BOTH) {
            fprintf(stderr, "Both resolution are not supported with output to stdout\n");
            fprintf(stderr, "Use fifo or run two processes\n");
            return -2;
        } else if ((resolution != RESOLUTION_NONE) && (audio == 1)) {
            fprintf(stderr, "Both video and audio are not supported with output to stdout\n");
            fprintf(stderr, "Use fifo or run two processes\n");
            return -2;
        }
    }

    fFS = fopen(BUFFER_FILE, "r");
    if ( fFS == NULL ) {
        fprintf(stderr, "Could not get size of %s\n", BUFFER_FILE);
        return -3;
    }
    fseek(fFS, 0, SEEK_END);
    buf_size = ftell(fFS);
    fclose(fFS);
    if (debug) fprintf(stderr, "The size of the buffer is %d\n", buf_size);

#ifdef USE_SEMAPHORE
    if (sem_fshare_open() != 0) {
        fprintf(stderr, "Error - could not open semaphores\n") ;
        return -4;
    }
#endif

    // Opening an existing file
    fshm = shm_open(BUFFER_SHM, O_RDWR, 0);
    if ( fshm == -1 ) {
        fprintf(stderr, "Error - could not open file %s\n", BUFFER_FILE) ;
        return -5;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fshm, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Error - mapping file %s\n", BUFFER_FILE);
        close(fshm);
        return -5;
    }
    if (debug) fprintf(stderr, "Mapping file %s, size %d, to %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "Closing the file %s\n", BUFFER_FILE) ;
    close(fshm);

    // Opening/setting output file
    if (fifo == 0) {
        if (resolution == RESOLUTION_LOW) {
            stdoutbuf = (char *) malloc(sizeof(char) * 131072);
            if (setvbuf(stdout, stdoutbuf, _IOFBF, sizeof(stdoutbuf)) != 0) {
                fprintf(stderr, "Error setting stdout buffer\n");
            }
            fOutLow = stdout;
        } else if (resolution == RESOLUTION_HIGH) {
            stdoutbuf = (char *) malloc(sizeof(char) * 524288);
            if (setvbuf(stdout, stdoutbuf, _IOFBF, sizeof(stdoutbuf)) != 0) {
                fprintf(stderr, "Error setting stdout buffer\n");
            }
            fOutHigh = stdout;
        } else if (audio == 1) {
            stdoutbuf = (char *) malloc(sizeof(char) * 32768);
            if (setvbuf(stdout, stdoutbuf, _IOFBF, sizeof(stdoutbuf)) != 0) {
                fprintf(stderr, "Error setting stdout buffer\n");
            }
            fOutAac = stdout;
        }
    } else {
        pthread_t unlock_low_thread, unlock_high_thread, unlock_aac_thread;

        sigaction(SIGPIPE, &(struct sigaction){{sigpipe_handler}}, NULL);

        if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
            unlink(FIFO_NAME_LOW);
            if (mkfifo(FIFO_NAME_LOW, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_LOW);
                return -6;
            }
            if(pthread_create(&unlock_low_thread, NULL, unlock_fifo_thread, (void *) FIFO_NAME_LOW)) {
                fprintf(stderr, "Error creating thread\n");
                return -6;
            }
            pthread_detach(unlock_low_thread);
            fOutLow = fopen(FIFO_NAME_LOW, "w");
            if (fOutLow == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_LOW);
                return -6;
            }
        }
        if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
            unlink(FIFO_NAME_HIGH);
            if (mkfifo(FIFO_NAME_HIGH, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_HIGH);
                return -6;
            }
            if(pthread_create(&unlock_high_thread, NULL, unlock_fifo_thread, (void *) FIFO_NAME_HIGH)) {
                fprintf(stderr, "Error creating thread\n");
                return -6;
            }
            pthread_detach(unlock_high_thread);
            fOutHigh = fopen(FIFO_NAME_HIGH, "w");
            if (fOutHigh == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_HIGH);
                return -6;
            }
        }

        if (audio == 1) {
            unlink(FIFO_NAME_AAC);
            if (mkfifo(FIFO_NAME_AAC, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_AAC);
                return -6;
            }
            if(pthread_create(&unlock_aac_thread, NULL, unlock_fifo_thread, (void *) FIFO_NAME_AAC)) {
                fprintf(stderr, "Error creating thread\n");
                return -6;
            }
            pthread_detach(unlock_aac_thread);
            fOutAac = fopen(FIFO_NAME_AAC, "w");
            if (fOutAac == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_AAC);
                return -6;
            }
        }

        fprintf(stderr, "fifo started\n");
    }

#ifdef USE_SEMAPHORE
    sem_write_lock();
#endif

    // Autodetect offset if not defined
    if (buf_offset == FRAME_OFFSET_AUTODETECT) {
        memcpy(&i, addr + FRAME_OFFSET_TRY_1, sizeof(i));
        if (i != 0)
            buf_offset = FRAME_OFFSET_TRY_1;
        else
            buf_offset = FRAME_OFFSET_TRY_2;
    }

    memcpy(&i, addr + 16, sizeof(i));
    buf_idx = addr + buf_offset + i;
    buf_idx_cur = buf_idx;
    memcpy(&i, addr + 4, sizeof(i));
    buf_idx_end = buf_idx + i;
    if (buf_idx_end >= addr + buf_size) buf_idx_end -= (buf_size - buf_offset);
    buf_idx_end_prev = buf_idx_end;
#ifdef USE_SEMAPHORE
    sem_write_unlock();
#endif
    last_counter = 0;

    // Autodetect header size if not defined
    if ((frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) && (debug)) fprintf(stderr, "detecting frame header size\n");
    while (frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) {
        header_a2 = (unsigned char *) memmem(addr + buf_offset, buf_size - buf_offset, PPS4_START, sizeof(PPS4_START));
        if ((header_a2 != NULL) && (header_a2 - 40 > addr + buf_offset)) {
            header_a1 = cb_move(header_a2, -40);
            header_a1 = (unsigned char *) memmem(header_a1, 40, PPS4_HEADER, sizeof(PPS4_HEADER));
            if (header_a1 != NULL) {
                frame_header_size = header_a2 - header_a1;
                if (frame_header_size < 0)
                        frame_header_size += (buf_size - buf_offset);
                if ((frame_header_size < 0) && (frame_header_size > 40))
                    frame_header_size = FRAME_HEADER_SIZE_AUTODETECT;
            }
        }
        usleep(1000);
    }
    if (debug) fprintf(stderr, "frame header size = %d\n", frame_header_size);

    if (debug) fprintf(stderr, "starting capture main loop\n");

    // Infinite loop
    while (1) {
#ifdef USE_SEMAPHORE
        sem_write_lock();
#endif
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx = addr + buf_offset + i;
        memcpy(&i, addr + 4, sizeof(i));
        buf_idx_end = buf_idx + i;
        if (buf_idx_end >= addr + buf_size) buf_idx_end -= (buf_size - buf_offset);
        // Check if the header is ok
        memcpy(&i, addr + 12, sizeof(i));
        if (buf_idx_end != addr + buf_offset + i) {
            usleep(1000);
            continue;
        }

        if (buf_idx_end == buf_idx_end_prev) {
#ifdef USE_SEMAPHORE
            sem_write_unlock();
#endif
            usleep(10000);
            continue;
        }

        buf_idx_cur = buf_idx_end_prev;
        frame_sync = 1;
        i = 0;

        while (buf_idx_cur != buf_idx_end) {
            cb2s_headercpy((unsigned char *) &fhs[i], buf_idx_cur, frame_header_size);
            // Check the len
            if (fhs[i].len > buf_size - buf_offset - frame_header_size) {
                frame_sync = 0;
                break;
            }
            fhs_addr[i] = buf_idx_cur;
            buf_idx_cur = cb_move(buf_idx_cur, fhs[i].len + frame_header_size);
            i++;
            // Check if the sync is lost
            if (i == 10) {
                frame_sync = 0;
                break;
            }
        }

#ifdef USE_SEMAPHORE
        sem_write_unlock();
#endif

        if (frame_sync == 0) {
            buf_idx_end_prev = buf_idx_end;
            usleep(10000);
            continue;
        }

        n = i;
        // Ignore last frame, it could be corrupted
        if (n > 1) {
            buf_idx_end_prev = fhs_addr[n - 1];
            n--;
        } else {
            usleep(10000);
            continue;
        }

        if (n > 0) {
            if (fhs[0].counter != last_counter + 1) {
                fprintf(stderr, "%lld: warning - %d frame(s) lost\n",
                            current_timestamp(), fhs[0].counter - (last_counter + 1));
            }
            last_counter = fhs[n - 1].counter;
        }

        for (i = 0; i < n; i++) {
            buf_idx_cur = fhs_addr[i];
            frame_len = fhs[i].len;
            // If SPS
            if (fhs[i].type & 0x0002) {
                buf_idx_cur = cb_move(buf_idx_cur, frame_header_size + 6);
                frame_len -= 6;

                // Autodetect stream type (only the 1st time)
                if ((stream_type.codec_low  == CODEC_NONE) && (fhs[i].type & 0x0800)) {
                    if (cb_memcmp(SPS4_640X360, buf_idx_cur, sizeof(SPS4_640X360)) == 0) {
                        stream_type.codec_low = CODEC_H264;
                        stream_type.sps_type_low = 0x0101;
                    } else if (cb_memcmp(SPS4_2_640X360, buf_idx_cur, sizeof(SPS4_2_640X360)) == 0) {
                        stream_type.codec_low = CODEC_H264;
                        stream_type.sps_type_low = 0x0201;
                    } else if (cb_memcmp(SPS4_3_640X360, buf_idx_cur, sizeof(SPS4_3_640X360)) == 0) {
                        stream_type.codec_low = CODEC_H264;
                        stream_type.sps_type_low = 0x0401;
                    } else if (cb_memcmp(SPS4_START, buf_idx_cur, sizeof(SPS4_START)) == 0) {
                        stream_type.codec_low = CODEC_H264;
                        stream_type.sps_type_low = 0x0000;
                    } else if (cb_memcmp(SPS5_START, buf_idx_cur, sizeof(SPS5_START)) == 0) {
                        stream_type.codec_low = CODEC_H265;
                        stream_type.sps_type_low = 0x0000;
                    }
                    if ((debug) && (stream_type.codec_low != CODEC_NONE)) fprintf(stderr, "%lld: low - codec type is %d - sps type is %d\n",
                            current_timestamp(), stream_type.codec_low, stream_type.sps_type_low);
                } else if ((stream_type.codec_high  == CODEC_NONE) && (fhs[i].type & 0x0400)) {
                    if (cb_memcmp(SPS4_1920X1080, buf_idx_cur, sizeof(SPS4_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0102;
                    } else if (cb_memcmp(SPS4_2304X1296, buf_idx_cur, sizeof(SPS4_2304X1296)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0103;
                    } else if (cb_memcmp(SPS4_2_1920X1080, buf_idx_cur, sizeof(SPS4_2_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0202;
                    } else if (cb_memcmp(SPS4_2_2304X1296, buf_idx_cur, sizeof(SPS4_2_2304X1296)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0203;
                    } else if (cb_memcmp(SPS4_3_1920X1080, buf_idx_cur, sizeof(SPS4_3_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0402;
                    } else if (cb_memcmp(SPS4_3_2304X1296, buf_idx_cur, sizeof(SPS4_3_2304X1296)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0403;
                    } else if (cb_memcmp(SPS5_1920X1080, buf_idx_cur, sizeof(SPS5_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H265;
                        stream_type.sps_type_high = 0x0802;
                    } else if (cb_memcmp(SPS5_2_1920X1080, buf_idx_cur, sizeof(SPS5_2_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H265;
                        stream_type.sps_type_high = 0x1002;
                    } else if (cb_memcmp(SPS5_3_2304X1296, buf_idx_cur, sizeof(SPS5_3_2304X1296)) == 0) {
                        stream_type.codec_high = CODEC_H265;
                        stream_type.sps_type_high = 0x2003;
                    } else if (cb_memcmp(SPS4_START, buf_idx_cur, sizeof(SPS4_START)) == 0) {
                        stream_type.codec_high = CODEC_H264;
                        stream_type.sps_type_high = 0x0000;
                    } else if (cb_memcmp(SPS5_START, buf_idx_cur, sizeof(SPS5_START)) == 0) {
                        stream_type.codec_high = CODEC_H265;
                        stream_type.sps_type_high = 0x0000;
                    }
                    if ((debug) && (stream_type.codec_high != CODEC_NONE)) fprintf(stderr, "%lld: high - codec type is %d - sps type is %d\n",
                            current_timestamp(), stream_type.codec_high, stream_type.sps_type_high);
                }
            } else {
                buf_idx_cur = cb_move(buf_idx_cur, frame_header_size);
            }

            write_enable = 1;
            frame_counter = fhs[i].stream_counter;
            if (fhs[i].type & 0x0800) {
                frame_type = TYPE_LOW;
            } else if (fhs[i].type & 0x0400) {
                frame_type = TYPE_HIGH;
            } else if (fhs[i].type & 0x0100) {
                frame_type = TYPE_AAC;
            } else {
                frame_type = TYPE_NONE;
            }
            if ((frame_type == TYPE_LOW) && ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH))) {
                if ((65536 + frame_counter - frame_counter_last_valid_low) % 65536 > 1) {

                    if (debug) fprintf(stderr, "%lld: warning - %d low res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_low - 1) % 65536, frame_counter, frame_counter_last_valid_low);
                    frame_counter_last_valid_low = frame_counter;
                } else {
                    if (debug) {
                        if (fhs[i].type & 0x0002) {
                            fprintf(stderr, "%lld: SPS   detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_low, frame_type);
                        } else {
                            fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_low, frame_type);
                        }
                    }
                    frame_counter_last_valid_low = frame_counter;
                }

                buf_idx_start = buf_idx_cur;
            } else if ((frame_type == TYPE_HIGH) && ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))) {
                if ((65536 + frame_counter - frame_counter_last_valid_high) % 65536 > 1) {

                    if (debug) fprintf(stderr, "%lld: warning - %d high res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_high - 1) % 65536, frame_counter, frame_counter_last_valid_high);
                    frame_counter_last_valid_high = frame_counter;
                } else {
                    if (debug) {
                        if (fhs[i].type & 0x0002) {
                            fprintf(stderr, "%lld: SPS   detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_high, frame_type);
                        } else {
                            fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_high, frame_type);
                        }
                    }
                    frame_counter_last_valid_high = frame_counter;
                }

                buf_idx_start = buf_idx_cur;
            } else if ((frame_type == TYPE_AAC) && (audio == 1)) {
                if ((65536 + frame_counter - frame_counter_last_valid_audio) % 65536 > 1) {
                    if (debug) fprintf(stderr, "%lld: warning - %d AAC frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_audio - 1) % 65536, frame_counter, frame_counter_last_valid_audio);
                    frame_counter_last_valid_audio = frame_counter;
                } else {
                    if (debug) fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - audio AAC\n",
                                current_timestamp(), frame_len, fhs[i].stream_counter);

                    frame_counter_last_valid_audio = frame_counter;
                }
                buf_idx_start = buf_idx_cur;
            } else {
                write_enable = 0;
            }

            // Send the frame to the ouput buffer
            if (write_enable) {
                if ((frame_type == TYPE_LOW) && ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) && (stream_type.codec_low != CODEC_NONE)) {
                    fOut = fOutLow;
                } else if ((frame_type == TYPE_HIGH) && ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) && (stream_type.codec_high != CODEC_NONE)) {
                    fOut = fOutHigh;
                } else if ((frame_type == TYPE_AAC) && (audio == 1)) {
                    fOut = fOutAac;
                } else {
                    fOut = NULL;
                }
                if (fOut != NULL) {
                    if (sps_timing_info) {
                        // Overwrite SPS or VPS with one that contains timing info at 20 fps
                        if (fhs[i].type & 0x0002) {
                            if (frame_type == TYPE_LOW) {
                                if (stream_type.sps_type_low & 0x0101) {
                                    fwrite(SPS4_640X360_TI, 1, sizeof(SPS4_640X360_TI), fOut);
                                } else if (stream_type.sps_type_low & 0x0201) {
                                    fwrite(SPS4_2_640X360_TI, 1, sizeof(SPS4_2_640X360_TI), fOut);
                                } else if (stream_type.sps_type_low & 0x0401) {
                                    fwrite(SPS4_3_640X360_TI, 1, sizeof(SPS4_3_640X360_TI), fOut);
                                } else {
                                    if (buf_idx_start + frame_len > addr + buf_size) {
                                        fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                        fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                                    } else {
                                        fwrite(buf_idx_start, 1, frame_len, fOut);
                                    }
                                }
                            } else if (frame_type == TYPE_HIGH) {
                                if (stream_type.sps_type_high == 0x0102) {
                                    fwrite(SPS4_1920X1080_TI, 1, sizeof(SPS4_1920X1080_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x0202) {
                                    fwrite(SPS4_2_1920X1080_TI, 1, sizeof(SPS4_2_1920X1080_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x0402) {
                                    fwrite(SPS4_3_1920X1080_TI, 1, sizeof(SPS4_3_1920X1080_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x0103) {
                                    fwrite(SPS4_2304X1296_TI, 1, sizeof(SPS4_2304X1296_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x0203) {
                                    fwrite(SPS4_2_2304X1296_TI, 1, sizeof(SPS4_2_2304X1296_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x0403) {
                                    fwrite(SPS4_3_2304X1296_TI, 1, sizeof(SPS4_3_2304X1296_TI), fOut);
/*                                } else if (stream_type.sps_type_high == 0x0802) {
                                    fwrite(SPS5_1920X1080_TI, 1, sizeof(SPS5_1920X1080_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x1002) {
                                    fwrite(SPS5_2_1920X1080_TI, 1, sizeof(SPS5_2_1920X1080_TI), fOut);
                                } else if (stream_type.sps_type_high == 0x2003) {
                                    fwrite(SPS5_3_2304X1296_TI, 1, sizeof(SPS5_3_2304X1296_TI), fOut); */
                                } else {
                                    if (buf_idx_start + frame_len > addr + buf_size) {
                                        fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                        fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                                    } else {
                                        fwrite(buf_idx_start, 1, frame_len, fOut);
                                    }
                                }
                            } else {
                                if (buf_idx_start + frame_len > addr + buf_size) {
                                    fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                    fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                                } else {
                                    fwrite(buf_idx_start, 1, frame_len, fOut);
                                }
                            }
                        } else if (fhs[i].type & 0x0008) {
                            if (frame_type == TYPE_LOW) {
                                if (buf_idx_start + frame_len > addr + buf_size) {
                                    fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                    fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                                } else {
                                    fwrite(buf_idx_start, 1, frame_len, fOut);
                                }
                            } else if (frame_type == TYPE_HIGH) {
/*                                if (stream_type.vps_type_high == 0x0102) {
                                    fwrite(VPS5_1920X1080_TI, 1, sizeof(VPS5_1920X1080_TI), fOut);
                                } else if (stream_type.vps_type_high == 0x0202) {
                                    fwrite(VPS5_2_1920X1080_TI, 1, sizeof(VPS5_2_1920X1080_TI), fOut);
                                } else { */
                                    if (buf_idx_start + frame_len > addr + buf_size) {
                                        fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                        fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                                    } else {
                                        fwrite(buf_idx_start, 1, frame_len, fOut);
                                    }
/*                                } */
                            } else {
                                if (buf_idx_start + frame_len > addr + buf_size) {
                                    fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                    fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                                } else {
                                    fwrite(buf_idx_start, 1, frame_len, fOut);
                                }
                            }
                        } else {
                            if (buf_idx_start + frame_len > addr + buf_size) {
                                fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                                fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                            } else {
                                fwrite(buf_idx_start, 1, frame_len, fOut);
                            }
                        }
                    } else {
                        if (buf_idx_start + frame_len > addr + buf_size) {
                            fwrite(buf_idx_start, 1, addr + buf_size - buf_idx_start, fOut);
                            fwrite(addr + buf_offset, 1, frame_len - (addr + buf_size - buf_idx_start), fOut);
                        } else {
                            fwrite(buf_idx_start, 1, frame_len, fOut);
                        }
                    }
                    if (debug) fprintf(stderr, "%lld: writing frame, length %d\n", current_timestamp(), frame_len);
                }
            }
        }

        usleep(25000);
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(addr, buf_size) == -1) {
        if (debug) fprintf(stderr, "error - unmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);
    }

#ifdef USE_SEMAPHORE
    sem_fshare_close();
#endif

    if (fifo == 0) {
        free(stdoutbuf);
    } else {
        if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
            fclose(fOutLow);
            unlink(FIFO_NAME_LOW);
        }
        if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
            fclose(fOutHigh);
            unlink(FIFO_NAME_HIGH);
        }

        if (audio == 1) {
            fclose(fOutAac);
            unlink(FIFO_NAME_AAC);
        }
    }

    return 0;
}
