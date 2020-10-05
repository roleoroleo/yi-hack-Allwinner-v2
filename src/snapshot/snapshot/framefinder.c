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
 * Scans the buffer, finds the last h264 i-frame and saves the relative
 * position to /tmp/iframe.idx
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#define BUF_OFFSET 300
#define BUF_SIZE 1786156
#define FRAME_HEADER_SIZE 22

#define USLEEP 100000

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define I_FILE "/tmp/iframe.idx"

#define STATE_NONE 0                   /* Starting state*/
#define STATE_SPS_HIGH 1               /* Last nalu is SPS high res */
#define STATE_SPS_LOW 2                /* Last nalu is SPS low res */
#define STATE_PPS_HIGH 3               /* Last nalu is PPS high res */
#define STATE_PPS_LOW 4                /* Last nalu is PPS low res */
#define STATE_IDR_HIGH 5               /* Last nalu is IDR high res */
#define STATE_IDR_LOW 6                /* Last nalu is IDR low res */

typedef struct {
    int sps_addr;
    int sps_len;
    int pps_addr;
    int pps_len;
    int idr_addr;
    int idr_len;
} frame;

unsigned char IDR[]               = {0x65, 0xB8};
unsigned char NAL_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char PFR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char SPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char PPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char SPS_640X360[]       = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01};
unsigned char SPS_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC};

unsigned char *addr;                      /* Pointer to shared memory region (header) */
int debug = 0;                            /* Set to 1 to debug this .cpp */

int state = STATE_NONE;                   /* State of the state machine */

/* Locate a string in the circular buffer */
unsigned char * cb_memmem(unsigned char *src,
    int src_len, unsigned char *what, int what_len, unsigned char *buf, int buf_size)
{
    unsigned char *p;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, buf + buf_size - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(buf, src + src_len - buf, what, what_len);
        }
    }
    return p;
}

unsigned char * cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > addr + BUF_SIZE))
        buf -= (BUF_SIZE - BUF_OFFSET);
    if ((offset < 0) && (buf < addr + BUF_OFFSET))
        buf += (BUF_SIZE - BUF_OFFSET);

    return buf;
}

void writeFile(char *name, char *data, int len) {
    FILE *fp;

    fp = fopen(name, "w") ;
    if (fp == NULL) {
        if (debug) fprintf(stderr, "could not open file %s\n", name) ;
        return;
    }

    if (fwrite(data, sizeof(char), len, fp) != len) {
        if (debug) fprintf(stderr, "error writing to file %s\n", name) ;
    }
    fclose(fp) ;
}

unsigned int getFrameLen(unsigned char *buf, int offset)
{
    unsigned char *tmpbuf = buf;
    int ret = 0;

    tmpbuf = cb_move(tmpbuf, -(offset + FRAME_HEADER_SIZE));
    memcpy(&ret, tmpbuf, 4);
    tmpbuf = cb_move(tmpbuf, offset + FRAME_HEADER_SIZE);

    return ret;
}

