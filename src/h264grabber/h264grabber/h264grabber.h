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

#ifndef _H264_GRABBER_H
#define _H264_GRABBER_H

#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/time.h>

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
#define READ_LOCK_FILE "/fshare_read_lock"
#define WRITE_LOCK_FILE "/fshare_write_lock"
#define READ_NOTIFY_FILE "/fshare_read_notify_%d"
#define SLOT_LOCK_NAME "fshare.slot%d"     // shared with rRTSPServer: they must not use the same slot

/**
 * DOC: fshare reader slots
 *
 * The writer and the stock readers share the buffer with a readers/writer
 * lock and 17 reader slots:
 *
 * - fshare_write_lock is held by the writer while it writes a frame, and by the
 *   readers as a group (taken by the first reader, released by the last one);
 * - fshare_read_lock protects the readers counter (first int of the buffer);
 * - every slot has a mask of the frame types it wants and a semaphore
 *   fshare_read_notify_N: after a frame of a wanted type is written, the writer
 *   sets has_data and, if the reader is waiting, posts the semaphore.
 *
 * The buffer starts with the fshare_header fields, then the 17 slots, then
 * the data area.
 *
 * - 368 bytes before the data area, 20 bytes slots, 28 or 24 bytes frame headers;
 * - 300 bytes before the data area, 16 bytes slots, 26 or 22 bytes frame headers.
 */
#define FSHARE_SLOTS 17
#define FSHARE_TYPE_MASK 0xFF00
#define SLOT_AUTO -2
#define SLOT_DISABLED -1
#define SLOT_WAIT_MS 1000                 // max wait for a notification, to check exit_main

/**
 * struct fshare_slot - Reader slot
 * @has_data: set by the writer when it writes a frame of a type in @mask,
 *            cleared by the reader when it has read all of them
 * @waiting: the reader is waiting on its notify semaphore
 * @last_counter: counter of the last frame read
 * @mask: bits 0-3: NAL type filter (0 = all), bits 8-15: frame types
 * @u1: unknown
 */
struct fshare_slot {
    int32_t has_data;
    int32_t waiting;
    uint32_t last_counter;
    uint16_t mask;
    uint16_t u1;
} __attribute__((__packed__));

/**
 * struct fshare_header - Fields at the start of the buffer
 * @readers: number of readers inside the buffer
 * @used: bytes of valid data
 * @u1: unknown
 * @write_pos: offset where the next frame is written
 * @read_pos: offset of the oldest frame
 * @last_time: time_ms of the newest frame
 * @last_counter: counter of the newest frame
 *
 * The FSHARE_SLOTS slots follow.
 */
struct fshare_header {
    int32_t readers;
    int32_t used;
    uint32_t u1;
    uint32_t write_pos;
    uint32_t read_pos;
    uint32_t last_time;
    uint32_t last_counter;
} __attribute__((__packed__));

#define FIFO_NAME_LOW "/tmp/h264_low_fifo"
#define FIFO_NAME_HIGH "/tmp/h264_high_fifo"
#define FIFO_NAME_AAC  "/tmp/aac_audio_fifo"

#define CODEC_NONE 0
#define CODEC_H264 264
#define CODEC_H265 265

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif
#ifndef F_GETPIPE_SZ
#define F_GETPIPE_SZ 1032
#endif

#define FIFO_SIZE_LOW  (128 * 1024)
#define FIFO_SIZE_HIGH (256 * 1024)
#define FIFO_SIZE_AAC  0                  // 0 = keep the default size
#define FIFO_REOPEN_MS 500                // retry to open a fifo without reader every 500 ms
#define FIFO_WRITE_TIMEOUT_MS 100         // max wait to complete a frame larger than the free space

/**
 * struct output_s - Output of a stream (stdout or fifo)
 * @name: fifo name, NULL if the output is stdout
 * @fp: stdout only
 * @fd: fifo only, -1 if not open
 * @req_size: requested pipe size (0 = default)
 * @size: actual pipe size
 * @is_video: video output (1) or audio output (0)
 * @drop_until_sync: drop frames until the next SPS (H264) or VPS (H265)
 * @next_open: next time to try to open the fifo (ms)
 */
struct output_s {
    const char *name;
    FILE *fp;
    int fd;
    int req_size;
    int size;
    int is_video;
    int drop_until_sync;
    long long next_open;
};

