/*
 * Copyright (c) 2022 roleo.
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

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "H264VideoFramedMemoryServerMediaSubsession.hh"
#include "H265VideoFramedMemoryServerMediaSubsession.hh"
#include "ADTSAudioFramedMemoryServerMediaSubsession.hh"
#include "WAVAudioFifoServerMediaSubsession.hh"
#include "WAVAudioFifoSource.hh"
#include "AudioFramedMemorySource.hh"
#include "StreamReplicator.hh"
#include "DummySink.hh"
#include "aLawAudioFilter.hh"

#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
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
unsigned char SPS5_1920X1080[]      = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x7B, 0xA0,
                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x08};
unsigned char SPS5_1920X1080_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x7B, 0xA0,
                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x40,
                                       0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x05, 0x02};
unsigned char SPS5_2_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xBA, 0xA0,
                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x08};
unsigned char SPS5_2_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
                                       0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xBA, 0xA0,
                                       0x03, 0xC0, 0x80, 0x10, 0xE7, 0xF9, 0x6B, 0xB9,
                                       0x12, 0x20, 0xB2, 0xFC, 0xF3, 0xCF, 0x3C, 0xF3,
                                       0xCF, 0x3C, 0xF3, 0xCF, 0x3C, 0xF3, 0xCF, 0x3C,
                                       0xF3, 0xCB, 0x73, 0x70, 0x10, 0x10, 0x10, 0x40,
                                       0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x05, 0x02};
unsigned char VPS5_1920X1080[]      = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0x7B, 0xAC, 0x09};
unsigned char VPS5_1920X1080_TI[]   = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0x7B, 0xAC, 0x0C, 0x00, 0x00, 0x00, 0x40,
                                       0x00, 0x00, 0x00, 0x05, 0x40};
unsigned char VPS5_2_1920X1080[]    = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0xBA, 0xAC, 0x09};
unsigned char VPS5_2_1920X1080_TI[] = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01,
                                       0xFF, 0xFF, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
                                       0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                       0x00, 0xBA, 0xAC, 0x0C, 0x00, 0x00, 0x0F, 0xA4,
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
cb_output_buffer output_buffer_low;
cb_output_buffer output_buffer_high;
cb_output_buffer output_buffer_audio;

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

void s2cb_memcpy(cb_output_buffer *dest, unsigned char *src, size_t n)
{
    unsigned char *uc_dest = dest->write_index;

    if (uc_dest + n > dest->buffer + dest->size) {
        memcpy(uc_dest, src, dest->buffer + dest->size - uc_dest);
        memcpy(dest->buffer, src + (dest->buffer + dest->size - uc_dest), n - (dest->buffer + dest->size - uc_dest));
        dest->write_index = n + uc_dest - dest->size;
    } else {
        memcpy(uc_dest, src, n);
        dest->write_index += n;
    }
    if (dest->write_index == dest->buffer + dest->size) {
        dest->write_index = dest->buffer;
    }
}

void cb2cb_memcpy(cb_output_buffer *dest, cb_input_buffer *src, size_t n)
{
    unsigned char *uc_src = src->read_index;

    if (uc_src + n > src->buffer + src->size) {
        s2cb_memcpy(dest, uc_src, src->buffer + src->size - uc_src);
        s2cb_memcpy(dest, src->buffer + src->offset, n - (src->buffer + src->size - uc_src));
        src->read_index = src->offset + n + uc_src - src->size;
    } else {
        s2cb_memcpy(dest, uc_src, n);
        src->read_index += n;
    }
}

/* Locate a string in the circular buffer */
unsigned char *cb_memmem(unsigned char *src, int src_len, unsigned char *what, int what_len)
{
    unsigned char *p;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, input_buffer.buffer + input_buffer.size - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(input_buffer.buffer + input_buffer.offset, src + src_len - (input_buffer.buffer + input_buffer.offset), what, what_len);
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
        ret = memcmp(str1, str2, input_buffer.buffer + input_buffer.size - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (input_buffer.buffer + input_buffer.size - str2), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

// The second argument is the circular buffer
void cb2s_memcpy(unsigned char *dest, unsigned char *src, size_t n)
{
    if (src + n > input_buffer.buffer + input_buffer.size) {
        memcpy(dest, src, input_buffer.buffer + input_buffer.size - src);
        memcpy(dest + (input_buffer.buffer + input_buffer.size - src), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - src));
    } else {
        memcpy(dest, src, n);
    }
}

// The second argument is the circular buffer
void cb2s_headercpy(unsigned char *dest, unsigned char *src, size_t n)
{
    struct frame_header *fh = (struct frame_header *) dest;
    struct frame_header_22 fh22;
    struct frame_header_26 fh26;
    struct frame_header_28 fh28;
    unsigned char *fp = NULL;

    if (n == sizeof(fh22)) {
        fp = (unsigned char *) &fh22;
    } else if (n == sizeof(fh26)) {
        fp = (unsigned char *) &fh26;
    } else if (n == sizeof(fh28)) {
        fp = (unsigned char *) &fh28;
    }
    if (fp == NULL) return;

    if (src + n > input_buffer.buffer + input_buffer.size) {
        memcpy(fp, src, input_buffer.buffer + input_buffer.size - src);
        memcpy(fp + (input_buffer.buffer + input_buffer.size - src), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - src));
    } else {
        memcpy(fp, src, n);
    }
    if (n == sizeof(fh22)) {
        fh->len = fh22.len;
        fh->counter = fh22.counter;
        fh->time = fh22.time;
        fh->type = fh22.type;
        fh->stream_counter = fh22.stream_counter;
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
    int fshm;

    int frame_type = TYPE_NONE;
    int frame_len = 0;
    int frame_counter = -1;
    int frame_counter_last_valid_low = -1;
    int frame_counter_last_valid_high = -1;
    int frame_counter_last_valid_audio = -1;

    int i, n;
    cb_output_buffer *cb_current;
    int write_enable = 0;
    int frame_sync = 0;

    struct frame_header fhs[10];
    unsigned char* fhs_addr[10];
    uint32_t last_counter;

#ifdef USE_SEMAPHORE
    if (sem_fshare_open() != 0) {
        fprintf(stderr, "error - could not open semaphores\n") ;
        free(output_buffer_low.buffer);
        free(output_buffer_high.buffer);
        exit(EXIT_FAILURE);
    }
#endif

    // Opening an existing file
    fshm = shm_open(input_buffer.filename, O_RDWR, 0);
    if (fshm == -1) {
        fprintf(stderr, "error - could not open file %s\n", input_buffer.filename) ;
        free(output_buffer_low.buffer);
        free(output_buffer_high.buffer);
        exit(EXIT_FAILURE);
    }

    // Map file to memory
    input_buffer.buffer = (unsigned char*) mmap(NULL, input_buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fshm, 0);
    if (input_buffer.buffer == MAP_FAILED) {
        fprintf(stderr, "%lld: error - mapping file %s\n", current_timestamp(), input_buffer.filename);
        close(fshm);
        free(output_buffer_low.buffer);
        free(output_buffer_high.buffer);
        exit(EXIT_FAILURE);
    }
    if (debug & 1) fprintf(stderr, "%lld: mapping file %s, size %d, to %08x\n", current_timestamp(), input_buffer.filename, input_buffer.size, (unsigned int) input_buffer.buffer);

    // Closing the file
    if (debug & 1) fprintf(stderr, "%lld: closing the file %s\n", current_timestamp(), input_buffer.filename);
    close(fshm) ;

#ifdef USE_SEMAPHORE
    sem_write_lock();
#endif
    memcpy(&i, input_buffer.buffer + 16, sizeof(i));
    buf_idx = input_buffer.buffer + input_buffer.offset + i;
    buf_idx_cur = buf_idx;
    memcpy(&i, input_buffer.buffer + 4, sizeof(i));
    buf_idx_end = buf_idx + i;
    if (buf_idx_end >= input_buffer.buffer + input_buffer.size) buf_idx_end -= (input_buffer.size - input_buffer.offset);
    buf_idx_end_prev = buf_idx_end;
#ifdef USE_SEMAPHORE
    sem_write_unlock();
#endif
    last_counter = 0;

    if (debug & 1) fprintf(stderr, "%lld: starting capture main loop\n", current_timestamp());

    // Infinite loop
    while (1) {
#ifdef USE_SEMAPHORE
        sem_write_lock();
#endif
        memcpy(&i, input_buffer.buffer + 16, sizeof(i));
        buf_idx = input_buffer.buffer + input_buffer.offset + i;
        memcpy(&i, input_buffer.buffer + 4, sizeof(i));
        buf_idx_end = buf_idx + i;
        if (buf_idx_end >= input_buffer.buffer + input_buffer.size) buf_idx_end -= (input_buffer.size - input_buffer.offset);
        // Check if the header is ok
        memcpy(&i, input_buffer.buffer + 12, sizeof(i));
        if (buf_idx_end != input_buffer.buffer + input_buffer.offset + i) {
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
            if (fhs[i].len > input_buffer.size - input_buffer.offset) {
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
                    }
                    if ((debug & 1) && (stream_type.codec_low != CODEC_NONE)) fprintf(stderr, "%lld: low - codec type is %d - sps type is %d\n",
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
                    }
                    if ((debug & 1) && (stream_type.codec_high != CODEC_NONE)) fprintf(stderr, "%lld: high - codec type is %d - sps type is %d\n",
                            current_timestamp(), stream_type.codec_high, stream_type.sps_type_high);
                }
            } else if (fhs[i].type & 0x0008) {
                buf_idx_cur = cb_move(buf_idx_cur, frame_header_size);
                if ((stream_type.codec_low  == CODEC_NONE) && (fhs[i].type & 0x0800)) {
                    if (cb_memcmp(VPS5_START, buf_idx_cur, sizeof(VPS5_START)) == 0) {
                        stream_type.codec_low = CODEC_H265;
                        stream_type.vps_type_low = 0x0101;
                    }
                    if ((debug & 1) && (stream_type.codec_low != CODEC_NONE)) fprintf(stderr, "%lld: low - codec type is %d - vps type is %d\n",
                            current_timestamp(), stream_type.codec_low, stream_type.vps_type_low);
                } else if ((stream_type.codec_high  == CODEC_NONE) && (fhs[i].type & 0x0400)) {
                    if (cb_memcmp(VPS5_1920X1080, buf_idx_cur, sizeof(VPS5_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H265;
                        stream_type.vps_type_high = 0x0102;
                    } else if (cb_memcmp(VPS5_2_1920X1080, buf_idx_cur, sizeof(VPS5_2_1920X1080)) == 0) {
                        stream_type.codec_high = CODEC_H265;
                        stream_type.vps_type_high = 0x0202;
                    }
                    if ((debug & 1) && (stream_type.codec_high != CODEC_NONE)) fprintf(stderr, "%lld: high - codec type is %d - vps type is %d\n",
                            current_timestamp(), stream_type.codec_high, stream_type.vps_type_high);
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
            if ((frame_type == TYPE_LOW) && (resolution != RESOLUTION_HIGH)) {
                if ((65536 + frame_counter - frame_counter_last_valid_low) % 65536 > 1) {

                    if (debug & 1) fprintf(stderr, "%lld: warning - %d low res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_low - 1) % 65536, frame_counter, frame_counter_last_valid_low);
                    frame_counter_last_valid_low = frame_counter;
                } else {
                    if (debug & 1) {
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
            } else if ((frame_type == TYPE_HIGH) && (resolution != RESOLUTION_LOW)) {
                if ((65536 + frame_counter - frame_counter_last_valid_high) % 65536 > 1) {

                    if (debug & 1) fprintf(stderr, "%lld: warning - %d high res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_high - 1) % 65536, frame_counter, frame_counter_last_valid_high);
                    frame_counter_last_valid_high = frame_counter;
                } else {
                    if (debug & 1) {
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
            } else if (frame_type == TYPE_AAC) {
                if ((65536 + frame_counter - frame_counter_last_valid_audio) % 65536 > 1) {
                    if (debug & 1) fprintf(stderr, "%lld: warning - %d AAC frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                                current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_audio - 1) % 65536, frame_counter, frame_counter_last_valid_audio);
                    frame_counter_last_valid_audio = frame_counter;
                } else {
                    if ((audio == 2) && (debug & 1)) fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - audio AAC\n",
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
                    cb_current = &output_buffer_low;
                } else if ((frame_type == TYPE_HIGH) && (resolution != RESOLUTION_LOW) && (stream_type.codec_high != CODEC_NONE)) {
                    cb_current = &output_buffer_high;
                } else if (frame_type == TYPE_AAC) {
                    cb_current = &output_buffer_audio;
                } else {
                    cb_current = NULL;
                }

                if (cb_current != NULL) {
                    if (debug & 1) fprintf(stderr, "%lld: frame_len: %d - cb_current->size: %d\n", current_timestamp(), frame_len, cb_current->size);
                    if (frame_len > (signed) cb_current->size) {
                        fprintf(stderr, "%lld: error - frame size exceeds buffer size\n", current_timestamp());
                    } else {
                        pthread_mutex_lock(&(cb_current->mutex));
                        input_buffer.read_index = buf_idx_start;

                        cb_current->output_frame[cb_current->frame_write_index].ptr = cb_current->write_index;
                        cb_current->output_frame[cb_current->frame_write_index].counter = frame_counter;

                        if (sps_timing_info) {
                            // Overwrite SPS or VPS with one that contains timing info at 20 fps
                            if (fhs[i].type & 0x0002) {
                                if (frame_type == TYPE_LOW) {
                                    if (stream_type.sps_type_low & 0x0101) {
                                        frame_len = sizeof(SPS4_640X360_TI);
                                        s2cb_memcpy(cb_current, SPS4_640X360_TI, sizeof(SPS4_640X360_TI));
                                    } else if (stream_type.sps_type_low & 0x0201) {
                                        frame_len = sizeof(SPS4_2_640X360_TI);
                                        s2cb_memcpy(cb_current, SPS4_2_640X360_TI, sizeof(SPS4_2_640X360_TI));
                                    } else if (stream_type.sps_type_low & 0x0401) {
                                        frame_len = sizeof(SPS4_3_640X360_TI);
                                        s2cb_memcpy(cb_current, SPS4_3_640X360_TI, sizeof(SPS4_3_640X360_TI));
                                    } else {
                                        // don't change frame_len
                                        cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                                    }
                                } else if (frame_type == TYPE_HIGH) {
                                    if (stream_type.sps_type_high == 0x0102) {
                                        frame_len = sizeof(SPS4_1920X1080_TI);
                                        s2cb_memcpy(cb_current, SPS4_1920X1080_TI, sizeof(SPS4_1920X1080_TI));
                                    } else if (stream_type.sps_type_high == 0x0202) {
                                        frame_len = sizeof(SPS4_2_1920X1080_TI);
                                        s2cb_memcpy(cb_current, SPS4_2_1920X1080_TI, sizeof(SPS4_2_1920X1080_TI));
                                    } else if (stream_type.sps_type_high == 0x0402) {
                                        frame_len = sizeof(SPS4_3_1920X1080_TI);
                                        s2cb_memcpy(cb_current, SPS4_3_1920X1080_TI, sizeof(SPS4_3_1920X1080_TI));
                                    } else if (stream_type.sps_type_high == 0x0103) {
                                        frame_len = sizeof(SPS4_2304X1296_TI);
                                        s2cb_memcpy(cb_current, SPS4_2304X1296_TI, sizeof(SPS4_2304X1296_TI));
                                    } else if (stream_type.sps_type_high == 0x0203) {
                                        frame_len = sizeof(SPS4_2_2304X1296_TI);
                                        s2cb_memcpy(cb_current, SPS4_2_2304X1296_TI, sizeof(SPS4_2_2304X1296_TI));
                                    } else if (stream_type.vps_type_high & 0x0102) {
                                        frame_len = sizeof(SPS5_1920X1080_TI);
                                        s2cb_memcpy(cb_current, SPS5_1920X1080_TI, sizeof(SPS5_1920X1080_TI));
                                    } else if (stream_type.vps_type_high & 0x0202) {
                                        frame_len = sizeof(SPS5_2_1920X1080_TI);
                                        s2cb_memcpy(cb_current, SPS5_2_1920X1080_TI, sizeof(SPS5_2_1920X1080_TI));
                                    } else {
                                        // don't change frame_len
                                        cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                                    }
                                } else {
                                    // don't change frame_len
                                    cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                                }
                            } else if (fhs[i].type & 0x0008) {
                                if (frame_type == TYPE_LOW) {
                                    // don't change frame_len
                                    cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                                } else if (frame_type == TYPE_HIGH) {
                                    if (stream_type.vps_type_high == 0x0102) {
                                        frame_len = sizeof(VPS5_1920X1080_TI);
                                        s2cb_memcpy(cb_current, VPS5_1920X1080_TI, sizeof(VPS5_1920X1080_TI));
                                    } else if (stream_type.vps_type_high == 0x0202) {
                                        frame_len = sizeof(VPS5_2_1920X1080_TI);
                                        s2cb_memcpy(cb_current, VPS5_2_1920X1080_TI, sizeof(VPS5_2_1920X1080_TI));
                                    } else {
                                        // don't change frame_len
                                        cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                                    }
                                } else {
                                    // don't change frame_len
                                    cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                                }
                            } else {
                                // don't change frame_len
                                cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                            }
                        } else {
                            cb2cb_memcpy(cb_current, &input_buffer, frame_len);
                        }

                        cb_current->output_frame[cb_current->frame_write_index].size = frame_len;
                        if (debug & 1) {
                            fprintf(stderr, "%lld: frame_len: %d - frame_counter: %d - resolution: %d\n", current_timestamp(), frame_len, frame_counter, frame_type);
                            fprintf(stderr, "%lld: frame_write_index: %d/%d\n", current_timestamp(), cb_current->frame_write_index, cb_current->output_frame_size);
                        }
                        cb_current->frame_write_index = (cb_current->frame_write_index + 1) % cb_current->output_frame_size;
                        pthread_mutex_unlock(&(cb_current->mutex));
                    }
                }
            }
        }

        usleep(25000);
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(input_buffer.buffer, input_buffer.size) == -1) {
        fprintf(stderr, "%lld: error - unmapping file\n", current_timestamp());
    } else {
        if (debug & 1) fprintf(stderr, "%lld: unmapping file %s, size %d, from %08x\n", current_timestamp(), input_buffer.filename, input_buffer.size, (unsigned int) input_buffer.buffer);
    }

#ifdef USE_SEMAPHORE
    sem_fshare_close();
#endif

    return NULL;
}

StreamReplicator* startReplicatorStream(const char* inputAudioFileName, int convertTo) {
    FramedSource* resultSource;

    if ((convertTo == WA_PCMA) || (convertTo == WA_PCMU) || (convertTo == WA_PCM)) {
        // Create a single WAVAudioFifo source that will be replicated for mutliple streams
        WAVAudioFifoSource* wavSource = WAVAudioFifoSource::createNew(*env, inputAudioFileName);
        if (wavSource == NULL) {
            *env << "Failed to create Fifo Source \n";
        }

        // Optionally convert to uLaw or aLaw pcm
        if (convertTo == WA_PCMA) {
            resultSource = aLawFromPCMAudioSource::createNew(*env, wavSource, 1/*little-endian*/);
        } else if (convertTo == WA_PCMU) {
            resultSource = uLawFromPCMAudioSource::createNew(*env, wavSource, 1/*little-endian*/);
        } else {
            resultSource = EndianSwap16::createNew(*env, wavSource);
        }
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, resultSource);

    // Begin by creating an input stream from our replicator:
    FramedSource* source = replicator->createStreamReplica();

    // Then create a 'dummy sink' object to receive the replica stream:
    MediaSink* sink = DummySink::createNew(*env, "dummy");

    // Now, start playing, feeding the sink object from the source:
    sink->startPlaying(*source, NULL, NULL);

    return replicator;
}

StreamReplicator* startReplicatorStream(cb_output_buffer *cbBuffer, unsigned samplingFrequency, unsigned numChannels) {
    // Create a single ADTSFromWAVAudioFifo source that will be replicated for mutliple streams
    AudioFramedMemorySource* adtsSource = AudioFramedMemorySource::createNew(*env, cbBuffer, samplingFrequency, numChannels);
    if (adtsSource == NULL) {
        *env << "Failed to create Fifo Source \n";
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, adtsSource);

    // Begin by creating an input stream from our replicator:
    FramedSource* source = replicator->createStreamReplica();

    // Then create a 'dummy sink' object to receive the replica stream:
    MediaSink* sink = DummySink::createNew(*env, "dummy");

    // Now, start playing, feeding the sink object from the source:
    sink->startPlaying(*source, NULL, NULL);

    return replicator;
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms, char const* streamName, int audio)
{
    if (debug == 0) {
        char* url = rtspServer->rtspURL(sms);
        UsageEnvironment& env = rtspServer->envir();
        env << "\n\"" << streamName << "\" stream, from memory\n";
        if (audio == 1)
            env << "PCM audio enabled\n";
        else if (audio == 2)
            env << "AAC audio enabled\n";
        env << "Play this stream using the URL \"" << url << "\"\n";
        delete[] url;
    }
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [options]\n\n", progname);
    fprintf(stderr, "\t-m MODEL, --model MODEL\n");
    fprintf(stderr, "\t\tset model: y21ga, y211ga, y291ga, h30ga, r30gb, r35gb, r40ga, h51ga, h52ga, h60ga, y28ga, y29ga, q321br_lsx, qg311r or b091qp (default y21ga)\n");
    fprintf(stderr, "\t-r RES,   --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: low, high or both (default high)\n");
    fprintf(stderr, "\t-a AUDIO, --audio AUDIO\n");
    fprintf(stderr, "\t\tset audio: yes, no, alaw, ulaw, pcm or aac (default yes)\n");
    fprintf(stderr, "\t-p PORT,  --port PORT\n");
    fprintf(stderr, "\t\tset TCP port (default 554)\n");
    fprintf(stderr, "\t-s,       --sti\n");
    fprintf(stderr, "\t\tdon't overwrite SPS timing info (default overwrite)\n");
    fprintf(stderr, "\t-u USER,  --user USER\n");
    fprintf(stderr, "\t\tset username\n");
    fprintf(stderr, "\t-w PASSWORD,  --password PASSWORD\n");
    fprintf(stderr, "\t\tset password\n");
    fprintf(stderr, "\t-d DEBUG, --debug DEBUG\n");
    fprintf(stderr, "\t\t0 none, 1 grabber, 2 rtsp library or 3 both\n");
    fprintf(stderr, "\t-h,       --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char** argv)
{
    char *str;
    int nm;
    char user[65];
    char pwd[65];
    int pth_ret;
    int c;
    char *endptr;

    pthread_t capture_thread;

    int convertTo = WA_PCMU;
    char const* inputAudioFileName = "/tmp/audio_fifo";
    struct stat stat_buffer;
    FILE *fFS;

    // Setting default
    model = Y21GA;
    resolution = RESOLUTION_HIGH;
    audio = 1;
    port = 554;
    sps_timing_info = 1;
    debug = 0;

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

        c = getopt_long (argc, argv, "m:r:a:p:su:w:d:h",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            if (strcasecmp("y21ga", optarg) == 0) {
                model = Y21GA;
            } else if (strcasecmp("y211ga", optarg) == 0) {
                model = Y211GA;
            } else if (strcasecmp("y291ga", optarg) == 0) {
                model = Y291GA;
            } else if (strcasecmp("h30ga", optarg) == 0) {
                model = H30GA;
            } else if (strcasecmp("r30gb", optarg) == 0) {
                model = R30GB;
            } else if (strcasecmp("r35gb", optarg) == 0) {
                model = R35GB;
            } else if (strcasecmp("r40ga", optarg) == 0) {
                model = R40GA;
            } else if (strcasecmp("h51ga", optarg) == 0) {
                model = H51GA;
            } else if (strcasecmp("h52ga", optarg) == 0) {
                model = H52GA;
            } else if (strcasecmp("h60ga", optarg) == 0) {
                model = H60GA;
            } else if (strcasecmp("y28ga", optarg) == 0) {
                model = Y28GA;
            } else if (strcasecmp("y29ga", optarg) == 0) {
                model = Y29GA;
            } else if (strcasecmp("q321br_lsx", optarg) == 0) {
                model = Q321BR_LSX;
            } else if (strcasecmp("qg311r", optarg) == 0) {
                model = QG311R;
            } else if (strcasecmp("b091qp", optarg) == 0) {
                model = B091QP;
            }
            break;

        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            } else if (strcasecmp("both", optarg) == 0) {
                resolution = RESOLUTION_BOTH;
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
            if (debug <= 0) {
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
        if (strcasecmp("y21ga", str) == 0) {
            model = Y21GA;
        } else if (strcasecmp("y211ga", str) == 0) {
            model = Y211GA;
        } else if (strcasecmp("y291ga", str) == 0) {
            model = Y291GA;
        } else if (strcasecmp("h30ga", str) == 0) {
            model = H30GA;
        } else if (strcasecmp("r30gb", str) == 0) {
            model = R30GB;
        } else if (strcasecmp("r35gb", str) == 0) {
            model = R35GB;
        } else if (strcasecmp("r40ga", str) == 0) {
            model = R40GA;
        } else if (strcasecmp("h51ga", str) == 0) {
            model = H51GA;
        } else if (strcasecmp("h52ga", str) == 0) {
            model = H52GA;
        } else if (strcasecmp("h60ga", str) == 0) {
            model = H60GA;
        } else if (strcasecmp("y28ga", str) == 0) {
            model = Y28GA;
        } else if (strcasecmp("y29ga", str) == 0) {
            model = Y29GA;
        } else if (strcasecmp("q321br_lsx", str) == 0) {
            model = Q321BR_LSX;
        } else if (strcasecmp("qg311r", str) == 0) {
            model = QG311R;
        } else if (strcasecmp("b091qp", str) == 0) {
            model = B091QP;
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
    if (debug & 1) fprintf(stderr, "%lld: the size of the buffer is %d\n",
            current_timestamp(), buf_size);

    if (model == Y21GA) {
        buf_offset = BUF_OFFSET_Y21GA;
        frame_header_size = FRAME_HEADER_SIZE_Y21GA;
    } else if (model == Y211GA) {
        buf_offset = BUF_OFFSET_Y211GA;
        frame_header_size = FRAME_HEADER_SIZE_Y211GA;
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
    output_buffer_low.type = TYPE_LOW;
    output_buffer_low.size = OUTPUT_BUFFER_SIZE_LOW;
    output_buffer_low.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_LOW * sizeof(unsigned char));
    output_buffer_low.write_index = output_buffer_low.buffer;
    output_buffer_low.frame_read_index = 0;
    output_buffer_low.frame_write_index = 0;
    output_buffer_low.output_frame_size = sizeof(output_buffer_low.output_frame) / sizeof(output_buffer_low.output_frame[0]);
    if (output_buffer_low.buffer == NULL) {
        fprintf(stderr, "could not alloc memory\n");
        exit(EXIT_FAILURE);
    }
    output_buffer_low.output_frame[0].ptr = output_buffer_low.buffer;
    output_buffer_low.output_frame[0].counter = 0;
    output_buffer_low.output_frame[0].size = 0;

    // High res
    output_buffer_high.type = TYPE_HIGH;
    output_buffer_high.size = OUTPUT_BUFFER_SIZE_HIGH;
    output_buffer_high.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_HIGH * sizeof(unsigned char));
    output_buffer_high.write_index = output_buffer_high.buffer;
    output_buffer_high.frame_read_index = 0;
    output_buffer_high.frame_write_index = 0;
    output_buffer_high.output_frame_size = sizeof(output_buffer_high.output_frame) / sizeof(output_buffer_high.output_frame[0]);
    if (output_buffer_high.buffer == NULL) {
        fprintf(stderr, "could not alloc memory\n");
        if(output_buffer_low.buffer != NULL) free(output_buffer_low.buffer);
        exit(EXIT_FAILURE);
    }
    output_buffer_high.output_frame[0].ptr = output_buffer_high.buffer;
    output_buffer_high.output_frame[0].counter = 0;
    output_buffer_high.output_frame[0].size = 0;

    // Audio
    output_buffer_audio.type = TYPE_AAC;
    output_buffer_audio.size = OUTPUT_BUFFER_SIZE_AUDIO;
    output_buffer_audio.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_AUDIO * sizeof(unsigned char));
    output_buffer_audio.write_index = output_buffer_audio.buffer;
    output_buffer_audio.frame_read_index = 0;
    output_buffer_audio.frame_write_index = 0;
    output_buffer_audio.output_frame_size = sizeof(output_buffer_audio.output_frame) / sizeof(output_buffer_audio.output_frame[0]);
    if (output_buffer_audio.buffer == NULL) {
        fprintf(stderr, "could not alloc memory\n");
        if(output_buffer_low.buffer != NULL) free(output_buffer_low.buffer);
        if(output_buffer_high.buffer != NULL) free(output_buffer_high.buffer);
        exit(EXIT_FAILURE);
    }
    output_buffer_audio.output_frame[0].ptr = output_buffer_audio.buffer;
    output_buffer_audio.output_frame[0].counter = 0;
    output_buffer_audio.output_frame[0].size = 0;

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    // Start capture thread
    if (pthread_mutex_init(&(output_buffer_low.mutex), NULL) != 0) { 
        *env << "Failed to create mutex\n";
        if(output_buffer_low.buffer != NULL) free(output_buffer_low.buffer);
        if(output_buffer_high.buffer != NULL) free(output_buffer_high.buffer);
        if(output_buffer_audio.buffer != NULL) free(output_buffer_audio.buffer);
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&(output_buffer_high.mutex), NULL) != 0) { 
        *env << "Failed to create mutex\n";
        if(output_buffer_low.buffer != NULL) free(output_buffer_low.buffer);
        if(output_buffer_high.buffer != NULL) free(output_buffer_high.buffer);
        if(output_buffer_audio.buffer != NULL) free(output_buffer_audio.buffer);
        exit(EXIT_FAILURE);
    }
    pth_ret = pthread_create(&capture_thread, NULL, capture, (void*) NULL);
    if (pth_ret != 0) {
        *env << "Failed to create capture thread\n";
        if(output_buffer_low.buffer != NULL) free(output_buffer_low.buffer);
        if(output_buffer_high.buffer != NULL) free(output_buffer_high.buffer);
        if(output_buffer_audio.buffer != NULL) free(output_buffer_audio.buffer);
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

    if (debug & 1) {
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
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        if(output_buffer_low.buffer != NULL) free(output_buffer_low.buffer);
        if(output_buffer_high.buffer != NULL) free(output_buffer_high.buffer);
        if(output_buffer_audio.buffer != NULL) free(output_buffer_audio.buffer);
        exit(1);
    }

    StreamReplicator* replicator = NULL;
    if (audio == 1) {
        if (debug & 1) fprintf(stderr, "Starting pcm replicator\n");
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(inputAudioFileName, convertTo);
    } else if (audio == 2) {
        if (debug & 1) fprintf(stderr, "Starting aac replicator\n");
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(&output_buffer_audio, 16000, 1);
    }

    char const* descriptionString = "Session streamed by \"rRTSPServer\"";

    // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
    OutPacketBuffer::maxSize = 262144;

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
                                   ::createNew(*env, &output_buffer_high, reuseFirstSource));
        } else if (stream_type.codec_high == CODEC_H265) {
            sms_high->addSubsession(H265VideoFramedMemoryServerMediaSubsession
                                   ::createNew(*env, &output_buffer_high, reuseFirstSource));
        }
        if (audio == 1) {
            sms_high->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, convertTo));
        } else if (audio == 2) {
            sms_high->addSubsession(ADTSAudioFramedMemoryServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource));
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
                                       ::createNew(*env, &output_buffer_low, reuseFirstSource));
        } else if (stream_type.codec_low == CODEC_H265) {
            sms_low->addSubsession(H265VideoFramedMemoryServerMediaSubsession
                                       ::createNew(*env, &output_buffer_low, reuseFirstSource));
        }
        if (audio == 1) {
            sms_low->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, convertTo));
        } else if (audio == 2) {
            sms_low->addSubsession(ADTSAudioFramedMemoryServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource));
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
                                       ::createNew(*env, replicator, reuseFirstSource, convertTo));
        } else if (audio == 2) {
            sms_audio->addSubsession(ADTSAudioFramedMemoryServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource));
        }
        rtspServer->addServerMediaSession(sms_audio);

        announceStream(rtspServer, sms_audio, streamName, audio);
    }

    env->taskScheduler().doEventLoop(); // does not return

    pthread_mutex_destroy(&(output_buffer_low.mutex));
    pthread_mutex_destroy(&(output_buffer_high.mutex));

    // Free buffers
    free(output_buffer_low.buffer);
    free(output_buffer_high.buffer);
    free(output_buffer_audio.buffer);

    return 0; // only to prevent compiler warning
}