int main(int argc, char **argv) {
    unsigned char *addrh;                     /* Pointer to h264 data */
    int sizeh;                                /* Size of h264 data */
    unsigned char *buf_idx_1, *buf_idx_2;
    unsigned char *buf_idx_w, *buf_idx_tmp;
    unsigned char *buf_idx_start, *buf_idx_end;
    FILE *fFid;
    uint32_t utmp[4];
    uint32_t utmp_old[4];

    int i;
    int sequence_size;

    frame hl_frame[2], hl_frame_old[2];

    // Opening an existing file
    fFid = fopen(BUFFER_FILE, "r") ;
    if ( fFid == NULL ) {
        if (debug) fprintf(stderr, "could not open file %s\n", BUFFER_FILE) ;
        return -1;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, BUF_SIZE, PROT_READ, MAP_SHARED, fileno(fFid), 0);
    if (addr == MAP_FAILED) {
        if (debug) fprintf(stderr, "error mapping file %s\n", BUFFER_FILE);
            return -2;
        }
    if (debug) fprintf(stderr, "mapping file %s, size %d, to %08x\n", BUFFER_FILE, BUF_SIZE, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "closing the file %s\n", BUFFER_FILE) ;
    fclose(fFid) ;

    // Define default vaules
    addrh = addr + BUF_OFFSET;
    sizeh = BUF_SIZE - BUF_OFFSET;

    buf_idx_1 = addrh;
    buf_idx_w = 0;

    state = STATE_NONE;

    // Infinite loop
    while (1) {
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx_w = addrh + i;
        if (debug) fprintf(stderr, "buf_idx_w: %08x\n", (unsigned int) buf_idx_w);

        buf_idx_tmp = cb_memmem(buf_idx_1, buf_idx_w - buf_idx_1, NAL_START, sizeof(NAL_START), addrh, sizeh);
        if (buf_idx_tmp == NULL) {
            usleep(USLEEP);
            continue;
        } else {
            buf_idx_1 = buf_idx_tmp;
        }
        if (debug) fprintf(stderr, "found buf_idx_1: %08x\n", (unsigned int) buf_idx_1);

        buf_idx_tmp = cb_memmem(buf_idx_1 + 1, buf_idx_w - (buf_idx_1 + 1), NAL_START, sizeof(NAL_START), addrh, sizeh);
        if (buf_idx_tmp == NULL) {
            usleep(USLEEP);
            continue;
        } else {
            buf_idx_2 = buf_idx_tmp;
        }
        if (debug) fprintf(stderr, "found buf_idx_2: %08x\n", (unsigned int) buf_idx_2);

        switch (state) {
            case STATE_SPS_HIGH:
                if (memcmp(buf_idx_1, PPS_START, sizeof(PPS_START)) == 0) {
                    state = STATE_PPS_HIGH;
                    if (debug) fprintf(stderr, "state = STATE_PPS_HIGH\n");
                    hl_frame[0].pps_addr = buf_idx_1 - addr;
                    hl_frame[0].pps_len = getFrameLen(buf_idx_1, 0);
                }
                break;
            case STATE_PPS_HIGH:
                if (memcmp(buf_idx_1, IDR_START, sizeof(IDR_START)) == 0) {
                    state = STATE_NONE;
                    if (debug) fprintf(stderr, "state = STATE_IDR_HIGH\n");
                    hl_frame[0].idr_addr = buf_idx_1 - addr;
                    hl_frame[0].idr_len = getFrameLen(buf_idx_1, 0);

                    if (memcmp(hl_frame, hl_frame_old, 2 * sizeof(frame)) != 0) {
                        writeFile(I_FILE, (unsigned char *) hl_frame, 2 * sizeof(frame));
                        memcpy(hl_frame_old, hl_frame, 2 * sizeof(frame));
                    }
                }
                break;
//            case STATE_IDR_HIGH:
//                state = STATE_NONE;
//                if (debug) fprintf(stderr, "state = STATE_NONE\n");
//                buf_idx_end = buf_idx_1;
//                sequence_size = buf_idx_end - buf_idx_start;
//                if (sequence_size < 0)
//                    sequence_size = (addrh + sizeh) - buf_idx_start + buf_idx_end - addrh;
//                // Write IDR address and size to the file
//                utmp[0] = (uint32_t) (buf_idx_start - addr);
//                utmp[1] = (uint32_t) sequence_size;
//                if (memcmp(utmp, utmp_old, sizeof(utmp)) != 0) {
//                    writeFile(I_FILE, (char *) utmp, sizeof(utmp));
//                    memcpy(utmp_old, utmp, sizeof(utmp));
//                }
//                break;

            case STATE_SPS_LOW:
                if (memcmp(buf_idx_1, PPS_START, sizeof(PPS_START)) == 0) {
                    state = STATE_PPS_LOW;
                    if (debug) fprintf(stderr, "state = STATE_PPS_LOW\n");
                    hl_frame[1].pps_addr = buf_idx_1 - addr;
                    hl_frame[1].pps_len = getFrameLen(buf_idx_1, 0);
                }
                break;
            case STATE_PPS_LOW:
                if (memcmp(buf_idx_1, IDR_START, sizeof(IDR_START)) == 0) {
                    state = STATE_NONE;
                    if (debug) fprintf(stderr, "state = STATE_IDR_LOW\n");
                    hl_frame[1].idr_addr = buf_idx_1 - addr;
                    hl_frame[1].idr_len = getFrameLen(buf_idx_1, 0);

                    if (memcmp(hl_frame, hl_frame_old, 2 * sizeof(frame)) != 0) {
                        writeFile(I_FILE, (unsigned char *) hl_frame, 2 * sizeof(frame));
                        memcpy(hl_frame_old, hl_frame, 2 * sizeof(frame));
                    }
                }
                break;
//            case STATE_IDR_LOW:
//                state = STATE_NONE;
//                if (debug) fprintf(stderr, "state = STATE_NONE\n");
//                buf_idx_end = buf_idx_1;
//                sequence_size = buf_idx_end - buf_idx_start;
//                if (sequence_size < 0)
//                    sequence_size = (addrh + sizeh) - buf_idx_start + buf_idx_end - addrh;
//                // Write IDR address and size to the file
//                utmp[2] = (uint32_t) (buf_idx_start - addr);
//                utmp[3] = (uint32_t) sequence_size;
//                if (memcmp(utmp, utmp_old, sizeof(utmp)) != 0) {
//                    writeFile(I_FILE, (char *) utmp, sizeof(utmp));
//                    memcpy(utmp_old, utmp, sizeof(utmp));
//                }
//                break;

            case STATE_NONE:
                if (memcmp(buf_idx_1, SPS_1920X1080, sizeof(SPS_1920X1080)) == 0) {
                    state = STATE_SPS_HIGH;
                    if (debug) fprintf(stderr, "state = STATE_SPS_HIGH\n");
                    hl_frame[0].sps_addr = buf_idx_1 - addr;
                    hl_frame[0].sps_len = getFrameLen(buf_idx_1, 6);
                } else if (memcmp(buf_idx_1, SPS_640X360, sizeof(SPS_640X360)) == 0) {
                    state = STATE_SPS_LOW;
                    if (debug) fprintf(stderr, "state = STATE_SPS_LOW\n");
                    hl_frame[1].sps_addr = buf_idx_1 - addr;
                    hl_frame[1].sps_len = getFrameLen(buf_idx_1, 6);
                }
                break;

            default:
                break;
        }

        buf_idx_1 = buf_idx_2;
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(addr, BUF_SIZE) == -1) {
        if (debug) fprintf(stderr, "error munmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, BUF_SIZE, addr);
    }

    return 0;
}
