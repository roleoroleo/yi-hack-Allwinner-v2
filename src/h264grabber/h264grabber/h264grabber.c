/*
 * Copyright (c) 2026 roleo.
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
#include <sys/resource.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <semaphore.h>

#include "h264grabber.h"

#define MAX_FRAMES_PER_POLL 64            // frames parsed from the shared buffer in a single poll
#define MAX_BAD_POLLS 10                  // consecutive polls with an invalid header before a resync
#define COUNTER_MASK 0xFFFFFFFF           // the frame counter in the header is 32 bit

int buf_offset;
int buf_size;
int frame_header_size;
struct stream_type_s stream_type;

unsigned char NALx_START[]          = {0x00, 0x00, 0x00, 0x01};
unsigned char SPS4_START[]          = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char SPS5_START[]          = {0x00, 0x00, 0x00, 0x01, 0x42};
unsigned char PPS4_START[]          = {0x00, 0x00, 0x00, 0x01, 0x68};

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

struct output_s out_low, out_high, out_aac;
volatile sig_atomic_t exit_main = 0;
unsigned char *frame_copy = NULL;           // polling mode: copy of the frame, checked before the output

int slot;                                   // reader slot, SLOT_DISABLED for the polling mode
int slot_size;                              // size of a slot in the layout of this buffer
int model_v1;                               // v1 model (Allwinner): the stock apps use other slots
int slot_lock_fd = -1;
sem_t *sem_fshare_read_lock = SEM_FAILED;
sem_t *sem_fshare_write_lock = SEM_FAILED;
sem_t *sem_fshare_notify = SEM_FAILED;

int frame_counter_last_valid_low = -1;
int frame_counter_last_valid_high = -1;
int frame_counter_last_valid_audio = -1;

long long current_timestamp()
{
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

// Bytes from "from" to "to" going forward in the circular buffer
int cb_dist(unsigned char *to, unsigned char *from)
{
    int d = to - from;

    if (d < 0) d += (buf_size - buf_offset);
    return d;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf >= addr + buf_size))
        buf -= (buf_size - buf_offset);
    if ((offset < 0) && (buf < addr + buf_offset))
        buf += (buf_size - buf_offset);

    return buf;
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
    if (fp == NULL) {
        // Unsupported header size: never leave the caller's header uninitialized
        memset(fh, 0, sizeof(struct frame_header));
        return;
    }

    if (src + n > addr + buf_size) {
        memcpy(fp, src, addr + buf_size - src);
        memcpy(fp + (addr + buf_size - src), addr + buf_offset, n - (addr + buf_size - src));
    } else {
        memcpy(fp, src, n);
    }
    if (n == sizeof(fh22)) {
        fh->len = fh22.len;
        fh->counter = fh22.counter;
        fh->time_ms = fh22.time_ms;
        fh->type = fh22.type;
        fh->stream_counter = fh22.stream_counter;
    } else if (n == sizeof(fh24)) {
        fh->len = fh24.len;
        fh->counter = fh24.counter;
        fh->time_ms = fh24.time_ms;
        fh->type = fh24.type;
        fh->stream_counter = fh24.stream_counter;
    } else if (n == sizeof(fh26)) {
        fh->len = fh26.len;
        fh->counter = fh26.counter;
        fh->time_ms = fh26.time_ms;
        fh->type = fh26.type;
        fh->stream_counter = fh26.stream_counter;
    } else if (n == sizeof(fh28)) {
        fh->len = fh28.len;
        fh->counter = fh28.counter;
        fh->time_ms = fh28.time_ms;
        fh->type = fh28.type;
        fh->stream_counter = fh28.stream_counter;
    }
}

/**
 * slot_reserve() - Reserve a reader slot for this process
 * @s: slot number
 *
 * Bind a unix socket with the name of the slot in the abstract namespace:
 * the name is released by the kernel when the process dies, so a crashed
 * process never keeps a slot busy. (The camera kernel has no file locking:
 * flock() and fcntl() locks fail with ENOSYS.)
 *
 * Return: 0 if the slot is ours, -1 if another process has it.
 */
int slot_reserve(int s)
{
    struct sockaddr_un sa;
    socklen_t len;
    int fd;

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        fprintf(stderr, "error creating socket: %s\n", strerror(errno));
        return -1;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    // sun_path[0] = 0: abstract namespace, no file is created
    len = snprintf(sa.sun_path + 1, sizeof(sa.sun_path) - 1, SLOT_LOCK_NAME, s);
    len += offsetof(struct sockaddr_un, sun_path) + 1;
    if (bind(fd, (struct sockaddr *) &sa, len) < 0) {
        if (debug) fprintf(stderr, "slot %d not available: %s\n", s, strerror(errno));
        close(fd);
        return -1;
    }
    slot_lock_fd = fd;
    return 0;
}

/**
 * slot_release() - Release the slot reserved by slot_reserve()
 */
void slot_release()
{
    if (slot_lock_fd >= 0) {
        close(slot_lock_fd);
        slot_lock_fd = -1;
    }
}

/**
 * sem_fshare_open() - Open the semaphores of the fshare buffer
 * @s: reader slot, whose notify semaphore is opened
 *
 * Return: 0 on success, a negative value if a semaphore can't be opened.
 */
int sem_fshare_open(int s)
{
    char name[32];

    sem_fshare_read_lock = sem_open(READ_LOCK_FILE, O_RDWR);
    if (sem_fshare_read_lock == SEM_FAILED) {
        fprintf(stderr, "error opening %s: %s\n", READ_LOCK_FILE, strerror(errno));
        return -1;
    }
    sem_fshare_write_lock = sem_open(WRITE_LOCK_FILE, O_RDWR);
    if (sem_fshare_write_lock == SEM_FAILED) {
        fprintf(stderr, "error opening %s: %s\n", WRITE_LOCK_FILE, strerror(errno));
        return -2;
    }
    snprintf(name, sizeof(name), READ_NOTIFY_FILE, s);
    sem_fshare_notify = sem_open(name, O_RDWR);
    if (sem_fshare_notify == SEM_FAILED) {
        fprintf(stderr, "error opening %s: %s\n", name, strerror(errno));
        return -3;
    }
    return 0;
}

/**
 * sem_fshare_close() - Close the semaphores opened by sem_fshare_open()
 */