/**
 * DOC: Frame type
 *
 * - bit 0: IDR
 * - bit 1: SPS
 * - bit 2: PPS
 * - bit 3: VPS
 * - bit 4: ?
 * - bit 5: the data starts with an info prefix: 1st byte 1 -> 6 bytes
 *   {1, fps, width (16 bit), height (16 bit)}, 1st byte 2 -> 5 bytes
 * - bit 6: HEVC 265
 * - bit 7: ?
 * - bit 8: AAC
 * - bit 9: ?
 * - bit 10: main (high)
 * - bit 11: sub (low)
 * - bit 12: fast
 * - bit 13: ?
 * - bit 14: ?
 * - bit 15: ?
 */

/**
 * DOC: Frame headers
 *
 * Every frame in the data area of the buffer starts with a header.
 * Four sizes exist:
 *
 * - 28 bytes: v2 models with a 368 bytes buffer header;
 * - 26 bytes: v2 models with a 300 bytes buffer header;
 * - 24 bytes;
 * - 22 bytes: y20ga, y25ga, y30qa, first r30gb firmwares.
 */

/**
 * struct frame_header - The header fields used by the readers
 * @len: bytes of data after the header
 * @counter: sequence number of the frame in the buffer
 * @time_ms: UTC time of the frame in ms, truncated to 32 bits
 * @type: frame type, see "Frame type"
 * @stream_counter: sequence number of the frame in its stream
 *
 * Filled by cb2s_headercpy() from any frame header size.
 */
struct frame_header {
    uint32_t len;
    uint32_t counter;
    uint32_t time_ms;
    uint16_t type;
    uint16_t stream_counter;
} __attribute__((__packed__));

/**
 * struct frame_header_22 - Frame header of 22 bytes
 * @len: bytes of data after the header (info prefix included, see type bit 5)
 * @counter: sequence number of the frame in the buffer, all the streams
 *           together (it never takes the value 0)
 * @time_sec: UTC time of the frame in seconds (time())
 * @time_ms: UTC time of the frame in ms, truncated to 32 bits (it wraps every
 *           ~49.7 days)
 * @type: frame type, see "Frame type"
 * @stream_counter: sequence number of the frame in its stream (it wraps at 65536)
 * @cfg: a byte of the configuration, the same in all the frames (meaning
 *       unknown, 0 on the tested cameras)
 */
struct frame_header_22 {
    uint32_t len;
    uint32_t counter;
    uint32_t time_sec;
    uint32_t time_ms;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t cfg;
} __attribute__((__packed__));

/**
 * struct frame_header_24 - Frame header of 24 bytes
 * @len: see struct frame_header_22
 * @counter: see struct frame_header_22
 * @time_sec: see struct frame_header_22
 * @time_ms: see struct frame_header_22
 * @type: see struct frame_header_22
 * @stream_counter: see struct frame_header_22
 * @cfg: see struct frame_header_22
 * @ref_lost: 1 if dropped, to make room
 */
struct frame_header_24 {
    uint32_t len;
    uint32_t counter;
    uint32_t time_sec;
    uint32_t time_ms;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t cfg;
    uint16_t ref_lost;
} __attribute__((__packed__));

/**
 * struct frame_header_26 - Frame header of 26 bytes
 * @len: see struct frame_header_22
 * @counter: see struct frame_header_22
 * @time_sec: see struct frame_header_22
 * @time_min: time_sec / 60, only on the frames of a key frame group (SPS, PPS,
 *            IDR), 0 on the others
 * @time_ms: see struct frame_header_22
 * @type: see struct frame_header_22
 * @stream_counter: see struct frame_header_22
 * @cfg: see struct frame_header_22
 */
struct frame_header_26 {
    uint32_t len;
    uint32_t counter;
    uint32_t time_sec;
    uint32_t time_min;
    uint32_t time_ms;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t cfg;
} __attribute__((__packed__));

/**
 * struct frame_header_28 - Frame header of 28 bytes
 * @len: see struct frame_header_22
 * @counter: see struct frame_header_22
 * @time_sec: see struct frame_header_22
 * @time_min: see struct frame_header_26
 * @time_ms: see struct frame_header_22
 * @type: see struct frame_header_22
 * @stream_counter: see struct frame_header_22
 * @cfg: see struct frame_header_22
 * @ref_lost: see struct frame_header_24
 */
struct frame_header_28 {
    uint32_t len;
    uint32_t counter;
    uint32_t time_sec;
    uint32_t time_min;
    uint32_t time_ms;
    uint16_t type;
    uint16_t stream_counter;
    uint16_t cfg;
    uint16_t ref_lost;
} __attribute__((__packed__));

struct stream_type_s {
    int codec_low;
    int codec_high;
    int sps_type_low;
    int sps_type_high;
};

#endif
