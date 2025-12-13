/*
 * Copyright (c) 2025 roleo.
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

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "H264VideoFramedMemoryServerMediaSubsession.hh"
#include "H265VideoFramedMemoryServerMediaSubsession.hh"
#include "ADTSAudioFramedMemoryServerMediaSubsession.hh"
#include "ADTSAudioFileServerMediaSubsession_BC.hh"
#include "PCMAudioFileServerMediaSubsession_BC.hh"
#include "WAVAudioFifoServerMediaSubsession.hh"
#include "WAVAudioFifoSource.hh"
#include "AudioFramedMemorySource.hh"
#include "StreamReplicator.hh"
#include "aLawAudioFilter.hh"
#include "PCMFileSink.hh"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <queue>
#include <vector>

#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "rRTSPServer.h"

//#define USE_SEMAPHORE 1
#ifdef USE_SEMAPHORE
#include <semaphore.h>
#endif

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

int debug;                                  /* Set to 1 to debug this .c */
int model;
int resolution;
int audio;
int port;
int sps_timing_info;

#ifdef USE_SEMAPHORE
sem_t *sem_fshare_read_lock = SEM_FAILED;
sem_t *sem_fshare_write_lock = SEM_FAILED;
#endif

cb_input_buffer input_buffer;
output_queue output_queue_high;
output_queue output_queue_low;
output_queue output_queue_audio;
output_queue *p_output_queue;

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

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
        p = (unsigned char*) std::search(src, src+src_len, what, what+what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) std::search(src, input_buffer.buffer + input_buffer.size, what, what + what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) std::search(input_buffer.buffer + input_buffer.offset, src + src_len, what, what + what_len);
        }
    }
    return p;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > input_buffer.buffer + input_buffer.size))
        buf -= (input_buffer.size - input_buffer.offset);
    if ((offset < 0) && (buf < input_buffer.buffer + input_buffer.offset))
        buf += (input_buffer.size - input_buffer.offset);

    return buf;
}

// The second argument is the circular buffer
int cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
{
    int ret;

    if (str2 + n > input_buffer.buffer + input_buffer.size) {
        ret = std::memcmp(str1, str2, input_buffer.buffer + input_buffer.size - str2);
        if (ret != 0) return ret;
        ret = std::memcmp(str1 + (input_buffer.buffer + input_buffer.size - str2), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - str2));
    } else {
        ret = std::memcmp(str1, str2, n);
    }

    return ret;
}

