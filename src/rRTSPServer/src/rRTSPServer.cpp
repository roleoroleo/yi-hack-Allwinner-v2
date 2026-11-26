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
#include <utility>

#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <signal.h>
#include <semaphore.h>

#include "rRTSPServer.h"

#define MAX_FRAMES_PER_POLL 64            // frames parsed from the shared buffer in a single poll
#define MAX_BAD_POLLS 10                  // consecutive polls with an invalid header before a resync
#define COUNTER_MASK 0xFFFFFFFF           // the frame counter in the header is 32 bit

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

int model;
int resolution;
int audio;
int port;
int sps_timing_info;
int debug;

int slot;                                   // reader slot, SLOT_DISABLED for the polling mode
int slot_size;                              // size of a slot in the layout of this buffer
int slot_lock_fd = -1;
sem_t *sem_fshare_read_lock = SEM_FAILED;
sem_t *sem_fshare_write_lock = SEM_FAILED;
sem_t *sem_fshare_notify = SEM_FAILED;
volatile sig_atomic_t exit_main = 0;   // SIGTERM or SIGINT received (slot mode only)

int frame_counter_last_valid_low = -1;
int frame_counter_last_valid_high = -1;
int frame_counter_last_valid_audio = -1;

cb_input_buffer input_buffer;
output_queue output_queue_high;
output_queue output_queue_low;
output_queue output_queue_audio;

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