void sem_fshare_close()
{
    if (sem_fshare_notify != SEM_FAILED) {
        sem_close(sem_fshare_notify);
        sem_fshare_notify = SEM_FAILED;
    }
    if (sem_fshare_write_lock != SEM_FAILED) {
        sem_close(sem_fshare_write_lock);
        sem_fshare_write_lock = SEM_FAILED;
    }
    if (sem_fshare_read_lock != SEM_FAILED) {
        sem_close(sem_fshare_read_lock);
        sem_fshare_read_lock = SEM_FAILED;
    }
}

/**
 * sem_wait_nointr() - sem_wait() restarted if interrupted by a signal
 * @sem: semaphore
 *
 * The locks must be taken: a signal must not make the caller go on without them.
 */
void sem_wait_nointr(sem_t *sem)
{
    while ((sem_wait(sem) != 0) && (errno == EINTR));
}

/**
 * reader_lock() - Enter the buffer as a reader
 *
 * The first reader takes the writer lock, so the writer can't write any frame until
 * the last reader leaves: nothing between reader_lock() and reader_unlock()
 * may block.
 */
void reader_lock()
{
    struct fshare_header *h = (struct fshare_header *) addr;

    sem_wait_nointr(sem_fshare_read_lock);
    h->readers++;
    if (h->readers == 1) sem_wait_nointr(sem_fshare_write_lock);
    sem_post(sem_fshare_read_lock);
}

/**
 * reader_unlock() - Leave the buffer: the last reader releases the writer lock
 */
void reader_unlock()
{
    struct fshare_header *h = (struct fshare_header *) addr;

    sem_wait_nointr(sem_fshare_read_lock);
    h->readers--;
    if (h->readers == 0) sem_post(sem_fshare_write_lock);
    sem_post(sem_fshare_read_lock);
}

/**
 * fshare_slot_size() - Size of a reader slot in the layout of the buffer
 * @offset: bytes before the data area (buffer header and slots)
 * @header_size: size of the frame headers
 *
 * Return: size of a slot in bytes, 0 if the layout is unknown (the slots
 * can't be used).
 */
int fshare_slot_size(int offset, int header_size)
{
    if ((offset == 368) && ((header_size == 28) || (header_size == 24))) return 20;
    if ((offset == 300) && ((header_size == 26) || (header_size == 22))) return 16;
    return 0;
}

/**
 * slot_auto_choice() - Slots tried by -S -2
 * @first: set to the slot tried first
 * @second: set to the slot tried if @first is busy
 * @v1: the model is a v1 one (y20ga, y25ga, y30qa, y501gc)
 *
 * The slots that the stock apps of the model don't use, see "fshare reader slots".
 */
void slot_auto_choice(int *first, int *second, int v1)
{
    if (v1) {
        *first = 16;                        // no rtmp/tserver in the v1 firmwares
        *second = 15;                       // p2p_tnp, only with 10 clients
    } else {
        *first = 15;
        *second = 14;
    }
}

/**
 * slot_ptr() - Address of a reader slot
 * @s: slot number
 *
 * The slots follow the fields of struct fshare_header, slot_size bytes each.
 *
 * Return: pointer to the fields of slot @s.
 */
struct fshare_slot *slot_ptr(int s)
{
    return (struct fshare_slot *) (addr + sizeof(struct fshare_header) + s * slot_size);
}

/**
 * slot_set_mask() - Change the mask of a slot
 * @s: slot number
 * @mask: frame types to receive, see struct fshare_slot
 *
 * The stock readers change it holding the writer lock.
 */
void slot_set_mask(int s, uint16_t mask)
{
    sem_wait_nointr(sem_fshare_write_lock);
    slot_ptr(s)->mask = mask;
    sem_post(sem_fshare_write_lock);
}

/**
 * slot_seek_newest() - Start reading a slot from the newest frame
 * @s: slot number
 */
void slot_seek_newest(int s)
{
    struct fshare_header *h = (struct fshare_header *) addr;

    reader_lock();
    slot_ptr(s)->last_counter = h->last_counter;
    slot_ptr(s)->has_data = 0;
    reader_unlock();
}

/**
 * slot_stop() - Stop receiving the notifications of a slot
 * @s: slot number
 */
void slot_stop(int s)
{
    slot_set_mask(s, 0);
    reader_lock();
    slot_ptr(s)->has_data = 0;
    slot_ptr(s)->waiting = 0;
    reader_unlock();
}

/**
 * slot_wait() - Wait until the writer has a frame for a slot
 * @s: slot number
 *
 * The semaphore can count more posts than the frames still to read:
 * has_data, checked again by the caller, is what matters.
 *
 * Return: 1 if there is data, 0 after SLOT_WAIT_MS or if a signal arrived.
 */