// The second argument is the circular buffer
void cb2s_memcpy(unsigned char *dest, unsigned char *src, size_t n)
{
    if (src + n > input_buffer.buffer + input_buffer.size) {
        std::memcpy(dest, src, input_buffer.buffer + input_buffer.size - src);
        std::memcpy(dest + (input_buffer.buffer + input_buffer.size - src), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - src));
    } else {
        std::memcpy(dest, src, n);
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

    if (src + n > input_buffer.buffer + input_buffer.size) {
        std::memcpy(fp, src, input_buffer.buffer + input_buffer.size - src);
        std::memcpy(fp + (input_buffer.buffer + input_buffer.size - src), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - src));
    } else {
        std::memcpy(fp, src, n);
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
    int *fshare_frame_buf_start = (int *) input_buffer.buffer;

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

void *capture(void *ptr)
{
    unsigned char *buf_idx, *buf_idx_cur, *buf_idx_end, *buf_idx_end_prev;
    unsigned char *buf_idx_start = NULL;
    unsigned char *header_a1, *header_a2;
    int fshm;

    int frame_type = TYPE_NONE;
    int frame_len = 0;
    int frame_counter = -1;
    uint32_t frame_time;
    int frame_counter_last_valid_low = -1;
    int frame_counter_last_valid_high = -1;
    int frame_counter_last_valid_audio = -1;

    int i, n;
    int write_enable = 0;
    int frame_sync = 0;

    struct frame_header fhs[10];
    unsigned char* fhs_addr[10];
    uint32_t last_counter;

#ifdef USE_SEMAPHORE
    if (sem_fshare_open() != 0) {
        fprintf(stderr, "error - could not open semaphores\n") ;
        exit(EXIT_FAILURE);
    }
#endif

    // Opening an existing file
    fshm = shm_open(input_buffer.filename, O_RDWR, 0);
    if (fshm == -1) {
        fprintf(stderr, "error - could not open file %s\n", input_buffer.filename) ;
        exit(EXIT_FAILURE);
    }

    // Map file to memory
    input_buffer.buffer = (unsigned char*) mmap(NULL, input_buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fshm, 0);
    if (input_buffer.buffer == MAP_FAILED) {
        fprintf(stderr, "%lld: capture - error - mapping file %s\n", current_timestamp(), input_buffer.filename);
        close(fshm);
        exit(EXIT_FAILURE);
    }
    if (debug & 3) fprintf(stderr, "%lld: capture - mapping file %s, size %d, to %08x\n", current_timestamp(), input_buffer.filename, input_buffer.size, (unsigned int) input_buffer.buffer);

    // Closing the file
    if (debug & 3) fprintf(stderr, "%lld: capture - closing the file %s\n", current_timestamp(), input_buffer.filename);
    close(fshm) ;

#ifdef USE_SEMAPHORE
    sem_write_lock();
#endif

    // Autodetect offset if not defined
    if (input_buffer.offset == FRAME_OFFSET_AUTODETECT) {
        std::memcpy(&i, input_buffer.buffer + FRAME_OFFSET_TRY_1, sizeof(i));
        if (i != 0)
            input_buffer.offset = FRAME_OFFSET_TRY_1;
        else
            input_buffer.offset = FRAME_OFFSET_TRY_2;
    }

    std::memcpy(&i, input_buffer.buffer + 16, sizeof(i));
    buf_idx = input_buffer.buffer + input_buffer.offset + i;
    buf_idx_cur = buf_idx;
    std::memcpy(&i, input_buffer.buffer + 4, sizeof(i));
    buf_idx_end = buf_idx + i;
    if (buf_idx_end >= input_buffer.buffer + input_buffer.size) buf_idx_end -= (input_buffer.size - input_buffer.offset);
    buf_idx_end_prev = buf_idx_end;
#ifdef USE_SEMAPHORE
    sem_write_unlock();
#endif
    last_counter = 0;

    // Autodetect header size if not defined
    if ((frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) && (debug & 3)) fprintf(stderr, "%lld: capture - detecting frame header size\n", current_timestamp());
    while (frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) {
        header_a2 = (unsigned char *) std::search(input_buffer.buffer + input_buffer.offset, input_buffer.buffer + input_buffer.size, PPS4_START, PPS4_START + sizeof(PPS4_START));
        if ((header_a2 != NULL) && (header_a2 - 40 > input_buffer.buffer + input_buffer.offset)) {
            header_a1 = cb_move(header_a2, -40);
            header_a1 = (unsigned char *) std::search(header_a1, header_a1 + 40, PPS4_HEADER, PPS4_HEADER + sizeof(PPS4_HEADER));
            if (header_a1 != NULL) {
                frame_header_size = header_a2 - header_a1;
                if (frame_header_size < 0)
                        frame_header_size += (input_buffer.size - input_buffer.offset);
                if ((frame_header_size < 0) && (frame_header_size > 40))
                    frame_header_size = FRAME_HEADER_SIZE_AUTODETECT;
            }
        }
        usleep(1000);
    }
    if (debug & 3) fprintf(stderr, "%lld: capture - frame offset = %d\n", current_timestamp(), input_buffer.offset);
    if (debug & 3) fprintf(stderr, "%lld: capture - frame header size = %d\n", current_timestamp(), frame_header_size);

    if (debug & 3) fprintf(stderr, "%lld: capture - starting capture main loop\n", current_timestamp());

    // Infinite loop
    while (1) {
#ifdef USE_SEMAPHORE
        sem_write_lock();
#endif
        std::memcpy(&i, input_buffer.buffer + 16, sizeof(i));
        buf_idx = input_buffer.buffer + input_buffer.offset + i;
        std::memcpy(&i, input_buffer.buffer + 4, sizeof(i));
        buf_idx_end = buf_idx + i;
        if (buf_idx_end >= input_buffer.buffer + input_buffer.size) buf_idx_end -= (input_buffer.size - input_buffer.offset);
        // Check if the header is ok
        std::memcpy(&i, input_buffer.buffer + 12, sizeof(i));
        if (buf_idx_end != input_buffer.buffer + input_buffer.offset + i) {
            if (debug & 3) fprintf(stderr, "%lld: capture - buf_idx_end != input_buffer.buffer + input_buffer.offset + i\n", current_timestamp());
            usleep(1000);
            continue;
        }

        if (buf_idx_end == buf_idx_end_prev) {
#ifdef USE_SEMAPHORE
            sem_write_unlock();
#endif
            if (debug & 3) fprintf(stderr, "%lld: capture - buf_idx_end == buf_idx_end_prev\n", current_timestamp());
            usleep(10000);
            continue;
        }

        buf_idx_cur = buf_idx_end_prev;
        frame_sync = 1;
        i = 0;

        while (buf_idx_cur != buf_idx_end) {
            cb2s_headercpy((unsigned char *) &fhs[i], buf_idx_cur, frame_header_size);
            // Check the len
            if (fhs[i].len > input_buffer.size - input_buffer.offset - frame_header_size) {
                frame_sync = 0;
                if (debug & 3) fprintf(stderr, "%lld: capture - fhs[i].len > input_buffer.size - input_buffer.offset\n", current_timestamp());
                break;
            }
            fhs_addr[i] = buf_idx_cur;
            buf_idx_cur = cb_move(buf_idx_cur, fhs[i].len + frame_header_size);
            i++;
            // Check if the sync is lost
            if (i == 10) {
                frame_sync = 0;
                if (debug & 3) fprintf(stderr, "%lld: capture - i=10\n", current_timestamp());
                break;
            }
        }

#ifdef USE_SEMAPHORE
        sem_write_unlock();
#endif

        if (frame_sync == 0) {
            buf_idx_end_prev = buf_idx_end;
            if (debug & 3) fprintf(stderr, "%lld: capture - frame_sync == 0\n", current_timestamp());
            usleep(10000);
            continue;
        }

        n = i;
        // Ignore last frame, it could be corrupted
        if (n > 1) {
            buf_idx_end_prev = fhs_addr[n - 1];
            n--;
        } else {
            if (debug & 3) fprintf(stderr, "%lld: capture - ! n > 1\n", current_timestamp());
            usleep(10000);
            continue;
        }

        if (n > 0) {
            if (fhs[0].counter != last_counter + 1) {
                fprintf(stderr, "%lld: capture - warning - %d frame(s) lost\n",
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
                    if ((debug & 1) && (stream_type.codec_low != CODEC_NONE)) fprintf(stderr, "%lld: h264 in - low - codec type is %d - sps type is %d\n",
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
                    if ((debug & 1) && (stream_type.codec_high != CODEC_NONE)) fprintf(stderr, "%lld: h264 in - high - codec type is %d - sps type is %d\n",
                            current_timestamp(), stream_type.codec_high, stream_type.sps_type_high);
                }
            } else {
                buf_idx_cur = cb_move(buf_idx_cur, frame_header_size);
            }

            write_enable = 1;
            frame_counter = fhs[i].stream_counter;
            frame_time = fhs[i].time;

            // (fhs[i].type & 0x0002) -> SPS
            // (fhs[i].type & 0x0008) -> VPS
            // (fhs[i].type & 0x0800) -> low res frame
            // (fhs[i].type & 0x0400) -> high res frame
            // (fhs[i].type & 0x0100) -> aac frame
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

                    if (debug & 1) fprintf(stderr, "%lld: h26x in - warning - %d low res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_low - 1) % 65536, frame_counter, frame_counter_last_valid_low);
                    frame_counter_last_valid_low = frame_counter;
                } else {
                    if (debug & 1) {
                        if (fhs[i].type & 0x0002) {
                            fprintf(stderr, "%lld: h26x in - SPS detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_low, frame_type);
                        } else {
                            fprintf(stderr, "%lld: h26x in - frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_low, frame_type);
                        }
                    }
                    frame_counter_last_valid_low = frame_counter;
                }

                buf_idx_start = buf_idx_cur;
            } else if ((frame_type == TYPE_HIGH) && ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))) {
                if ((65536 + frame_counter - frame_counter_last_valid_high) % 65536 > 1) {

                    if (debug & 1) fprintf(stderr, "%lld: h26x in - warning - %d high res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_high - 1) % 65536, frame_counter, frame_counter_last_valid_high);
                    frame_counter_last_valid_high = frame_counter;
                } else {
                    if (debug & 1) {
                        if (fhs[i].type & 0x0002) {
                            fprintf(stderr, "%lld: h26x in - SPS detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_high, frame_type);
                        } else {
                            fprintf(stderr, "%lld: h26x in - frame detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                                    current_timestamp(), frame_len, frame_counter,
                                    frame_counter_last_valid_high, frame_type);
                        }
                    }
                    frame_counter_last_valid_high = frame_counter;
                }

                buf_idx_start = buf_idx_cur;
            } else if ((frame_type == TYPE_AAC) && (audio == 2)) {
                if ((65536 + frame_counter - frame_counter_last_valid_audio) % 65536 > 1) {
                    if (debug & 2) fprintf(stderr, "%lld: aac in - warning - %d AAC frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_audio - 1) % 65536, frame_counter, frame_counter_last_valid_audio);
                    frame_counter_last_valid_audio = frame_counter;
                } else {
                    if (debug & 2) fprintf(stderr, "%lld: aac in - frame detected - frame_len: %d - frame_counter: %d - audio AAC\n",
                                current_timestamp(), frame_len, fhs[i].stream_counter);

                    frame_counter_last_valid_audio = frame_counter;
                }
                buf_idx_start = buf_idx_cur;
            } else {
                write_enable = 0;
            }

            // Send the frame to the ouput buffer
            if (write_enable) {
                if ((frame_type == TYPE_LOW) && (resolution != RESOLUTION_HIGH) && (stream_type.codec_low != CODEC_NONE)) {
                    p_output_queue = &output_queue_low;
                } else if ((frame_type == TYPE_HIGH) && (resolution != RESOLUTION_LOW) && (stream_type.codec_high != CODEC_NONE)) {
                    p_output_queue = &output_queue_high;
                } else if ((frame_type == TYPE_AAC) && (audio != 0)) {
                    p_output_queue = &output_queue_audio;
                } else {
                    p_output_queue = NULL;
                }

                if (p_output_queue != NULL) {
                    if (debug & 3) fprintf(stderr, "%lld: h264/aac in - frame_len: %d - cb_current->size: %d\n", current_timestamp(), frame_len, p_output_queue->frame_queue.size());
                    pthread_mutex_lock(&(p_output_queue->mutex));
                    input_buffer.read_index = buf_idx_start;
                    unsigned char *tmp_out;

                    if (sps_timing_info) {
                        // Overwrite SPS or VPS with one that contains timing info at 20 fps
                        if (fhs[i].type & 0x0002) {
                            if (frame_type == TYPE_LOW) {
                                if (stream_type.sps_type_low & 0x0101) {
                                    frame_len = sizeof(SPS4_640X360_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_640X360_TI, frame_len);
                                } else if (stream_type.sps_type_low & 0x0201) {
                                    frame_len = sizeof(SPS4_2_640X360_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_2_640X360_TI, frame_len);
                                } else if (stream_type.sps_type_low & 0x0401) {
                                    frame_len = sizeof(SPS4_3_640X360_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_3_640X360_TI, frame_len);
                                } else {
                                    // don't change frame_len
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                                }
                            } else if (frame_type == TYPE_HIGH) {
                                if (stream_type.sps_type_high == 0x0102) {
                                    frame_len = sizeof(SPS4_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_1920X1080_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x0202) {
                                    frame_len = sizeof(SPS4_2_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_2_1920X1080_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x0402) {
                                    frame_len = sizeof(SPS4_3_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_3_1920X1080_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x0103) {
                                    frame_len = sizeof(SPS4_2304X1296_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_2304X1296_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x0203) {
                                    frame_len = sizeof(SPS4_2_2304X1296_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_2_2304X1296_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x0403) {
                                    frame_len = sizeof(SPS4_3_2304X1296_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS4_3_2304X1296_TI, frame_len);
/*                                } else if (stream_type.sps_type_high & 0x0802) {
                                    frame_len = sizeof(SPS5_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS5_1920X1080_TI, frame_len);
                                } else if (stream_type.sps_type_high & 0x1002) {
                                    frame_len = sizeof(SPS5_2_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS5_2_1920X1080_TI, frame_len);
                                } else if (stream_type.sps_type_high & 0x2002) {
                                    frame_len = sizeof(SPS5_3_2304X1296_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, SPS5_3_2304X1296_TI, frame_len); */
                                } else {
                                    // don't change frame_len
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                                }
                            } else {
                                // don't change frame_len
                                tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                            }
                        } else if (fhs[i].type & 0x0008) {
                            if (frame_type == TYPE_LOW) {
                                // don't change frame_len
                                tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                            } else if (frame_type == TYPE_HIGH) {
/*                                if (stream_type.sps_type_high == 0x0802) {
                                    frame_len = sizeof(VPS5_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, VPS5_1920X1080_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x1002) {
                                    frame_len = sizeof(VPS5_2_1920X1080_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, VPS5_2_1920_1080_TI, frame_len);
                                } else if (stream_type.sps_type_high == 0x2002) {
                                    frame_len = sizeof(VPS5_3_2304X1296_TI);
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, VPS5_3_2304X1296_TI, frame_len);
                                } else { */
                                    // don't change frame_len
                                    tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                    cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
/*                                } */
                            } else {
                                // don't change frame_len
                                tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                                cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                            }
                        } else {
                            // don't change frame_len
                            tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                            cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                        }

                    } else {
                        tmp_out = (unsigned char *) malloc(frame_len * sizeof(unsigned char));
                        cb2s_memcpy(tmp_out, input_buffer.read_index, frame_len);
                    }

                    output_frame of;
                    of.frame = {tmp_out, tmp_out + frame_len};
                    of.counter = frame_counter;
                    of.time = frame_time;
                    p_output_queue->frame_queue.push(of);
                    free(tmp_out);
                    while (p_output_queue->frame_queue.size() > MAX_QUEUE_SIZE) p_output_queue->frame_queue.pop();

                    if (debug & 3) {
                        fprintf(stderr, "%lld: h264/aac in - frame_len: %d - frame_counter: %d - resolution: %d\n", current_timestamp(), frame_len, frame_counter, frame_type);
                    }
                    pthread_mutex_unlock(&(p_output_queue->mutex));
                }
            }
        }

        usleep(10000);
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(input_buffer.buffer, input_buffer.size) == -1) {
        fprintf(stderr, "%lld: capture - error - unmapping file\n", current_timestamp());
    } else {
        if (debug & 3) fprintf(stderr, "%lld: capture - unmapping file %s, size %d, from %08x\n", current_timestamp(), input_buffer.filename, input_buffer.size, (unsigned int) input_buffer.buffer);
    }

#ifdef USE_SEMAPHORE
    sem_fshare_close();
#endif

    return NULL;
}

StreamReplicator* startReplicatorStream(const char* inputAudioFileName, unsigned samplingFrequency,
        unsigned char numChannels, unsigned char bitsPerSample, int convertTo) {

    FramedSource* resultSource = NULL;

    // Create a single WAVAudioFifo source that will be replicated for mutliple streams
    WAVAudioFifoSource* wavSource = WAVAudioFifoSource::createNew(*env, inputAudioFileName, samplingFrequency, numChannels, bitsPerSample);
    if (wavSource == NULL) {
        fprintf(stderr, "Failed to create Fifo Source \n");
    }

    // Optionally convert to uLaw or aLaw pcm
    if (convertTo == WA_PCMA) {
        resultSource = aLawFromPCMAudioSource::createNew(*env, wavSource, 1/*little-endian*/);
    } else if (convertTo == WA_PCMU) {
        resultSource = uLawFromPCMAudioSource::createNew(*env, wavSource, 1/*little-endian*/);
    } else {
        resultSource = EndianSwap16::createNew(*env, wavSource);
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, resultSource);

    // Begin by creating an input stream from our replicator:
    replicator->createStreamReplica();

    return replicator;
}

StreamReplicator* startReplicatorStream(output_queue *qBuffer, unsigned samplingFrequency, unsigned char numChannels, Boolean useTimeForPres) {
    // Create a single ADTSFromWAVAudioFifo source that will be replicated for mutliple streams
    AudioFramedMemorySource* adtsSource = AudioFramedMemorySource::createNew(*env, qBuffer, samplingFrequency, numChannels, useTimeForPres);
    if (adtsSource == NULL) {
        fprintf(stderr, "Failed to create Fifo Source \n");
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, adtsSource);

    // Begin by creating an input stream from our replicator:
    replicator->createStreamReplica();

    return replicator;
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms, char const* streamName, int audio)
{
    char* url = rtspServer->rtspURL(sms);
    fprintf(stderr, "\n\"%s\" stream, from memory\n", streamName);
    if (audio == 1)
        fprintf(stderr, "PCM audio enabled\n");
    else if (audio == 2)
        fprintf(stderr, "AAC audio enabled\n");
    fprintf(stderr, "Play this stream using the URL \"%s\"\n", url);
    delete[] url;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [options]\n\n", progname);
    fprintf(stderr, "\t-m MODEL, --model MODEL\n");
    fprintf(stderr, "\t\tset model: y20ga, y25ga, y30qa or y501gc (Allwinner)\n");
    fprintf(stderr, "\t\tset model: y21ga, y211ga, y211ba, y213ga, y291ga, h30ga, r30gb, r35gb, r37gb, r40ga, h51ga, h52ga, h60ga, y28ga, y29ga, y623, q321br_lsx, qg311r or b091qp (Allwinner-v2 - default y21ga)\n");
    fprintf(stderr, "\t-r RES,   --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: low, high, both or none (default high)\n");
    fprintf(stderr, "\t-a AUDIO, --audio AUDIO\n");
    fprintf(stderr, "\t\tset audio: yes, no, alaw, ulaw, pcm or aac (default ulaw)\n");
    fprintf(stderr, "\t-b CODEC, --audio_back_channel CODEC\n");
    fprintf(stderr, "\t\tenable audio back channel and set codec: alaw, ulaw or aac\n");
    fprintf(stderr, "\t-p PORT,  --port PORT\n");
    fprintf(stderr, "\t\tset TCP port (default 554)\n");
    fprintf(stderr, "\t-s,       --sti\n");
    fprintf(stderr, "\t\tdon't overwrite SPS timing info (default overwrite)\n");
    fprintf(stderr, "\t-u USER,  --user USER\n");
    fprintf(stderr, "\t\tset username\n");
    fprintf(stderr, "\t-w PASSWORD,  --password PASSWORD\n");
    fprintf(stderr, "\t\tset password\n");
    fprintf(stderr, "\t-d DEBUG, --debug DEBUG\n");
    fprintf(stderr, "\t\t0 none, +1 video in, +2 audio in, +4 video out, +8 audio out\n");
    fprintf(stderr, "\t-h,       --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char** argv)
{
    char *str;
    int nm;
    int back_channel;
    char user[65];
    char pwd[65];
    int pth_ret;
    int c;
    char *endptr;
    unsigned char v;
    Boolean enable_speaker;
    pthread_t capture_thread;

    int convertTo = WA_PCMU;
    char const* inputAudioFileName = "/tmp/audio_fifo";
    char const* outputAudioFileName = "/tmp/audio_in_fifo";
    struct stat stat_buffer;
    FILE *fFS;
    Boolean useTimeForPres;

    // Setting default
    model = Y21GA;
    resolution = RESOLUTION_HIGH;
    audio = 1;
    back_channel = 0;
    port = 554;
    sps_timing_info = 1;
    debug = 0;
    v = 2;
    enable_speaker = False;

    // Autodetect sps/vps type
    stream_type.codec_low = CODEC_NONE;
    stream_type.codec_high = CODEC_NONE;
    stream_type.sps_type_low = 0;
    stream_type.sps_type_high = 0;
    stream_type.vps_type_low = 0;
    stream_type.vps_type_high = 0;

    memset(user, 0, sizeof(user));
    memset(pwd, 0, sizeof(pwd));

    while (1) {
        static struct option long_options[] =
        {
            {"model",  required_argument, 0, 'm'},
            {"resolution",  required_argument, 0, 'r'},
            {"audio",  required_argument, 0, 'a'},
            {"audio_back_channel", required_argument, 0, 'b'},
            {"port",  required_argument, 0, 'p'},
            {"sti",  no_argument, 0, 's'},
            {"user",  required_argument, 0, 'u'},
            {"password",  required_argument, 0, 'w'},
            {"debug",  required_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "m:r:a:b:p:su:w:d:h",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            if (strcasecmp("y20ga", optarg) == 0) {
                model = Y20GA;
                v = 1;
            } else if (strcasecmp("y25ga", optarg) == 0) {
                model = Y25GA;
                v = 1;
            } else if (strcasecmp("y30qa", optarg) == 0) {
                model = Y30QA;
                v = 1;
            } else if (strcasecmp("y501gc", optarg) == 0) {
                model = Y501GC;
                v = 1;
            } else if (strcasecmp("y21ga", optarg) == 0) {
                model = Y21GA;
                v = 2;
            } else if (strcasecmp("y211ga", optarg) == 0) {
                model = Y211GA;
                v = 2;
            } else if (strcasecmp("y211ba", optarg) == 0) {
                model = Y211BA;
                v = 2;
            } else if (strcasecmp("y213ga", optarg) == 0) {
                model = Y213GA;
                v = 2;
            } else if (strcasecmp("y291ga", optarg) == 0) {
                model = Y291GA;
                v = 2;
            } else if (strcasecmp("h30ga", optarg) == 0) {
                model = H30GA;
                v = 2;
            } else if (strcasecmp("r30gb", optarg) == 0) {
                model = R30GB;
                v = 2;
            } else if (strcasecmp("r35gb", optarg) == 0) {
                model = R35GB;
                v = 2;
            } else if (strcasecmp("r37gb", optarg) == 0) {
                model = R37GB;
                v = 2;
            } else if (strcasecmp("r40ga", optarg) == 0) {
                model = R40GA;
                v = 2;
            } else if (strcasecmp("h51ga", optarg) == 0) {
                model = H51GA;
                v = 2;
            } else if (strcasecmp("h52ga", optarg) == 0) {
                model = H52GA;
                v = 2;
            } else if (strcasecmp("h60ga", optarg) == 0) {
                model = H60GA;
                v = 2;
            } else if (strcasecmp("y28ga", optarg) == 0) {
                model = Y28GA;
                v = 2;
            } else if (strcasecmp("y29ga", optarg) == 0) {
                model = Y29GA;
                v = 2;
            } else if (strcasecmp("y623", optarg) == 0) {
                model = Y623;
                v = 2;
            } else if (strcasecmp("q321br_lsx", optarg) == 0) {
                model = Q321BR_LSX;
                v = 2;
            } else if (strcasecmp("qg311r", optarg) == 0) {
                model = QG311R;
                v = 2;
            } else if (strcasecmp("b091qp", optarg) == 0) {
                model = B091QP;
                v = 2;
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
            if (strcasecmp("no", optarg) == 0) {
                audio = 0;
            } else if (strcasecmp("yes", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCMU;
            } else if (strcasecmp("alaw", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCMA;
            } else if (strcasecmp("ulaw", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCMU;
            } else if (strcasecmp("pcm", optarg) == 0) {
                audio = 1;
                convertTo = WA_PCM;
            } else if (strcasecmp("aac", optarg) == 0) {
                audio = 2;
            }
            break;

        case 'b':
            if (strcasecmp("alaw", optarg) == 0) {
                back_channel = 1;
            } else if (strcasecmp("ulaw", optarg) == 0) {
                back_channel = 2;
            } else if (strcasecmp("aac", optarg) == 0) {
                back_channel = 4;
            } else {
                back_channel = 0;
            }
            break;

        case 'p':
            errno = 0;    /* To distinguish success/failure after call */
            port = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (port == LONG_MAX || port == LONG_MIN)) || (errno != 0 && port == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 's':
            sps_timing_info = 0;
            break;

        case 'u':
            if (strlen(optarg) < sizeof(user)) {
                strcpy(user, optarg);
            }
            break;

        case 'w':
            if (strlen(optarg) < sizeof(pwd)) {
                strcpy(pwd, optarg);
            }
            break;

        case 'd':
            errno = 0;    /* To distinguish success/failure after call */
            debug = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (debug == LONG_MAX || debug == LONG_MIN)) || (errno != 0 && debug == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((debug < 0) || (debug > 15)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
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

    // Get parameters from environment
    str = getenv("RRTSP_MODEL");
    if (str != NULL) {
        if (strcasecmp("y20ga", str) == 0) {
            model = Y20GA;
            v = 1;
        } else if (strcasecmp("y25ga", str) == 0) {
            model = Y25GA;
            v = 1;
        } else if (strcasecmp("y30qa", str) == 0) {
            model = Y30QA;
            v = 1;
        } else if (strcasecmp("y501gc", str) == 0) {
            model = Y501GC;
            v = 1;
        } else if (strcasecmp("y21ga", str) == 0) {
            model = Y21GA;
            v = 2;
        } else if (strcasecmp("y211ga", str) == 0) {
            model = Y211GA;
            v = 2;
        } else if (strcasecmp("y211ba", str) == 0) {
            model = Y211BA;
            v = 2;
        } else if (strcasecmp("y213ga", str) == 0) {
            model = Y213GA;
            v = 2;
        } else if (strcasecmp("y291ga", str) == 0) {
            model = Y291GA;
            v = 2;
        } else if (strcasecmp("h30ga", str) == 0) {
            model = H30GA;
            v = 2;
        } else if (strcasecmp("r30gb", str) == 0) {
            model = R30GB;
            v = 2;
        } else if (strcasecmp("r35gb", str) == 0) {
            model = R35GB;
            v = 2;
        } else if (strcasecmp("r37gb", str) == 0) {
            model = R37GB;
            v = 2;
        } else if (strcasecmp("r40ga", str) == 0) {
            model = R40GA;
            v = 2;
        } else if (strcasecmp("h51ga", str) == 0) {
            model = H51GA;
            v = 2;
        } else if (strcasecmp("h52ga", str) == 0) {
            model = H52GA;
            v = 2;
        } else if (strcasecmp("h60ga", str) == 0) {
            model = H60GA;
            v = 2;
        } else if (strcasecmp("y28ga", str) == 0) {
            model = Y28GA;
            v = 2;
        } else if (strcasecmp("y29ga", str) == 0) {
            model = Y29GA;
            v = 2;
        } else if (strcasecmp("y623", str) == 0) {
            model = Y623;
            v = 2;
        } else if (strcasecmp("q321br_lsx", str) == 0) {
            model = Q321BR_LSX;
            v = 2;
        } else if (strcasecmp("qg311r", str) == 0) {
            model = QG311R;
            v = 2;
        } else if (strcasecmp("b091qp", str) == 0) {
            model = B091QP;
            v = 2;
        }
    }

    str = getenv("RRTSP_RES");
    if (str != NULL) {
        if (strcasecmp("low", str) == 0) {
            resolution = RESOLUTION_LOW;
        } else if (strcasecmp("high", str) == 0) {
            resolution = RESOLUTION_HIGH;
        } else if (strcasecmp("both", str) == 0) {
            resolution = RESOLUTION_BOTH;
        } else if (strcasecmp("none", str) == 0) {
            resolution = RESOLUTION_NONE;
        }
    }

    str = getenv("RRTSP_AUDIO");
    if (str != NULL) {
        if (strcasecmp("no", str) == 0) {
            audio = 0;
        } else if (strcasecmp("yes", str) == 0) {
            audio = 1;
            convertTo = WA_PCMU;
        } else if (strcasecmp("alaw", str) == 0) {
            audio = 1;
            convertTo = WA_PCMA;
        } else if (strcasecmp("ulaw", str) == 0) {
            audio = 1;
            convertTo = WA_PCMU;
        } else if (strcasecmp("pcm", str) == 0) {
            audio = 1;
            convertTo = WA_PCM;
        } else if (strcasecmp("aac", str) == 0) {
            audio = 2;
        }
    }

    str = getenv("RRTSP_AUDIO_BC");
    if (str != NULL) {
        if (strcasecmp("alaw", str) == 0) {
            back_channel = 1;
        } else if (strcasecmp("ulaw", str) == 0) {
            back_channel = 2;
        } else if (strcasecmp("aac", str) == 0) {
            back_channel = 4;
        } else {
            back_channel = 0;
        }
    }

    str = getenv("RRTSP_PORT");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0)) {
        port = nm;
    }

    str = getenv("RRTSP_STI");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0) && (nm <= 1)) {
        sps_timing_info = nm;
    }

    str = getenv("RRTSP_DEBUG");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0)) {
        debug = nm;
    }

    str = getenv("RRTSP_USER");
    if ((str != NULL) && (strlen(str) < sizeof(user))) {
        strcpy(user, str);
    }

    str = getenv("RRTSP_PWD");
    if ((str != NULL) && (strlen(str) < sizeof(pwd))) {
        strcpy(pwd, str);
    }

    fFS = fopen(BUFFER_FILE, "r");
    if ( fFS == NULL ) {
        fprintf(stderr, "could not get size of %s\n", BUFFER_FILE);
        exit(EXIT_FAILURE);
    }
    fseek(fFS, 0, SEEK_END);
    buf_size = ftell(fFS);
    fclose(fFS);
    if (debug) fprintf(stderr, "%lld: the size of the buffer is %d\n",
            current_timestamp(), buf_size);

    if  (v == 2) {
        enable_speaker = True;
    }

    if (model == Y20GA) {
        buf_offset = BUF_OFFSET_Y20GA;
        frame_header_size = FRAME_HEADER_SIZE_Y20GA;
    } else if (model == Y25GA) {
        buf_offset = BUF_OFFSET_Y25GA;
        frame_header_size = FRAME_HEADER_SIZE_Y25GA;
    } else if (model == Y30QA) {
        buf_offset = BUF_OFFSET_Y30QA;
        frame_header_size = FRAME_HEADER_SIZE_Y30QA;
    } else if (model == Y501GC) {
        buf_offset = BUF_OFFSET_Y501GC;
        frame_header_size = FRAME_HEADER_SIZE_Y501GC;
    } else if (model == Y21GA) {
        buf_offset = BUF_OFFSET_Y21GA;
        frame_header_size = FRAME_HEADER_SIZE_Y21GA;
    } else if (model == Y211GA) {
        buf_offset = BUF_OFFSET_Y211GA;
        frame_header_size = FRAME_HEADER_SIZE_Y211GA;
    } else if (model == Y211BA) {
        buf_offset = BUF_OFFSET_Y211BA;
        frame_header_size = FRAME_HEADER_SIZE_Y211BA;
    } else if (model == Y213GA) {
        buf_offset = BUF_OFFSET_Y213GA;
        frame_header_size = FRAME_HEADER_SIZE_Y213GA;
    } else if (model == Y291GA) {
        buf_offset = BUF_OFFSET_Y291GA;
        frame_header_size = FRAME_HEADER_SIZE_Y291GA;
    } else if (model == H30GA) {
        buf_offset = BUF_OFFSET_H30GA;
        frame_header_size = FRAME_HEADER_SIZE_H30GA;
    } else if (model == R30GB) {
        buf_offset = BUF_OFFSET_R30GB;
        frame_header_size = FRAME_HEADER_SIZE_R30GB;
    } else if (model == R35GB) {
        buf_offset = BUF_OFFSET_R35GB;
        frame_header_size = FRAME_HEADER_SIZE_R35GB;
    } else if (model == R37GB) {
        buf_offset = BUF_OFFSET_R37GB;
        frame_header_size = FRAME_HEADER_SIZE_R37GB;
    } else if (model == R40GA) {
        buf_offset = BUF_OFFSET_R40GA;
        frame_header_size = FRAME_HEADER_SIZE_R40GA;
    } else if (model == H51GA) {
        buf_offset = BUF_OFFSET_H51GA;
        frame_header_size = FRAME_HEADER_SIZE_H51GA;
    } else if (model == H52GA) {
        buf_offset = BUF_OFFSET_H52GA;
        frame_header_size = FRAME_HEADER_SIZE_H52GA;
    } else if (model == H60GA) {
        buf_offset = BUF_OFFSET_H60GA;
        frame_header_size = FRAME_HEADER_SIZE_H60GA;
    } else if (model == Y28GA) {
        buf_offset = BUF_OFFSET_Y28GA;
        frame_header_size = FRAME_HEADER_SIZE_Y28GA;
    } else if (model == Y29GA) {
        buf_offset = BUF_OFFSET_Y29GA;
        frame_header_size = FRAME_HEADER_SIZE_Y29GA;
    } else if (model == Y623) {
        buf_offset = BUF_OFFSET_Y623;
        frame_header_size = FRAME_HEADER_SIZE_Y623;
    } else if (model == Q321BR_LSX) {
        buf_offset = BUF_OFFSET_Q321BR_LSX;
        frame_header_size = FRAME_HEADER_SIZE_Q321BR_LSX;
    } else if (model == QG311R) {
        buf_offset = BUF_OFFSET_QG311R;
        frame_header_size = FRAME_HEADER_SIZE_QG311R;
    } else if (model == B091QP) {
        buf_offset = BUF_OFFSET_B091QP;
        frame_header_size = FRAME_HEADER_SIZE_B091QP;
    }

    // If fifo doesn't exist, disable audio
    if ((audio == 1) && (stat (inputAudioFileName, &stat_buffer) != 0)) {
        fprintf(stderr, "unable to find %s, audio disabled\n", inputAudioFileName);
        audio = 0;
    }

    setpriority(PRIO_PROCESS, 0, -10);

    // Fill input and output buffer struct
    strcpy(input_buffer.filename, BUFFER_SHM);
    input_buffer.size = buf_size;
    input_buffer.offset = buf_offset;

    // Low res
    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
        output_queue_low.type = TYPE_LOW;
    }

    // High res
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
        output_queue_high.type = TYPE_HIGH;
    }

    // Audio
    if (audio == 0) {
        // Just video, use timestamps from video samples
        useTimeForPres = False;
        output_queue_audio.type = TYPE_NONE;
    } else if (audio == 1) {
        // We don't have timestamps for PCM audio samples when the source is the fifo queue: use current time
        useTimeForPres = True;
        output_queue_audio.type = TYPE_NONE;
    } else if (audio == 2) {
        // AAC audio and H26x video, use timestamps from audio/video samples
        useTimeForPres = False;
        output_queue_audio.type = TYPE_AAC;
    }

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    // Init mutexes
    if (pthread_mutex_init(&(output_queue_low.mutex), NULL) != 0) {
        fprintf(stderr, "Failed to create mutex\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&(output_queue_high.mutex), NULL) != 0) {
        fprintf(stderr, "Failed to create mutex\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&(output_queue_audio.mutex), NULL) != 0) {
        fprintf(stderr, "Failed to create mutex\n");
        exit(EXIT_FAILURE);
    }

    // Start capture thread
    pth_ret = pthread_create(&capture_thread, NULL, capture, (void*) NULL);
    if (pth_ret != 0) {
        fprintf(stderr, "Failed to create capture thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(capture_thread);

    sleep(2);

    // Wait for stream type autodetect
    while (1) {
        if ((stream_type.codec_low != CODEC_NONE) && (stream_type.codec_high != CODEC_NONE)) {
            usleep(10000);
            break;
        }
        usleep(10000);
    }

    if (debug) {
        fprintf(stderr, "Stream detected: high res is %s, low res is %s\n",
                (stream_type.codec_high==CODEC_H264)?"h264":"h265",
                (stream_type.codec_low==CODEC_H264)?"h264":"h265");
    }

    UserAuthenticationDatabase* authDB = NULL;

    if ((user[0] != '\0') && (pwd[0] != '\0')) {
        // To implement client access control to the RTSP server, do the following:
        authDB = new UserAuthenticationDatabase;
        authDB->addUserRecord(user, pwd);
        // Repeat the above with each <username>, <password> that you wish to allow
        // access to the server.
    }

    // Create the RTSP server:
    RTSPServer* rtspServer = RTSPServer::createNew(*env, port, authDB);
    if (rtspServer == NULL) {
        fprintf(stderr, "Failed to create RTSP server: %s\n", env->getResultMsg());
        exit(1);
    }

    StreamReplicator* replicator = NULL;
    if (audio == 1) {
        if (debug) fprintf(stderr, "Starting pcm replicator\n");
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(inputAudioFileName, 8000, 1, 16, convertTo);
    } else if (audio == 2) {
        if (debug) fprintf(stderr, "Starting aac replicator\n");
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(&output_queue_audio, 16000, 1, useTimeForPres);
    }

    char const* descriptionString = "Session streamed by \"rRTSPServer\"";

    // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
    OutPacketBuffer::maxSize = OUTPUT_BUFFER_SIZE_HIGH;

    // Set up each of the possible streams that can be served by the
    // RTSP server.  Each such stream is implemented using a
    // "ServerMediaSession" object, plus one or more
    // "ServerMediaSubsession" objects for each audio/video substream.

    // A H.264/5 video elementary stream:
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_0.h264";

        ServerMediaSession* sms_high
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        if (stream_type.codec_high == CODEC_H264) {
            sms_high->addSubsession(H264VideoFramedMemoryServerMediaSubsession
                                   ::createNew(*env, &output_queue_high, useTimeForPres, reuseFirstSource));
        } else if (stream_type.codec_high == CODEC_H265) {
            sms_high->addSubsession(H265VideoFramedMemoryServerMediaSubsession
                                   ::createNew(*env, &output_queue_high, useTimeForPres, reuseFirstSource));
        }
        if (audio == 1) {
            sms_high->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 8000, 1, 16, convertTo));
        } else if (audio == 2) {
            sms_high->addSubsession(ADTSAudioFramedMemoryServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 16000, 1));
        }
        if (back_channel == 1) {
            PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                    ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ALAW, enable_speaker);
            sms_high->addSubsession(smss_bc);
        } else if (back_channel == 2) {
            PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                    ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ULAW, enable_speaker);
            sms_high->addSubsession(smss_bc);
        } else if (back_channel == 4) {
            ADTSAudioFileServerMediaSubsession_BC* smss_bc = ADTSAudioFileServerMediaSubsession_BC
                    ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, enable_speaker);
            sms_high->addSubsession(smss_bc);
        }
        rtspServer->addServerMediaSession(sms_high);

        announceStream(rtspServer, sms_high, streamName, audio);
    }

    // A H.264 video elementary stream:
    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_1.h264";

        ServerMediaSession* sms_low
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        if (stream_type.codec_low == CODEC_H264) {
            sms_low->addSubsession(H264VideoFramedMemoryServerMediaSubsession
                                       ::createNew(*env, &output_queue_low, useTimeForPres, reuseFirstSource));
        } else if (stream_type.codec_low == CODEC_H265) {
            sms_low->addSubsession(H265VideoFramedMemoryServerMediaSubsession
                                       ::createNew(*env, &output_queue_low, useTimeForPres, reuseFirstSource));
        }
        if (audio == 1) {
            sms_low->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 8000, 1, 16, convertTo));
        } else if (audio == 2) {
            sms_low->addSubsession(ADTSAudioFramedMemoryServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 16000, 1));
        }
        if (resolution == RESOLUTION_LOW) {
            if (back_channel == 1) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ALAW, enable_speaker);
                sms_low->addSubsession(smss_bc);
            } else if (back_channel == 2) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ULAW, enable_speaker);
                sms_low->addSubsession(smss_bc);
            } else if (back_channel == 4) {
                ADTSAudioFileServerMediaSubsession_BC* smss_bc = ADTSAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, enable_speaker);
                sms_low->addSubsession(smss_bc);
            }
        }
        rtspServer->addServerMediaSession(sms_low);

        announceStream(rtspServer, sms_low, streamName, audio);
    }

    // A PCM audio elementary stream:
    if (audio != 0)
    {
        char const* streamName = "ch0_2.h264";

        ServerMediaSession* sms_audio
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        if (audio == 1) {
            sms_audio->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 8000, 1, 16, convertTo));
        } else if (audio == 2) {
            sms_audio->addSubsession(ADTSAudioFramedMemoryServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, 16000, 1));
        }
        if (resolution == RESOLUTION_NONE) {
            if (back_channel == 1) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ALAW, enable_speaker);
                sms_audio->addSubsession(smss_bc);
            } else if (back_channel == 2) {
                PCMAudioFileServerMediaSubsession_BC* smss_bc = PCMAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, ULAW, enable_speaker);
                sms_audio->addSubsession(smss_bc);
            } else if (back_channel == 4) {
                ADTSAudioFileServerMediaSubsession_BC* smss_bc = ADTSAudioFileServerMediaSubsession_BC
                        ::createNew(*env, outputAudioFileName, reuseFirstSource, 16000, 1, enable_speaker);
                sms_audio->addSubsession(smss_bc);
            }
        }
        rtspServer->addServerMediaSession(sms_audio);

        announceStream(rtspServer, sms_audio, streamName, audio);
    }

    env->taskScheduler().doEventLoop(); // does not return

    pthread_mutex_destroy(&(output_queue_low.mutex));
    pthread_mutex_destroy(&(output_queue_high.mutex));
    pthread_mutex_destroy(&(output_queue_audio.mutex));

    return 0; // only to prevent compiler warning
}