long long current_timestamp()
{
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

/*
 * Map a camera frame timestamp (uint32 ms since boot, wraps ~every 49 days,
 * not wall-clock) to a real wall-clock presentation time suitable for RTP/RTCP.
 *
 * The anchor (camera_ms <-> wall clock) is held by the CALLER and is therefore
 * PER-SOURCE: video and audio each keep their own anchor. This avoids coupling
 * two independent (possibly different-based) camera clocks through one shared
 * anchor, which could otherwise make one stream's frame precede the anchor and
 * underflow the uint32 subtraction into a ~49-day-in-the-future timestamp
 * (freezing that stream, and the client's A/V sync with it).
 *
 * We also (re)anchor whenever a frame precedes the current anchor (out-of-order
 * or a clock reset), using a SIGNED delta, so a backwards timestamp re-syncs to
 * "now" instead of jumping far into the future.
 *
 * Presentation times become real wall-clock (correct RTCP), and within a source
 * the uint32 subtraction still handles the camera-clock wrap. A/V sync is
 * preserved up to the (small) difference in per-stream pipeline latency.
 *
 * Must be called only from the (single-threaded) live555 event loop.
 */
void frametime_to_presentation(uint32_t frame_time, struct timeval *pt,
                               bool *have_anchor, struct timeval *anchor_wall, uint32_t *anchor_ft) {
    // Re-anchor on the first frame, or if this frame precedes the anchor.
    if (*have_anchor && (int32_t)(frame_time - *anchor_ft) < 0) {
        *have_anchor = false;
    }
    if (!*have_anchor) {
        gettimeofday(anchor_wall, NULL);
        *anchor_ft = frame_time;
        *have_anchor = true;
        *pt = *anchor_wall;
        return;
    }

    uint32_t delta_ms = frame_time - *anchor_ft;   // modular: handles the wrap
    uint64_t usec = (uint64_t) anchor_wall->tv_usec + (uint64_t)(delta_ms % 1000) * 1000;
    pt->tv_sec  = anchor_wall->tv_sec + (time_t)(delta_ms / 1000) + (time_t)(usec / 1000000);
    pt->tv_usec = (long)(usec % 1000000);
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

// Bytes from "from" to "to" going forward in the circular buffer
int cb_dist(unsigned char *to, unsigned char *from)
{
    int d = to - from;

    if (d < 0) d += (input_buffer.size - input_buffer.offset);
    return d;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf >= input_buffer.buffer + input_buffer.size))
        buf -= (input_buffer.size - input_buffer.offset);
    if ((offset < 0) && (buf < input_buffer.buffer + input_buffer.offset))
        buf += (input_buffer.size - input_buffer.offset);

    return buf;
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
    if (fp == NULL) {
        // Unsupported header size: never leave the caller's header uninitialized
        std::memset(fh, 0, sizeof(struct frame_header));
        return;
    }

    if (src + n > input_buffer.buffer + input_buffer.size) {
        std::memcpy(fp, src, input_buffer.buffer + input_buffer.size - src);
        std::memcpy(fp + (input_buffer.buffer + input_buffer.size - src), input_buffer.buffer + input_buffer.offset, n - (input_buffer.buffer + input_buffer.size - src));
    } else {
        std::memcpy(fp, src, n);
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
    struct fshare_header *h = (struct fshare_header *) input_buffer.buffer;

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
    struct fshare_header *h = (struct fshare_header *) input_buffer.buffer;

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
    return (struct fshare_slot *) (input_buffer.buffer + sizeof(struct fshare_header) + s * slot_size);
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
    struct fshare_header *h = (struct fshare_header *) input_buffer.buffer;

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
    struct fshare_header *h = (struct fshare_header *) input_buffer.buffer;
    struct fshare_slot *sl = slot_ptr(s);
    unsigned char *ring = input_buffer.buffer + input_buffer.offset;
    uint32_t ring_size = input_buffer.size - input_buffer.offset;
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
 * stop_handler() - Handler of SIGTERM and SIGINT
 * @signal: signal number
 */
void stop_handler(int signal)
{
    exit_main = 1;
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
    return (len >= ref_len) && (std::memcmp(data, ref, ref_len) == 0);
}

/**
 * process_frame() - Handle one frame and push it to its output queue
 * @fh: header of the frame
 * @data: payload of the frame (after the frame header), contiguous in memory,
 *        fh->len bytes long
 * @ring_header: address of the frame header in the circular buffer in polling
 *               mode, NULL in slot mode
 *
 * In polling mode @data may still be in the circular buffer (if the frame
 * doesn't wrap around its end), and after the copy the header is checked
 * again, because the writer may have overwritten the frame. In slot mode
 * @data is a copy made with the readers lock, so it can't change.
 */
void process_frame(struct frame_header *fh, unsigned char *data, unsigned char *ring_header)
{
    int frame_type;
    int frame_len = fh->len;
    int frame_counter;
    uint32_t frame_time;
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
            if ((debug & 1) && (stream_type.codec_low != CODEC_NONE)) fprintf(stderr, "%lld: h264 in - low - codec type is %d - sps type is %d\n",
                    current_timestamp(), stream_type.codec_low.load(), stream_type.sps_type_low);
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
            if ((debug & 1) && (stream_type.codec_high != CODEC_NONE)) fprintf(stderr, "%lld: h264 in - high - codec type is %d - sps type is %d\n",
                    current_timestamp(), stream_type.codec_high.load(), stream_type.sps_type_high);
        }
    }

    write_enable = 1;
    frame_counter = fh->stream_counter;
    frame_time = fh->time_ms;

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
            if (debug & 1) fprintf(stderr, "%lld: h26x in - warning - %d low res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                        current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_low - 1) % 65536, frame_counter, frame_counter_last_valid_low);
        } else if (debug & 1) {
            fprintf(stderr, "%lld: h26x in - %s detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                    current_timestamp(), (fh->type & 0x0002) ? "SPS" : "frame", frame_len, frame_counter,
                    frame_counter_last_valid_low, frame_type);
        }
        frame_counter_last_valid_low = frame_counter;
    } else if ((frame_type == TYPE_HIGH) && ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))) {
        if ((65536 + frame_counter - frame_counter_last_valid_high) % 65536 > 1) {
            if (debug & 1) fprintf(stderr, "%lld: h26x in - warning - %d high res frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                        current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_high - 1) % 65536, frame_counter, frame_counter_last_valid_high);
        } else if (debug & 1) {
            fprintf(stderr, "%lld: h26x in - %s detected - frame_len: %d - frame_counter: %d - frame_counter_last_valid: %d - resolution: %d\n",
                    current_timestamp(), (fh->type & 0x0002) ? "SPS" : "frame", frame_len, frame_counter,
                    frame_counter_last_valid_high, frame_type);
        }
        frame_counter_last_valid_high = frame_counter;
    } else if ((frame_type == TYPE_AAC) && (audio == 2)) {
        if ((65536 + frame_counter - frame_counter_last_valid_audio) % 65536 > 1) {
            if (debug & 2) fprintf(stderr, "%lld: aac in - warning - %d AAC frame(s) lost - frame_counter: %d - frame_counter_last_valid: %d\n",
                        current_timestamp(), (65536 + frame_counter - frame_counter_last_valid_audio - 1) % 65536, frame_counter, frame_counter_last_valid_audio);
        } else if (debug & 2) {
            fprintf(stderr, "%lld: aac in - frame detected - frame_len: %d - frame_counter: %d - audio AAC\n",
                        current_timestamp(), frame_len, frame_counter);
        }
        frame_counter_last_valid_audio = frame_counter;
    } else {
        write_enable = 0;
    }

    if (!write_enable) return;

    // Send the frame to the ouput buffer
    output_queue *queue;
    if ((frame_type == TYPE_LOW) && (resolution != RESOLUTION_HIGH) && (stream_type.codec_low != CODEC_NONE)) {
        queue = &output_queue_low;
    } else if ((frame_type == TYPE_HIGH) && (resolution != RESOLUTION_LOW) && (stream_type.codec_high != CODEC_NONE)) {
        queue = &output_queue_high;
    } else if ((frame_type == TYPE_AAC) && (audio != 0)) {
        queue = &output_queue_audio;
    } else {
        return;
    }

    if (debug & 3) fprintf(stderr, "%lld: h264/aac in - frame_len: %d - cb_current->size: %d\n", current_timestamp(), frame_len, queue->frame_queue.size());

    if (frame_len > 0) {
        unsigned char *copy_src = data;

        // Overwrite SPS with one that contains timing info at 20 fps
        if (sps_timing_info && (fh->type & 0x0002)) {
            unsigned char *ti;
            int ti_len = 0;

            ti = get_sps_ti(&ti_len, frame_type);
            if (ti != NULL) {
                copy_src = ti;
                frame_len = ti_len;
            }
        }

        output_frame of;
        of.frame.resize(frame_len);
        std::memcpy(of.frame.data(), copy_src, frame_len);

        if (ring_header != NULL) {
            // The writer may have overwritten this frame while we parsed and
            // copied it. It rewrites the ring in order, so it reaches the
            // header before the data: if the header is unchanged, the copy is good.
            struct frame_header fh_check;
            __sync_synchronize();
            cb2s_headercpy((unsigned char *) &fh_check, ring_header, frame_header_size);
            if ((fh_check.counter != fh->counter) || (fh_check.len != fh->len)) {
                fprintf(stderr, "%lld: h264/aac in - warning - frame overwritten during copy, frame dropped\n", current_timestamp());
                return;
            }
        }

        of.counter = frame_counter;
        of.time = frame_time;
        // Lock only for the queue push + overflow trim
        pthread_mutex_lock(&(queue->mutex));
        queue->frame_queue.push(std::move(of));
        while (queue->frame_queue.size() > MAX_QUEUE_SIZE) queue->frame_queue.pop();
        pthread_mutex_unlock(&(queue->mutex));

        if (debug & 3) {
            fprintf(stderr, "%lld: h264/aac in - frame_len: %d - frame_counter: %d - resolution: %d\n", current_timestamp(), frame_len, frame_counter, frame_type);
        }
    } else {
        // Guard against a corrupt/short frame
        fprintf(stderr, "%lld: h264/aac in - warning - invalid frame_len %d, frame dropped\n", current_timestamp(), frame_len);
        return;
    }
}