int slot_wait(int s)
{
    struct fshare_slot *sl = slot_ptr(s);
    struct timespec ts;

    reader_lock();
    if (sl->has_data) {
        sl->waiting = 0;
        reader_unlock();
        return 1;
    }
    sl->waiting = 1;
    reader_unlock();

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += SLOT_WAIT_MS / 1000;
    ts.tv_nsec += (SLOT_WAIT_MS % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    if (sem_timedwait(sem_fshare_notify, &ts) != 0) return 0;
    return 1;
}

/**
 * slot_read() - Copy the new frames of a slot out of the buffer
 * @stage: buffer where the payloads are copied, one after the other
 * @fhs: set to the headers of the frames copied
 * @data: set to the payloads of the frames copied (pointers into @stage)
 * @more: set to 1 if not all the new frames fit, else to 0
 * @s: slot number
 * @stage_size: size of @stage
 * @max_frames: size of @fhs and @data
 *
 * Copy the frames newer than the last one read, of the types in the mask of
 * the slot, while holding the readers lock. The walk starts after the last
 * frame read, if it's still in the buffer, else from the oldest frame.
 *
 * Return: number of frames copied.
 */
int slot_read(unsigned char *stage, struct frame_header *fhs, unsigned char **data, int *more,
              int s, int stage_size, int max_frames)
{
    static int have_last_pos = 0;
    static uint32_t last_pos;               // offset of the last frame read
    struct fshare_header *h = (struct fshare_header *) addr;
    struct fshare_slot *sl = slot_ptr(s);
    unsigned char *ring = addr + buf_offset;
    uint32_t ring_size = buf_size - buf_offset;
    uint32_t fh_size = frame_header_size;
    struct frame_header f;                  // any frame header size, see cb2s_headercpy()
    uint32_t pos;                           // offsets in the data area, always < ring_size
    int32_t left;
    int n = 0, used = 0;

    *more = 0;
    reader_lock();
    pos = h->read_pos;
    left = h->used;
    // Start after the last frame read if it's still in the buffer: no need
    // to walk the headers of all the older frames
    if (have_last_pos) {
        cb2s_headercpy((unsigned char *) &f, ring + last_pos, fh_size);
        if ((f.counter == sl->last_counter) && (f.len <= ring_size - fh_size)) {
            uint32_t next = (last_pos + fh_size + f.len) % ring_size;
            uint32_t to_write = (h->write_pos + ring_size - next) % ring_size;
            if ((int32_t) to_write <= h->used) {
                pos = next;
                left = to_write;
            }
        }
    }
    while (left > 0) {
        cb2s_headercpy((unsigned char *) &f, ring + pos, fh_size);
        if (f.len > ring_size - fh_size) break;          // never seen: give up this round
        if ((sl->mask & f.type & FSHARE_TYPE_MASK) &&
                ((sl->last_counter == 0) || ((int32_t) (f.counter - sl->last_counter) > 0))) {
            if ((n == max_frames) || (used + (int) f.len > stage_size)) {
                *more = 1;
                break;
            }
            cb2s_memcpy(stage + used, cb_move(ring + pos, fh_size), f.len);
            fhs[n] = f;
            data[n] = stage + used;
            used += f.len;
            n++;
            sl->last_counter = f.counter;
            last_pos = pos;
            have_last_pos = 1;
        }
        pos = (pos + fh_size + f.len) % ring_size;
        left -= fh_size + f.len;
    }
    if (!*more) sl->has_data = 0;
    reader_unlock();

    return n;
}

/**
 * sigpipe_handler() - Handler of SIGPIPE
 * @signal: signal number
 */
void sigpipe_handler(int signal)
{
    // Do nothing
}

/**
 * stop_handler() - Handler of SIGTERM and SIGINT
 * @signal: signal number
 */
void stop_handler(int signal)
{
    exit_main = 1;
}

// Create the fifo only if it doesn't exist yet.
// Don't unlink an existing fifo: a reader that keeps it open would go on
// reading from the old one.
int create_fifo(const char *fifo_name, mode_t mode)
{
    struct stat st;

    if (mkfifo(fifo_name, mode) == 0) return 0;
    if (errno != EEXIST) return -1;
    if ((stat(fifo_name, &st) == 0) && S_ISFIFO(st.st_mode)) return 0;

    // The path exists but it's not a fifo: replace it
    if (unlink(fifo_name) < 0) return -1;
    return mkfifo(fifo_name, mode);
}

void output_reset(struct output_s *o)
{
    memset(o, 0, sizeof(*o));
    o->fd = -1;
}

void output_init_stdout(struct output_s *o, int is_video)
{
    output_reset(o);
    o->fp = stdout;
    o->is_video = is_video;
    o->drop_until_sync = is_video;
}

void output_init_fifo(struct output_s *o, const char *name, int req_size, int is_video)
{
    output_reset(o);
    o->name = name;
    o->req_size = req_size;
    o->is_video = is_video;
}

// Open the fifo without blocking. open(O_WRONLY | O_NONBLOCK) fails with ENXIO
// while nobody has the fifo open for reading: try again later.
// No helper thread is needed, and no byte of the stream is consumed by us.
void fifo_open(struct output_s *o)
{
    long long now;
    int fd;

    if ((o->name == NULL) || (o->fd >= 0)) return;
    now = current_timestamp();
    if (now < o->next_open) return;
    o->next_open = now + FIFO_REOPEN_MS;

    fd = open(o->name, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        if (errno != ENXIO) fprintf(stderr, "%lld: error opening fifo %s: %s\n", now, o->name, strerror(errno));
        return;
    }

    // A failure here is not fatal: go on with the size the kernel gives us
    if ((o->req_size > 0) && (fcntl(fd, F_SETPIPE_SZ, o->req_size) < 0)) {
        fprintf(stderr, "%lld: warning - unable to set size of fifo %s to %d: %s\n",
                now, o->name, o->req_size, strerror(errno));
    }
    o->size = fcntl(fd, F_GETPIPE_SZ);
    if (debug) fprintf(stderr, "%lld: fifo %s opened, size %d\n", now, o->name, o->size);

    o->fd = fd;
    // The reader must start from a sync point
    o->drop_until_sync = o->is_video;
}

// Close the fifo: with no other process holding it, the pipe and the data
// still inside are destroyed, so the next reader won't get old frames
void fifo_close(struct output_s *o)
{
    if (o->fd >= 0) {
        close(o->fd);
        o->fd = -1;
    }
    o->next_open = current_timestamp() + FIFO_REOPEN_MS;
}

// Write all the segments, waiting (poll) when the fifo is full, up to timeout_ms in total.
// Returns 0 on success or -errno (-ETIMEDOUT if the time is over).
int fifo_write_iov(int fd, struct iovec *iov, int cnt, int timeout_ms)
{
    long long deadline = current_timestamp() + timeout_ms;
    ssize_t w;

    while (cnt > 0) {
        w = writev(fd, iov, cnt);
        if (w > 0) {
            // Skip what has been written
            while ((cnt > 0) && ((size_t) w >= iov->iov_len)) {
                w -= iov->iov_len;
                iov++;
                cnt--;
            }
            if (cnt > 0) {
                iov->iov_base = (unsigned char *) iov->iov_base + w;
                iov->iov_len -= w;
            }
            continue;
        }
        if (w == 0) return -EIO;
        if (errno == EINTR) continue;
        if (errno == EAGAIN) {
            struct pollfd pfd;
            long long left = deadline - current_timestamp();
            int r;

            if (left <= 0) return -ETIMEDOUT;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            pfd.revents = 0;
            r = poll(&pfd, 1, (int) left);
            if (r == 0) return -ETIMEDOUT;
            if ((r < 0) && (errno != EINTR)) return -errno;
            if ((r > 0) && (pfd.revents & POLLERR)) return -EPIPE;
            continue;
        }
        return -errno;
    }

    return 0;
}

/**
 * is_sync_nal() - Check if a NAL unit is a point where a reader can start decoding
 * @codec: CODEC_H264, CODEC_H265 or CODEC_NONE
 * @nal: NAL unit, with its start code (at least 5 bytes)
 *
 * The sync points are the SPS for H264 and the VPS for H265: they are followed
 * by the other parameter sets and by the key frame.
 *
 * Return: 1 if @nal starts with a start code and is a sync point (any NAL
 * with CODEC_NONE), else 0.
 */
int is_sync_nal(int codec, unsigned char *nal)
{
    if (memcmp(nal, NALx_START, sizeof(NALx_START)) != 0) return 0;
    if (codec == CODEC_H264) return ((nal[4] & 0x1F) == 7);
    if (codec == CODEC_H265) return (((nal[4] >> 1) & 0x3F) == 32);
    return 1;
}

/**
 * output_frame() - Send a frame to an output
 * @o: output (stdout or fifo)
 * @iov: segments of the frame
 * @cnt: number of segments in @iov
 * @total: bytes of the frame (sum of the segments)
 * @is_sync: the frame is a sync point (see is_sync_nal())
 *
 * While @o->drop_until_sync is set (at the start, when the fifo is opened and
 * after a video frame is lost), the frames are dropped until the next sync
 * point.
 *
 * On stdout the writes are blocking; an error on stdout sets exit_main.
 *
 * On a fifo a frame is written whole or not at all: a truncated frame would
 * corrupt the stream until the next key frame without any notice. The frame
 * is dropped if nobody is reading or if the fifo hasn't enough free space;
 * if the reader closes the fifo, it's closed too and opened again later
 * (see fifo_open()).
 */
void output_frame(struct output_s *o, struct iovec *iov, int cnt, int total, int is_sync)
{
    int used = 0;
    int ret, i;

    if (o->name == NULL) {
        // stdout: blocking writes
        if (o->drop_until_sync && !is_sync) return;
        o->drop_until_sync = 0;
        for (i = 0; i < cnt; i++) fwrite(iov[i].iov_base, 1, iov[i].iov_len, o->fp);
        // With SIGPIPE ignored (e.g. by the parent process) a closed reader gives EPIPE here
        if ((fflush(o->fp) != 0) || ferror(o->fp)) {
            fprintf(stderr, "%lld: error writing to stdout: %s\n", current_timestamp(), strerror(errno));
            exit_main = 1;
        }
        return;
    }

    fifo_open(o);
    if (o->fd < 0) return;               // nobody is reading

    if (o->drop_until_sync) {
        if (!is_sync) return;
        o->drop_until_sync = 0;
    }

    // Check the free space: we are the only writer, so it can only grow until we write
    if (ioctl(o->fd, FIONREAD, &used) < 0) used = 0;
    if ((o->size > 0) && (total <= o->size) && (total > o->size - used)) {
        if (debug) fprintf(stderr, "%lld: fifo %s full (%d/%d), frame of %d bytes dropped\n",
                current_timestamp(), o->name, used, o->size, total);
        // The next frames may refer to this one: wait for a sync point
        o->drop_until_sync = o->is_video;
        return;
    }

    // The frame fits (or it's larger than the whole pipe and needs more writes)
    ret = fifo_write_iov(o->fd, iov, cnt, FIFO_WRITE_TIMEOUT_MS);
    if (ret == -EPIPE) {
        if (debug) fprintf(stderr, "%lld: fifo %s has no reader, closed\n", current_timestamp(), o->name);
        fifo_close(o);
    } else if (ret < 0) {
        fprintf(stderr, "%lld: warning - frame of %d bytes truncated on fifo %s: %s\n",
                current_timestamp(), total, o->name, strerror(-ret));
        o->drop_until_sync = o->is_video;
    }
}

/**
 * get_sps_ti() - Return the SPS with timing info
 * @len: length of the SPS returned
 * @frame_type: type of frame, low or high resolution
 *
 * Return: the SPS with timing info at 20 fps that replaces the detected one (NULL if none).
 */
unsigned char *get_sps_ti(int *len, int frame_type)
{
    int t;

    if (frame_type == TYPE_LOW) {
        t = stream_type.sps_type_low;
        if (t == 0x0101) { *len = sizeof(SPS4_640X360_TI);     return SPS4_640X360_TI; }
        if (t == 0x0201) { *len = sizeof(SPS4_2_640X360_TI);   return SPS4_2_640X360_TI; }
        if (t == 0x0401) { *len = sizeof(SPS4_3_640X360_TI);   return SPS4_3_640X360_TI; }
    } else if (frame_type == TYPE_HIGH) {
        t = stream_type.sps_type_high;
        if (t == 0x0102) { *len = sizeof(SPS4_1920X1080_TI);   return SPS4_1920X1080_TI; }
        if (t == 0x0202) { *len = sizeof(SPS4_2_1920X1080_TI); return SPS4_2_1920X1080_TI; }
        if (t == 0x0402) { *len = sizeof(SPS4_3_1920X1080_TI); return SPS4_3_1920X1080_TI; }
        if (t == 0x0104) { *len = sizeof(SPS4_2304X1296_TI);   return SPS4_2304X1296_TI; }
        if (t == 0x0204) { *len = sizeof(SPS4_2_2304X1296_TI); return SPS4_2_2304X1296_TI; }
        if (t == 0x0404) { *len = sizeof(SPS4_3_2304X1296_TI); return SPS4_3_2304X1296_TI; }
        // H265 SPS with timing info are not available (0x0802, 0x1002, 0x2004)
    }

    return NULL;
}

/**
 * frame_starts_with() - Compare the start of a frame with a known NAL
 * @data: frame data
 * @len: bytes of @data
 * @ref: NAL to look for
 * @ref_len: bytes of @ref
 *
 * Return: 1 if @data starts with @ref (without reading past the frame), else 0.
 */
int frame_starts_with(unsigned char *data, int len, unsigned char *ref, int ref_len)
{
    return (len >= ref_len) && (memcmp(data, ref, ref_len) == 0);
}

/**
 * process_frame() - Handle one frame and send it to its output
 * @fh: header of the frame
 * @data: payload of the frame (after the frame header), contiguous in memory,
 *        fh->len bytes long
 * @ring_header: address of the frame header in the circular buffer in polling
 *               mode, NULL in slot mode
 *
 * In polling mode @data may still be in the circular buffer (if the frame
 * doesn't wrap around its end): the frame is copied to frame_copy and after
 * the copy the header is checked again, because the writer may have
 * overwritten the frame. In slot mode @data is a copy made with the readers
 * lock, so it can't change.
 */
void process_frame(struct frame_header *fh, unsigned char *data, unsigned char *ring_header)
{
    struct output_s *out;
    int frame_type;
    int frame_len = fh->len;
    int frame_counter;
    int write_enable;

    // If SPS
    if (fh->type & 0x0002) {
        // Skip the prefix with fps, width and height
        data += 6;
        frame_len -= 6;
        if (frame_len <= 0) return;

        // Autodetect stream type (only the 1st time)
        if ((stream_type.codec_low  == CODEC_NONE) && (fh->type & 0x0800)) {
            if (frame_starts_with(data, frame_len, SPS4_640X360, sizeof(SPS4_640X360))) {
                stream_type.codec_low = CODEC_H264;
                stream_type.sps_type_low = 0x0101;
            } else if (frame_starts_with(data, frame_len, SPS4_2_640X360, sizeof(SPS4_2_640X360))) {
                stream_type.codec_low = CODEC_H264;
                stream_type.sps_type_low = 0x0201;
            } else if (frame_starts_with(data, frame_len, SPS4_3_640X360, sizeof(SPS4_3_640X360))) {
                stream_type.codec_low = CODEC_H264;
                stream_type.sps_type_low = 0x0401;
            } else if (frame_starts_with(data, frame_len, SPS4_START, sizeof(SPS4_START))) {
                stream_type.codec_low = CODEC_H264;
                stream_type.sps_type_low = 0x0000;
            } else if (frame_starts_with(data, frame_len, SPS5_START, sizeof(SPS5_START))) {
                stream_type.codec_low = CODEC_H265;
                stream_type.sps_type_low = 0x0000;
            }
            if ((debug) && (stream_type.codec_low != CODEC_NONE)) fprintf(stderr, "%lld: low - codec type is %d - sps type is %d\n",
                    current_timestamp(), stream_type.codec_low, stream_type.sps_type_low);
        } else if ((stream_type.codec_high  == CODEC_NONE) && (fh->type & 0x0400)) {
            if (frame_starts_with(data, frame_len, SPS4_1920X1080, sizeof(SPS4_1920X1080))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0102;
            } else if (frame_starts_with(data, frame_len, SPS4_2304X1296, sizeof(SPS4_2304X1296))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0104;
            } else if (frame_starts_with(data, frame_len, SPS4_2_1920X1080, sizeof(SPS4_2_1920X1080))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0202;
            } else if (frame_starts_with(data, frame_len, SPS4_2_2304X1296, sizeof(SPS4_2_2304X1296))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0204;
            } else if (frame_starts_with(data, frame_len, SPS4_3_1920X1080, sizeof(SPS4_3_1920X1080))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0402;
            } else if (frame_starts_with(data, frame_len, SPS4_3_2304X1296, sizeof(SPS4_3_2304X1296))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0404;
            } else if (frame_starts_with(data, frame_len, SPS5_1920X1080, sizeof(SPS5_1920X1080))) {
                stream_type.codec_high = CODEC_H265;
                stream_type.sps_type_high = 0x0802;
            } else if (frame_starts_with(data, frame_len, SPS5_2_1920X1080, sizeof(SPS5_2_1920X1080))) {
                stream_type.codec_high = CODEC_H265;
                stream_type.sps_type_high = 0x1002;
            } else if (frame_starts_with(data, frame_len, SPS5_3_2304X1296, sizeof(SPS5_3_2304X1296))) {
                stream_type.codec_high = CODEC_H265;
                stream_type.sps_type_high = 0x2004;
            } else if (frame_starts_with(data, frame_len, SPS4_START, sizeof(SPS4_START))) {
                stream_type.codec_high = CODEC_H264;
                stream_type.sps_type_high = 0x0000;
            } else if (frame_starts_with(data, frame_len, SPS5_START, sizeof(SPS5_START))) {
                stream_type.codec_high = CODEC_H265;
                stream_type.sps_type_high = 0x0000;
            }
            if ((debug) && (stream_type.codec_high != CODEC_NONE)) fprintf(stderr, "%lld: high - codec type is %d - sps type is %d\n",
                    current_timestamp(), stream_type.codec_high, stream_type.sps_type_high);
        }
    }

    write_enable = 1;
    frame_counter = fh->stream_counter;

    // (fh->type & 0x0002) -> SPS
    // (fh->type & 0x0008) -> VPS
    // (fh->type & 0x0800) -> low res frame
    // (fh->type & 0x0400) -> high res frame
    // (fh->type & 0x0100) -> aac frame
    if (fh->type & 0x0800) {
        frame_type = TYPE_LOW;
    } else if (fh->type & 0x0400) {
        frame_type = TYPE_HIGH;
    } else if (fh->type & 0x0100) {
        frame_type = TYPE_AAC;
    } else {
        frame_type = TYPE_NONE;
    }
    if ((frame_type == TYPE_LOW) && ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH))) {
        if ((65536 + frame_counter - frame_counter_last_valid_low) % 65536 > 1) {
            if (debug) fprintf(stderr, "%lld: warning - %d low res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                        current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_low - 1) % 65536, frame_counter, frame_counter_last_valid_low);
        } else if (debug) {
            fprintf(stderr, "%lld: %s detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                    current_timestamp(), (fh->type & 0x0002) ? "SPS  " : "frame", frame_len, frame_counter,
                    frame_counter_last_valid_low, frame_type);
        }
        frame_counter_last_valid_low = frame_counter;
    } else if ((frame_type == TYPE_HIGH) && ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))) {
        if ((65536 + frame_counter - frame_counter_last_valid_high) % 65536 > 1) {
            if (debug) fprintf(stderr, "%lld: warning - %d high res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                        current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_high - 1) % 65536, frame_counter, frame_counter_last_valid_high);
        } else if (debug) {
            fprintf(stderr, "%lld: %s detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                    current_timestamp(), (fh->type & 0x0002) ? "SPS  " : "frame", frame_len, frame_counter,
                    frame_counter_last_valid_high, frame_type);
        }
        frame_counter_last_valid_high = frame_counter;
    } else if ((frame_type == TYPE_AAC) && (audio == 1)) {
        if ((65536 + frame_counter - frame_counter_last_valid_audio) % 65536 > 1) {
            if (debug) fprintf(stderr, "%lld: warning - %d AAC frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                        current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_audio - 1) % 65536, frame_counter, frame_counter_last_valid_audio);
        } else if (debug) {
            fprintf(stderr, "%lld: frame detected - frame_len: %d - frame_counter: %d - audio AAC\n",
                        current_timestamp(), frame_len, frame_counter);
        }
        frame_counter_last_valid_audio = frame_counter;
    } else {
        write_enable = 0;
    }

    if (!write_enable) return;

    // Send the frame to the ouput buffer
    if ((frame_type == TYPE_LOW) && (stream_type.codec_low != CODEC_NONE)) {
        out = &out_low;
    } else if ((frame_type == TYPE_HIGH) && (stream_type.codec_high != CODEC_NONE)) {
        out = &out_high;
    } else if (frame_type == TYPE_AAC) {
        out = &out_aac;
    } else {
        out = NULL;
    }

    if (debug) fprintf(stderr, "%lld: h264/aac in - frame_len: %d\n", current_timestamp(), frame_len);

    if ((out != NULL) && (frame_len > 0)) {
        struct iovec iov[1];
        unsigned char *ti = NULL;
        int ti_len = 0;
        int codec = CODEC_NONE;
        int is_sync = 1;

        // Overwrite SPS with one that contains timing info at 20 fps
        if (sps_timing_info && (fh->type & 0x0002)) {
            ti = get_sps_ti(&ti_len, frame_type);
        }
        if (ti != NULL) {
            data = ti;
            frame_len = ti_len;
        } else if (ring_header != NULL) {
            // The frame may still be in the circular buffer: copy it, so it can't change after the check
            memcpy(frame_copy, data, frame_len);
            data = frame_copy;
        }

        if (ring_header != NULL) {
            // The writer may have overwritten this frame while we parsed and
            // copied it. It rewrites the ring in order, so it reaches the
            // header before the data: if the header is unchanged, the copy is good.
            struct frame_header fh_check;
            __sync_synchronize();
            cb2s_headercpy((unsigned char *) &fh_check, ring_header, frame_header_size);
            if ((fh_check.counter != fh->counter) || (fh_check.len != fh->len)) {
                fprintf(stderr, "%lld: h264/aac in - warning - frame overwritten during copy, frame dropped\n", current_timestamp());
                // The next frames may refer to this one: wait for a sync point
                out->drop_until_sync = out->is_video;
                return;
            }
        }

        iov[0].iov_base = data;
        iov[0].iov_len = frame_len;

        if (frame_type == TYPE_LOW) codec = stream_type.codec_low;
        else if (frame_type == TYPE_HIGH) codec = stream_type.codec_high;
        if (codec != CODEC_NONE) {
            is_sync = (frame_len >= 5) && is_sync_nal(codec, data);
        }
        output_frame(out, iov, 1, frame_len, is_sync);
        if (debug) fprintf(stderr, "%lld: writing frame, length %d\n", current_timestamp(), frame_len);
    } else if (frame_len <= 0) {
        // Guard against a corrupt/short frame
        fprintf(stderr, "%lld: h264/aac in - warning - invalid frame_len %d, frame dropped\n", current_timestamp(), frame_len);
        return;
    }
}

/**
 * slot_capture_loop() - Capture loop with a reader slot
 *
 * Wait for the notifications of the writer and copy the new frames out of the
 * buffer while holding the readers lock, then send them to the outputs.
 * It returns when exit_main is set.
 */
void slot_capture_loop()
{
    unsigned char *stage;
    unsigned char *data[MAX_FRAMES_PER_POLL];
    struct frame_header fhs[MAX_FRAMES_PER_POLL];
    int stage_size = buf_size - buf_offset;
    uint16_t mask = 0;
    int i, n, more;

    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) mask |= 0x0800;
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) mask |= 0x0400;
    if (audio) mask |= 0x0100;

    // A single frame is never larger than the data area
    stage = (unsigned char *) malloc(stage_size);
    if (stage == NULL) {
        fprintf(stderr, "Error - unable to allocate %d bytes\n", stage_size);
        return;
    }

    // Drop the notifications left by a previous reader of this slot
    while (sem_trywait(sem_fshare_notify) == 0);

    slot_set_mask(slot, mask);
    slot_seek_newest(slot);
    if (debug) fprintf(stderr, "%lld: reading from slot %d, mask 0x%04x\n", current_timestamp(), slot, mask);

    while (!exit_main) {
        if (!slot_wait(slot)) {
            // Timeout or signal: let the fifos without a reader be opened
            fifo_open(&out_low);
            fifo_open(&out_high);
            fifo_open(&out_aac);
            continue;
        }
        do {
            n = slot_read(stage, fhs, data, &more, slot, stage_size, MAX_FRAMES_PER_POLL);
            // The writer is free again: now the frames can be sent, even if a fifo is slow
            for (i = 0; (i < n) && !exit_main; i++) process_frame(&fhs[i], data[i], NULL);
        } while (more && !exit_main);
    }

    slot_stop(slot);
    free(stage);
}