/**
 * slot_capture_loop() - Capture loop with a reader slot
 *
 * Wait for the notifications of the writer and copy the new frames out of the
 * buffer while holding the readers lock, then queue them.
 * It returns only if the stop was requested (SIGTERM or SIGINT).
 */
void slot_capture_loop()
{
    unsigned char *stage;
    unsigned char *data[MAX_FRAMES_PER_POLL];
    struct frame_header fhs[MAX_FRAMES_PER_POLL];
    int stage_size = input_buffer.size - input_buffer.offset;
    uint16_t mask = 0, mask_detect;
    bool detected = false;
    int i, n, more;

    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH)) mask |= 0x0800;
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH)) mask |= 0x0400;
    if (audio == 2) mask |= 0x0100;
    // main() waits until the codecs of both video streams are known:
    // read both of them until their SPS have been seen
    mask_detect = mask | 0x0800 | 0x0400;

    // A single frame is never larger than the data area
    stage = (unsigned char *) malloc(stage_size);
    if (stage == NULL) {
        fprintf(stderr, "%lld: capture - error - unable to allocate %d bytes\n", current_timestamp(), stage_size);
        exit(EXIT_FAILURE);
    }

    // Drop the notifications left by a previous reader of this slot
    while (sem_trywait(sem_fshare_notify) == 0);

    slot_set_mask(slot, mask_detect);
    slot_seek_newest(slot);
    if (debug & 3) fprintf(stderr, "%lld: capture - reading from slot %d, mask 0x%04x\n", current_timestamp(), slot, mask_detect);

    while (!exit_main) {
        if (!detected && (stream_type.codec_low != CODEC_NONE) && (stream_type.codec_high != CODEC_NONE)) {
            detected = true;
            if (mask != mask_detect) {
                slot_set_mask(slot, mask);
                if (debug & 3) fprintf(stderr, "%lld: capture - stream types detected, slot mask 0x%04x\n", current_timestamp(), mask);
            }
        }
        if (!slot_wait(slot)) continue;     // timeout or signal
        do {
            n = slot_read(stage, fhs, data, &more, slot, stage_size, MAX_FRAMES_PER_POLL);
            // The writer is free again: now the frames can be queued
            for (i = 0; i < n; i++) process_frame(&fhs[i], data[i], NULL);
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
    if (input_buffer.offset == FRAME_OFFSET_AUTODETECT) {
        std::memcpy(&i, input_buffer.buffer + FRAME_OFFSET_TRY_1, sizeof(i));
        if (i != 0)
            input_buffer.offset = FRAME_OFFSET_TRY_1;
        else
            input_buffer.offset = FRAME_OFFSET_TRY_2;
    }

    // Autodetect header size if not defined
    if ((frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) && (debug & 3)) fprintf(stderr, "%lld: capture - detecting frame header size\n", current_timestamp());
    while (frame_header_size == FRAME_HEADER_SIZE_AUTODETECT) {
        // std::search returns the end of the range (never NULL) when nothing is found
        unsigned char *search_end = input_buffer.buffer + input_buffer.size;
        header_a2 = (unsigned char *) std::search(input_buffer.buffer + input_buffer.offset, search_end, PPS4_START, PPS4_START + sizeof(PPS4_START));
        if ((header_a2 != search_end) && (header_a2 - 40 > input_buffer.buffer + input_buffer.offset)) {
            unsigned char *window_start = cb_move(header_a2, -40);
            header_a1 = (unsigned char *) std::search(window_start, window_start + 40, PPS4_HEADER, PPS4_HEADER + sizeof(PPS4_HEADER));
            if (header_a1 != window_start + 40) {
                frame_header_size = header_a2 - header_a1;
                if (frame_header_size < 0)
                        frame_header_size += (input_buffer.size - input_buffer.offset);
                // Accept only the header layouts that cb2s_headercpy() can decode
                if ((frame_header_size != (int) sizeof(struct frame_header_22)) &&
                        (frame_header_size != (int) sizeof(struct frame_header_24)) &&
                        (frame_header_size != (int) sizeof(struct frame_header_26)) &&
                        (frame_header_size != (int) sizeof(struct frame_header_28))) {
                    if (debug & 3) fprintf(stderr, "%lld: capture - unsupported frame header size %d, retrying\n", current_timestamp(), frame_header_size);
                    frame_header_size = FRAME_HEADER_SIZE_AUTODETECT;
                }
            }
        }
        usleep(1000);
    }
    if (debug & 3) fprintf(stderr, "%lld: capture - frame offset = %d\n", current_timestamp(), input_buffer.offset);
    if (debug & 3) fprintf(stderr, "%lld: capture - frame header size = %d\n", current_timestamp(), frame_header_size);
}

void *capture(void *ptr)
{
    unsigned char *buf_idx, *buf_idx_cur, *buf_idx_end, *buf_idx_end_prev;
    int fshm;

    int i, n;
    int bad_header = 0;
    int bad_polls = 0;
    int more_frames;

    struct frame_header fhs[MAX_FRAMES_PER_POLL];
    unsigned char* fhs_addr[MAX_FRAMES_PER_POLL];
    unsigned char *payload, *wrap_buf;
    uint32_t last_counter = 0;
    int have_last_counter = 0;

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

    detect_layout();

    // Choose the reader slot
    if (slot != SLOT_DISABLED) {
        slot_size = fshare_slot_size(input_buffer.offset, frame_header_size);
        if ((slot_size == 0) || (input_buffer.size <= input_buffer.offset)) {
            fprintf(stderr, "warning - reader slots not supported by this model, polling the buffer\n");
            slot = SLOT_DISABLED;
        } else if (slot == SLOT_AUTO) {
            int first, second;

            slot_auto_choice(&first, &second, (model == Y20GA) || (model == Y25GA) || (model == Y30QA) || (model == Y501GC));
            if (slot_reserve(first) == 0) {
                slot = first;
            } else if (slot_reserve(second) == 0) {
                slot = second;
            } else {
                fprintf(stderr, "warning - slots %d and %d are used by other processes, polling the buffer\n", first, second);
                slot = SLOT_DISABLED;
            }
        } else if (slot_reserve(slot) != 0) {
            fprintf(stderr, "warning - slot %d is used by another process, polling the buffer\n", slot);
            slot = SLOT_DISABLED;
        }
    }
    if ((slot != SLOT_DISABLED) && (sem_fshare_open(slot) != 0)) {
        fprintf(stderr, "warning - could not open semaphores, polling the buffer\n");
        sem_fshare_close();
        slot_release();
        slot = SLOT_DISABLED;
    }
    if (slot != SLOT_DISABLED) {
        // The capture thread must leave the slot clean before the process
        // terminates: a reader killed while it holds the readers lock would
        // block the writer, and with it every stream of the camera
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = stop_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        fprintf(stderr, "using reader slot %d\n", slot);
    } else if (debug) {
        fprintf(stderr, "polling the buffer\n");
    }

    if (slot != SLOT_DISABLED) {
        slot_capture_loop();

        // Stop requested: the slot is clean, now the process can terminate
        sem_fshare_close();
        slot_release();
        fprintf(stderr, "%lld: capture - stop requested, slot %d released\n", current_timestamp(), slot);
        _exit(EXIT_SUCCESS);
    }

    // Polling mode: read the header of the buffer without locks

    std::memcpy(&i, input_buffer.buffer + 16, sizeof(i));
    buf_idx = input_buffer.buffer + input_buffer.offset + i;
    buf_idx_cur = buf_idx;
    std::memcpy(&i, input_buffer.buffer + 4, sizeof(i));
    buf_idx_end = buf_idx + i;
    if (buf_idx_end >= input_buffer.buffer + input_buffer.size) buf_idx_end -= (input_buffer.size - input_buffer.offset);
    buf_idx_end_prev = buf_idx_end;

    if (debug & 3) fprintf(stderr, "%lld: capture - starting capture main loop\n", current_timestamp());

    // A frame that wraps around the end of the buffer is copied here
    wrap_buf = (unsigned char *) malloc(input_buffer.size - input_buffer.offset);
    if (wrap_buf == NULL) {
        fprintf(stderr, "%lld: capture - error - unable to allocate %d bytes\n", current_timestamp(), input_buffer.size - input_buffer.offset);
        exit(EXIT_FAILURE);
    }

    // Infinite loop
    while (1) {
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
            if (debug & 3) fprintf(stderr, "%lld: capture - buf_idx_end == buf_idx_end_prev\n", current_timestamp());
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
                if (debug & 3) fprintf(stderr, "%lld: capture - more than %d frames in one poll\n", current_timestamp(), MAX_FRAMES_PER_POLL);
                break;
            }
            cb2s_headercpy((unsigned char *) &fhs[i], buf_idx_cur, frame_header_size);
            // Check the len: the frame must end within the data written so far
            if ((fhs[i].len > (uint32_t) (input_buffer.size - input_buffer.offset - frame_header_size)) ||
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
                fprintf(stderr, "%lld: capture - warning - invalid frame header, sync lost, %d bytes skipped\n",
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
            if (debug & 3) fprintf(stderr, "%lld: capture - ! n > 1\n", current_timestamp());
            usleep(10000);
            continue;
        }

        if (n > 0) {
            uint32_t expected = (last_counter + 1) & COUNTER_MASK;
            if (have_last_counter && (fhs[0].counter != expected)) {
                fprintf(stderr, "%lld: capture - warning - %u frame(s) lost\n",
                            current_timestamp(), (fhs[0].counter - expected) & COUNTER_MASK);
            }
            last_counter = fhs[n - 1].counter;
            have_last_counter = 1;
        }

        for (i = 0; i < n; i++) {
            // The payload is contiguous unless it wraps around the end of the buffer
            payload = cb_move(fhs_addr[i], frame_header_size);
            if (payload + fhs[i].len > input_buffer.buffer + input_buffer.size) {
                cb2s_memcpy(wrap_buf, payload, fhs[i].len);
                payload = wrap_buf;
            }
            process_frame(&fhs[i], payload, fhs_addr[i]);
        }

        if (!more_frames) usleep(10000);
    }

    // Unreacheable path

    free(wrap_buf);

    // Unmap file from memory
    if (munmap(input_buffer.buffer, input_buffer.size) == -1) {
        fprintf(stderr, "%lld: capture - error - unmapping file\n", current_timestamp());
    } else {
        if (debug & 3) fprintf(stderr, "%lld: capture - unmapping file %s, size %d, from %08x\n", current_timestamp(), input_buffer.filename, input_buffer.size, (unsigned int) input_buffer.buffer);
    }

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
    fprintf(stderr, "\t-S SLOT,  --slot SLOT\n");
    fprintf(stderr, "\t\treader slot 0-16, -1 to poll the buffer, -2 auto (default -1, auto = 15, else 14; v1 models: 16, else 15)\n");
    fprintf(stderr, "\t-d DEBUG, --debug DEBUG\n");
    fprintf(stderr, "\t\t0 none, +1 video in, +2 audio in, +4 video out, +8 audio out, +16 backchannel\n");
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
    slot = SLOT_DISABLED;
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
            {"slot",  required_argument, 0, 'S'},
            {"debug",  required_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "m:r:a:b:p:su:w:S:d:h",
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

        case 'S':
            slot = atoi(optarg);
            if ((slot < SLOT_AUTO) || (slot >= FSHARE_SLOTS)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
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
            if ((debug < 0) || (debug > 31)) {
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

    str = getenv("RRTSP_SLOT");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= SLOT_AUTO) && (nm < FSHARE_SLOTS)) {
        slot = nm;
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