/**
 * detect_layout() - Detect the layout of the buffer, if the model doesn't define it
 *
 * Set the offset of the data area and the size of the frame headers.
 */
void detect_layout()
{
    unsigned char *header_a1, *header_a2;
    int i;

    // Autodetect offset if not defined
    if (buf_offset == FRAME_OFFSET_AUTODETECT) {
        memcpy(&i, addr + FRAME_OFFSET_TRY_1, sizeof(i));
        if (i != 0)
            buf_offset = FRAME_OFFSET_TRY_1;
        else
            buf_offset = FRAME_OFFSET_TRY_2;
    }

    // Autodetect header size if not defined
    if ((frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) && (debug)) fprintf(stderr, "detecting frame header size\n");
    while ((frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) && !exit_main) {
        header_a2 = (unsigned char *) memmem(addr + buf_offset, buf_size - buf_offset, PPS4_START, sizeof(PPS4_START));
        if ((header_a2 != NULL) && (header_a2 - 40 > addr + buf_offset)) {
            header_a1 = cb_move(header_a2, -40);
            header_a1 = (unsigned char *) memmem(header_a1, 40, PPS4_HEADER, sizeof(PPS4_HEADER));
            if (header_a1 != NULL) {
                frame_header_size = header_a2 - header_a1;
                if (frame_header_size < 0)
                        frame_header_size += (buf_size - buf_offset);
                // Accept only the header layouts that cb2s_headercpy() can decode
                if ((frame_header_size != (int) sizeof(struct frame_header_22)) &&
                        (frame_header_size != (int) sizeof(struct frame_header_24)) &&
                        (frame_header_size != (int) sizeof(struct frame_header_26)) &&
                        (frame_header_size != (int) sizeof(struct frame_header_28))) {
                    if (debug) fprintf(stderr, "unsupported frame header size %d, retrying\n", frame_header_size);
                    frame_header_size = FRAME_HEADER_SIZE_AUTODETECT;
                }
            }
        }
        usleep(1000);
    }
    if (debug) fprintf(stderr, "frame offset = %d\n", buf_offset);
    if (debug) fprintf(stderr, "frame header size = %d\n", frame_header_size);
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
    fprintf(stderr, "\t-p, --priority\n");
    fprintf(stderr, "\t\tset high priority\n");
    fprintf(stderr, "\t-S SLOT, --slot SLOT\n");
    fprintf(stderr, "\t\treader slot 0-16, -1 to poll the buffer, -2 auto (default -1, auto = 15, else 14; v1 models: 16, else 15)\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
}

int main(int argc, char **argv) {
    int priority;
    unsigned char *buf_idx, *buf_idx_cur, *buf_idx_end, *buf_idx_end_prev;
    char *stdoutbuf = NULL;
    size_t stdoutbuf_size;
    FILE *fFS;
    int fshm;
    mode_t mode = 0755;

    unsigned char *wrap_buf = NULL;
    unsigned char *payload;
    int open_flags, prot;

    int i, n, c;
    int bad_header;
    int bad_polls = 0;
    int more_frames;

    struct frame_header fhs[MAX_FRAMES_PER_POLL];
    unsigned char* fhs_addr[MAX_FRAMES_PER_POLL];
    uint32_t last_counter = 0;
    int have_last_counter = 0;

    output_reset(&out_low);
    output_reset(&out_high);
    output_reset(&out_aac);

    resolution = RESOLUTION_HIGH;
    audio = 0;
    sps_timing_info = 1;
    fifo = 0;
    priority = 0;
    debug = 0;
    slot = SLOT_DISABLED;

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
            {"priority",  no_argument, 0, 'p'},
            {"slot",  required_argument, 0, 'S'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "m:r:afspS:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            model_v1 = 0;
            if (strcasecmp("y20ga", optarg) == 0) {
                model_v1 = 1;
                buf_offset = BUF_OFFSET_Y20GA;
                frame_header_size = FRAME_HEADER_SIZE_Y20GA;
            } else if (strcasecmp("y25ga", optarg) == 0) {
                model_v1 = 1;
                buf_offset = BUF_OFFSET_Y25GA;
                frame_header_size = FRAME_HEADER_SIZE_Y25GA;
            } else if (strcasecmp("y30qa", optarg) == 0) {
                model_v1 = 1;
                buf_offset = BUF_OFFSET_Y30QA;
                frame_header_size = FRAME_HEADER_SIZE_Y30QA;
            } else if (strcasecmp("y501gc", optarg) == 0) {
                model_v1 = 1;
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

        case 'p':
            fprintf (stderr, "high priority on\n");
            priority = 1;
            break;

        case 'S':
            slot = atoi(optarg);
            if ((slot < SLOT_AUTO) || (slot >= FSHARE_SLOTS)) {
                fprintf(stderr, "invalid slot %s\n", optarg);
                return -1;
            }
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

    // Set signal handler
    signal(SIGTERM, stop_handler);
    signal(SIGINT, stop_handler);

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

    if (priority)
        setpriority(PRIO_PROCESS, 0, -10);

    // Opening an existing file: the buffer is shared with the other camera processes.
    // A reader slot (if requested) needs to write the readers counter and the slot data,
    // the polling mode only reads.
    if (slot != SLOT_DISABLED) {
        open_flags = O_RDWR;
        prot = PROT_READ | PROT_WRITE;
    } else {
        open_flags = O_RDONLY;
        prot = PROT_READ;
    }
    fshm = shm_open(BUFFER_SHM, open_flags, 0);
    if ( fshm == -1 ) {
        fprintf(stderr, "Error - could not open file %s\n", BUFFER_FILE) ;
        return -5;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, buf_size, prot, MAP_SHARED, fshm, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Error - mapping file %s\n", BUFFER_FILE);
        close(fshm);
        return -5;
    }
    if (debug) fprintf(stderr, "Mapping file %s, size %d, to %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "Closing the file %s\n", BUFFER_FILE) ;
    close(fshm);

    detect_layout();

    // Choose the reader slot
    if (slot != SLOT_DISABLED) {
        slot_size = fshare_slot_size(buf_offset, frame_header_size);
        if ((slot_size == 0) || (buf_size <= buf_offset)) {
            if (slot != SLOT_AUTO) fprintf(stderr, "warning - reader slots not supported by this model, polling the buffer\n");
            slot = SLOT_DISABLED;
        } else if (slot == SLOT_AUTO) {
            int first, second;

            slot_auto_choice(&first, &second, model_v1);
            if (slot_reserve(first) == 0) {
                slot = first;
            } else if (slot_reserve(second) == 0) {
                slot = second;
            } else {
                fprintf(stderr, "warning - slots %d and %d are used by other grabbers, polling the buffer\n", first, second);
                slot = SLOT_DISABLED;
            }
        } else if (slot_reserve(slot) != 0) {
            fprintf(stderr, "warning - slot %d is used by another grabber, polling the buffer\n", slot);
            slot = SLOT_DISABLED;
        }
    }
    if ((slot != SLOT_DISABLED) && (sem_fshare_open(slot) != 0)) {
        fprintf(stderr, "warning - could not open semaphores, polling the buffer\n");
        sem_fshare_close();
        slot_release();
        slot = SLOT_DISABLED;
    }
    if (debug) {
        if (slot != SLOT_DISABLED) fprintf(stderr, "using reader slot %d\n", slot);
        else fprintf(stderr, "polling the buffer\n");
    }

    // Opening/setting output file
    if (fifo == 0) {
        if (resolution == RESOLUTION_LOW) {
            stdoutbuf_size = 256 * 1024;
            stdoutbuf = (char *) malloc(stdoutbuf_size);
            if ((stdoutbuf == NULL) || (setvbuf(stdout, stdoutbuf, _IOFBF, stdoutbuf_size) != 0)) {
                fprintf(stderr, "Error setting stdout buffer\n");
            }
            output_init_stdout(&out_low, 1);
        } else if (resolution == RESOLUTION_HIGH) {
            stdoutbuf_size = 256 * 1024;
            stdoutbuf = (char *) malloc(stdoutbuf_size);
            if ((stdoutbuf == NULL) || (setvbuf(stdout, stdoutbuf, _IOFBF, stdoutbuf_size) != 0)) {
                fprintf(stderr, "Error setting stdout buffer\n");
            }
            output_init_stdout(&out_high, 1);
        } else if (audio == 1) {
            stdoutbuf_size = 32 * 1024;
            stdoutbuf = (char *) malloc(stdoutbuf_size);
            if ((stdoutbuf == NULL) || (setvbuf(stdout, stdoutbuf, _IOFBF, stdoutbuf_size) != 0)) {
                fprintf(stderr, "Error setting stdout buffer\n");
            }
            output_init_stdout(&out_aac, 0);
        }
    } else {
        sigaction(SIGPIPE, &(struct sigaction){{sigpipe_handler}}, NULL);

        // The fifos are opened in non blocking mode when a reader is there (see fifo_open())
        if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) {
            if (create_fifo(FIFO_NAME_LOW, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_LOW);
                return -6;
            }
            output_init_fifo(&out_low, FIFO_NAME_LOW, FIFO_SIZE_LOW, 1);
            fifo_open(&out_low);
        }
        if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) {
            if (create_fifo(FIFO_NAME_HIGH, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_HIGH);
                return -6;
            }
            output_init_fifo(&out_high, FIFO_NAME_HIGH, FIFO_SIZE_HIGH, 1);
            fifo_open(&out_high);
        }
        if (audio == 1) {
            if (create_fifo(FIFO_NAME_AAC, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_AAC);
                return -6;
            }
            output_init_fifo(&out_aac, FIFO_NAME_AAC, FIFO_SIZE_AAC, 0);
            fifo_open(&out_aac);
        }

        fprintf(stderr, "fifo started\n");
    }

    if (slot != SLOT_DISABLED) {
        slot_capture_loop();
    } else {

        // Polling mode: read the header of the buffer without locks

        memcpy(&i, addr + 16, sizeof(i));
        buf_idx = addr + buf_offset + i;
        buf_idx_cur = buf_idx;
        memcpy(&i, addr + 4, sizeof(i));
        buf_idx_end = buf_idx + i;
        if (buf_idx_end >= addr + buf_size) buf_idx_end -= (buf_size - buf_offset);
        buf_idx_end_prev = buf_idx_end;

        if (debug) fprintf(stderr, "starting capture main loop\n");

        // A frame that wraps around the end of the buffer is copied here
        wrap_buf = (unsigned char *) malloc(buf_size - buf_offset);
        // Every frame is copied here before the output (see process_frame())
        frame_copy = (unsigned char *) malloc(buf_size - buf_offset);
        if ((wrap_buf == NULL) || (frame_copy == NULL)) {
            fprintf(stderr, "Error - unable to allocate %d bytes\n", buf_size - buf_offset);
            exit_main = 1;
        }

        // Infinite loop
        while (!exit_main) {
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
                usleep(10000);
                continue;
            }

            buf_idx_cur = buf_idx_end_prev;
            bad_header = 0;
            more_frames = 0;
            i = 0;

            while (buf_idx_cur != buf_idx_end) {
                if (i == MAX_FRAMES_PER_POLL) {
                    // Many frames to read (e.g. the process was stalled): go on at the next poll, without waiting
                    more_frames = 1;
                    break;
                }
                cb2s_headercpy((unsigned char *) &fhs[i], buf_idx_cur, frame_header_size);
                // Check the len: the frame must end within the data written so far
                if ((fhs[i].len > (uint32_t) (buf_size - buf_offset - frame_header_size)) ||
                        ((int) fhs[i].len + frame_header_size > cb_dist(buf_idx_end, buf_idx_cur))) {
                    bad_header = 1;
                    break;
                }
                fhs_addr[i] = buf_idx_cur;
                buf_idx_cur = cb_move(buf_idx_cur, fhs[i].len + frame_header_size);
                i++;
            }

            n = i;
            if (bad_header && (n <= 1)) {
                // An invalid header (maybe the writer is updating it): read again at the next poll.
                // If it doesn't get valid, the sync is lost: restart from the current end.
                bad_polls++;
                if (bad_polls >= MAX_BAD_POLLS) {
                    fprintf(stderr, "%lld: warning - invalid frame header, sync lost, %d bytes skipped\n",
                            current_timestamp(), cb_dist(buf_idx_end, buf_idx_end_prev));
                    buf_idx_end_prev = buf_idx_end;
                    bad_polls = 0;
                }
                usleep(10000);
                continue;
            }
            bad_polls = 0;

            // Ignore last frame, it could be corrupted: it will be read at the next poll
            if (n > 1) {
                buf_idx_end_prev = fhs_addr[n - 1];
                n--;
            } else {
                usleep(10000);
                continue;
            }

            if (n > 0) {
                uint32_t expected = (last_counter + 1) & COUNTER_MASK;
                if (have_last_counter && (fhs[0].counter != expected)) {
                    fprintf(stderr, "%lld: warning - %u frame(s) lost\n",
                                current_timestamp(), (fhs[0].counter - expected) & COUNTER_MASK);
                }
                last_counter = fhs[n - 1].counter;
                have_last_counter = 1;
            }

            for (i = 0; i < n; i++) {
                // The payload is contiguous unless it wraps around the end of the buffer
                payload = cb_move(fhs_addr[i], frame_header_size);
                if (payload + fhs[i].len > addr + buf_size) {
                    cb2s_memcpy(wrap_buf, payload, fhs[i].len);
                    payload = wrap_buf;
                }
                process_frame(&fhs[i], payload, fhs_addr[i]);
            }

            if (!more_frames) usleep(25000);
        }

        free(wrap_buf);
        free(frame_copy);
    }

    // Unmap file from memory
    if (munmap(addr, buf_size) == -1) {
        if (debug) fprintf(stderr, "error - unmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);
    }

    sem_fshare_close();
    slot_release();

    if (fifo == 0) {
        // Don't free stdoutbuf: stdout still uses it and it's flushed by exit()
    } else {
        fifo_close(&out_low);
        fifo_close(&out_high);
        fifo_close(&out_aac);
    }

    if (debug) fprintf(stderr, "Program terminated\n");

    return 0;
}
