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

#define BUF_OFFSET_Y21GA 368
#define BUF_SIZE_Y21GA 1786224
#define FRAME_HEADER_SIZE_Y21GA 28
#define DATA_OFFSET_Y21GA 4
#define LOWRES_BYTE_Y21GA 8
#define HIGHRES_BYTE_Y21GA 4

#define BUF_OFFSET_Y211GA 368
#define BUF_SIZE_Y211GA 1786224
#define FRAME_HEADER_SIZE_Y211GA 28
#define DATA_OFFSET_Y211GA 4
#define LOWRES_BYTE_Y211GA 8
#define HIGHRES_BYTE_Y211GA 4

#define BUF_OFFSET_H30GA 368
#define BUF_SIZE_H30GA 1786224
#define FRAME_HEADER_SIZE_H30GA 28
#define DATA_OFFSET_H30GA 4
#define LOWRES_BYTE_H30GA 8
#define HIGHRES_BYTE_H30GA 4

#define BUF_OFFSET_R30GB 300
#define BUF_SIZE_R30GB 1786156
#define FRAME_HEADER_SIZE_R30GB 22
#define DATA_OFFSET_R30GB 0
#define LOWRES_BYTE_R30GB 8
#define HIGHRES_BYTE_R30GB 4

#define BUF_OFFSET_R40GA 300
#define BUF_SIZE_R40GA 1786156
#define FRAME_HEADER_SIZE_R40GA 26
#define DATA_OFFSET_R40GA 4
#define LOWRES_BYTE_R40GA 8
#define HIGHRES_BYTE_R40GA 4

#define BUF_OFFSET_H51GA 368
#define BUF_SIZE_H51GA 524656
#define FRAME_HEADER_SIZE_H51GA 28
#define DATA_OFFSET_H51GA 4
#define LOWRES_BYTE_H51GA 8
#define HIGHRES_BYTE_H51GA 4

#define BUF_OFFSET_H52GA 368
#define BUF_SIZE_H52GA 1048944
#define FRAME_HEADER_SIZE_H52GA 28
#define DATA_OFFSET_H52GA 4
#define LOWRES_BYTE_H52GA 8
#define HIGHRES_BYTE_H52GA 4

#define BUF_OFFSET_H60GA 368
#define BUF_SIZE_H60GA 1048944
#define FRAME_HEADER_SIZE_H60GA 28
#define DATA_OFFSET_H60GA 4
#define LOWRES_BYTE_H60GA 8
#define HIGHRES_BYTE_H60GA 4

#define BUF_OFFSET_Y28GA 368
#define BUF_SIZE_Y28GA 1048944
#define FRAME_HEADER_SIZE_Y28GA 28
#define DATA_OFFSET_Y28GA 4
#define LOWRES_BYTE_Y28GA 8
#define HIGHRES_BYTE_Y28GA 4

#define BUF_OFFSET_Q321BR_LSX 300
#define BUF_SIZE_Q321BR_LSX 524588
#define FRAME_HEADER_SIZE_Q321BR_LSX 26
#define DATA_OFFSET_Q321BR_LSX 4
#define LOWRES_BYTE_Q321BR_LSX 8
#define HIGHRES_BYTE_Q321BR_LSX 4

#define BUF_OFFSET_QG311R 300
#define BUF_SIZE_QG311R 524588
#define FRAME_HEADER_SIZE_QG311R 26
#define DATA_OFFSET_QG311R 4
#define LOWRES_BYTE_QG311R 8
#define HIGHRES_BYTE_QG311R 4

#define USLEEP 100000

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define I_FILE "/tmp/iframe.idx"

#define STATE_NONE 0                   /* Starting state*/
#define STATE_SPS_HIGH 1               /* Last nalu is SPS high res */
#define STATE_SPS_LOW 2                /* Last nalu is SPS low res */
#define STATE_PPS_HIGH 3               /* Last nalu is PPS high res */
#define STATE_PPS_LOW 4                /* Last nalu is PPS low res */
#define STATE_VPS_HIGH 5               /* Last nalu is VPS high res */
#define STATE_VPS_LOW 6                /* Last nalu is VPS low res */
#define STATE_IDR_HIGH 7               /* Last nalu is IDR high res */
#define STATE_IDR_LOW 8                /* Last nalu is IDR low res */

typedef struct {
    int sps_addr;
    int sps_len;
    int pps_addr;
    int pps_len;
    int vps_addr;
    int vps_len;
    int idr_addr;
    int idr_len;
} frame;

int buf_offset;
int buf_size;
int frame_header_size;
int data_offset;
int lowres_byte;
int highres_byte;

unsigned char IDR4[]               = {0x65, 0xB8};
unsigned char NALx_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char IDR5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x26};
unsigned char PFR4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char PFR5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x02};
unsigned char SPS4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char SPS5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x42};
unsigned char PPS4_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char PPS5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x44};
unsigned char VPS5_START[]         = {0x00, 0x00, 0x00, 0x01, 0x40};

// Not necessary
//unsigned char SPS_640X360[]       = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
//                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
//                                       0x01, 0x01, 0x02};
//unsigned char SPS_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
//                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
//                                       0x04, 0x04, 0x04, 0x08};
//unsigned char SPS_R40GA_1920X1080[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20,
//                                       0xAC, 0x2C, 0xA8, 0x07, 0x80, 0x22, 0x5E, 0x59,
//                                       0xB8, 0x08, 0x08, 0x08, 0x10};
//unsigned char SPS_2304X1296[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
//                                       0x96, 0x54, 0x01, 0x20, 0x05, 0x19, 0x37, 0x01,
//                                       0x01, 0x01, 0x02};

unsigned char *addr;                      /* Pointer to shared memory region (header) */
int debug = 0;                            /* Set to 1 to debug this .cpp */

int state = STATE_NONE;                   /* State of the state machine */

/* Locate a string in the circular buffer */
unsigned char * cb_memmem(unsigned char *src,
    int src_len, unsigned char *what, int what_len, unsigned char *buffer, int buffer_size)
{
    unsigned char *p;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, buffer + buffer_size - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(buffer, src + src_len - buffer, what, what_len);
        }
    }
    return p;
}

unsigned char * cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > addr + buf_size))
        buf -= (buf_size - buf_offset);
    if ((offset < 0) && (buf < addr + buf_offset))
        buf += (buf_size - buf_offset);

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

    tmpbuf = cb_move(tmpbuf, -(offset + frame_header_size));
    memcpy(&ret, tmpbuf, 4);
    tmpbuf = cb_move(tmpbuf, offset + frame_header_size);

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

    buf_offset = BUF_OFFSET_Y21GA;
    buf_size = BUF_SIZE_Y21GA;
    frame_header_size = FRAME_HEADER_SIZE_Y21GA;
    data_offset = DATA_OFFSET_Y21GA;
    lowres_byte = LOWRES_BYTE_Y21GA;
    highres_byte = HIGHRES_BYTE_Y21GA;

    if (argc > 1) {
        if (strcasecmp("y21ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_Y21GA;
            buf_size = BUF_SIZE_Y21GA;
            frame_header_size = FRAME_HEADER_SIZE_Y21GA;
            data_offset = DATA_OFFSET_Y21GA;
            lowres_byte = LOWRES_BYTE_Y21GA;
            highres_byte = HIGHRES_BYTE_Y21GA;
        } else if (strcasecmp("y211ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_Y211GA;
            buf_size = BUF_SIZE_Y211GA;
            frame_header_size = FRAME_HEADER_SIZE_Y211GA;
            data_offset = DATA_OFFSET_Y211GA;
            lowres_byte = LOWRES_BYTE_Y211GA;
            highres_byte = HIGHRES_BYTE_Y211GA;
        } else if (strcasecmp("h30ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_H30GA;
            buf_size = BUF_SIZE_H30GA;
            frame_header_size = FRAME_HEADER_SIZE_H30GA;
            data_offset = DATA_OFFSET_H30GA;
            lowres_byte = LOWRES_BYTE_H30GA;
            highres_byte = HIGHRES_BYTE_H30GA;
        } else if (strcasecmp("r30gb", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_R30GB;
            buf_size = BUF_SIZE_R30GB;
            frame_header_size = FRAME_HEADER_SIZE_R30GB;
            data_offset = DATA_OFFSET_R30GB;
            lowres_byte = LOWRES_BYTE_R30GB;
            highres_byte = HIGHRES_BYTE_R30GB;
        } else if (strcasecmp("r40ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_R40GA;
            buf_size = BUF_SIZE_R40GA;
            frame_header_size = FRAME_HEADER_SIZE_R40GA;
            data_offset = DATA_OFFSET_R40GA;
            lowres_byte = LOWRES_BYTE_R40GA;
            highres_byte = HIGHRES_BYTE_R40GA;
        } else if (strcasecmp("h51ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_H51GA;
            buf_size = BUF_SIZE_H51GA;
            frame_header_size = FRAME_HEADER_SIZE_H51GA;
            data_offset = DATA_OFFSET_H51GA;
            lowres_byte = LOWRES_BYTE_H51GA;
            highres_byte = HIGHRES_BYTE_H51GA;
        } else if (strcasecmp("h52ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_H52GA;
            buf_size = BUF_SIZE_H52GA;
            frame_header_size = FRAME_HEADER_SIZE_H52GA;
            data_offset = DATA_OFFSET_H52GA;
            lowres_byte = LOWRES_BYTE_H52GA;
            highres_byte = HIGHRES_BYTE_H52GA;
        } else if (strcasecmp("h60ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_H60GA;
            buf_size = BUF_SIZE_H60GA;
            frame_header_size = FRAME_HEADER_SIZE_H60GA;
            data_offset = DATA_OFFSET_H60GA;
            lowres_byte = LOWRES_BYTE_H60GA;
            highres_byte = HIGHRES_BYTE_H60GA;
        } else if (strcasecmp("y28ga", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_Y28GA;
            buf_size = BUF_SIZE_Y28GA;
            frame_header_size = FRAME_HEADER_SIZE_Y28GA;
            data_offset = DATA_OFFSET_Y28GA;
            lowres_byte = LOWRES_BYTE_Y28GA;
            highres_byte = HIGHRES_BYTE_Y28GA;
        } else if (strcasecmp("q321br_lsx", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_Q321BR_LSX;
            buf_size = BUF_SIZE_Q321BR_LSX;
            frame_header_size = FRAME_HEADER_SIZE_Q321BR_LSX;
            data_offset = DATA_OFFSET_Q321BR_LSX;
            lowres_byte = LOWRES_BYTE_Q321BR_LSX;
            highres_byte = HIGHRES_BYTE_Q321BR_LSX;
        } else if (strcasecmp("qg311r", argv[1]) == 0) {
            buf_offset = BUF_OFFSET_QG311R;
            buf_size = BUF_SIZE_QG311R;
            frame_header_size = FRAME_HEADER_SIZE_QG311R;
            data_offset = DATA_OFFSET_QG311R;
            lowres_byte = LOWRES_BYTE_QG311R;
            highres_byte = HIGHRES_BYTE_QG311R;
        }
    }

    // Opening an existing file
    fFid = fopen(BUFFER_FILE, "r") ;
    if ( fFid == NULL ) {
        if (debug) fprintf(stderr, "could not open file %s\n", BUFFER_FILE) ;
        return -1;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, buf_size, PROT_READ, MAP_SHARED, fileno(fFid), 0);
    if (addr == MAP_FAILED) {
        if (debug) fprintf(stderr, "error mapping file %s\n", BUFFER_FILE);
            return -2;
    }
    if (debug) fprintf(stderr, "mapping file %s, size %d, to %08x\n", BUFFER_FILE, buf_size, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "closing the file %s\n", BUFFER_FILE) ;
    fclose(fFid) ;

    // Define default vaules
    addrh = addr + buf_offset;
    sizeh = buf_size - buf_offset;

    buf_idx_1 = addrh;
    buf_idx_w = 0;

    state = STATE_NONE;

    // Infinite loop
    while (1) {
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx_w = addrh + i;
        if (debug) fprintf(stderr, "buf_idx_w: %08x\n", (unsigned int) buf_idx_w);

        buf_idx_tmp = cb_memmem(buf_idx_1, buf_idx_w - buf_idx_1, NALx_START, sizeof(NALx_START), addrh, sizeh);
        if (buf_idx_tmp == NULL) {
            usleep(USLEEP);
            continue;
        } else {
            buf_idx_1 = buf_idx_tmp;
        }
        if (debug) fprintf(stderr, "found buf_idx_1: %08x\n", (unsigned int) buf_idx_1);

        buf_idx_tmp = cb_memmem(buf_idx_1 + 1, buf_idx_w - (buf_idx_1 + 1), NALx_START, sizeof(NALx_START), addrh, sizeh);
        if (buf_idx_tmp == NULL) {
            usleep(USLEEP);
            continue;
        } else {
            buf_idx_2 = buf_idx_tmp;
        }
        if (debug) fprintf(stderr, "found buf_idx_2: %08x\n", (unsigned int) buf_idx_2);

        switch (state) {
            case STATE_SPS_HIGH:
                if ((memcmp(buf_idx_1, PPS4_START, sizeof(PPS4_START)) == 0) ||
                        (memcmp(buf_idx_1, PPS5_START, sizeof(PPS5_START)) == 0)) {
                    state = STATE_PPS_HIGH;
                    if (debug) fprintf(stderr, "state = STATE_PPS_HIGH\n");
                    hl_frame[0].pps_addr = buf_idx_1 - addr;
                    hl_frame[0].pps_len = getFrameLen(buf_idx_1, 0);
                }
                break;
            case STATE_PPS_HIGH:
                if (memcmp(buf_idx_1, IDR4_START, sizeof(IDR4_START)) == 0) {
                    state = STATE_NONE;
                    if (debug) fprintf(stderr, "state = STATE_IDR_HIGH\n");
                    hl_frame[0].vps_addr = 0;
                    hl_frame[0].vps_len = 0;
                    hl_frame[0].idr_addr = buf_idx_1 - addr;
                    hl_frame[0].idr_len = getFrameLen(buf_idx_1, 0);

                    if (memcmp(hl_frame, hl_frame_old, 2 * sizeof(frame)) != 0) {
                        writeFile(I_FILE, (unsigned char *) hl_frame, 2 * sizeof(frame));
                        memcpy(hl_frame_old, hl_frame, 2 * sizeof(frame));
                    }
                }
                if (memcmp(buf_idx_1, VPS5_START, sizeof(VPS5_START)) == 0) {
                    state = STATE_VPS_HIGH;
                    if (debug) fprintf(stderr, "state = STATE_VPS_HIGH\n");
                    hl_frame[0].vps_addr = buf_idx_1 - addr;
                    hl_frame[0].vps_len = getFrameLen(buf_idx_1, 0);
                }
                break;
            case STATE_VPS_HIGH:
                if (memcmp(buf_idx_1, IDR5_START, sizeof(IDR5_START)) == 0) {
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

            case STATE_SPS_LOW:
                if (memcmp(buf_idx_1, PPS4_START, sizeof(PPS4_START)) == 0) {
                    state = STATE_PPS_LOW;
                    if (debug) fprintf(stderr, "state = STATE_PPS_LOW\n");
                    hl_frame[1].pps_addr = buf_idx_1 - addr;
                    hl_frame[1].pps_len = getFrameLen(buf_idx_1, 0);
                }
                break;
            case STATE_PPS_LOW:
                if (memcmp(buf_idx_1, IDR4_START, sizeof(IDR4_START)) == 0) {
                    state = STATE_NONE;
                    if (debug) fprintf(stderr, "state = STATE_IDR_LOW\n");
                    hl_frame[1].vps_addr = 0;
                    hl_frame[1].vps_len = 0;
                    hl_frame[1].idr_addr = buf_idx_1 - addr;
                    hl_frame[1].idr_len = getFrameLen(buf_idx_1, 0);

                    if (memcmp(hl_frame, hl_frame_old, 2 * sizeof(frame)) != 0) {
                        writeFile(I_FILE, (unsigned char *) hl_frame, 2 * sizeof(frame));
                        memcpy(hl_frame_old, hl_frame, 2 * sizeof(frame));
                    }
                }
                break;

            case STATE_NONE:
                if ((memcmp(buf_idx_1, SPS4_START, sizeof(SPS4_START)) == 0) ||
                        (memcmp(buf_idx_1, SPS5_START, sizeof(SPS5_START)) == 0)) {
                    buf_idx_1 = cb_move(buf_idx_1, - (6 + frame_header_size));
                    if (buf_idx_1[17 + data_offset] == highres_byte) {
                        buf_idx_1 = cb_move(buf_idx_1, 6 + frame_header_size);
                        state = STATE_SPS_HIGH;
                        hl_frame[0].sps_addr = buf_idx_1 - addr;
                        hl_frame[0].sps_len = getFrameLen(buf_idx_1, 6);
                    } else if (buf_idx_1[17 + data_offset] == lowres_byte) {
                        buf_idx_1 = cb_move(buf_idx_1, 6 + frame_header_size);
                        state = STATE_SPS_LOW;
                        hl_frame[1].sps_addr = buf_idx_1 - addr;
                        hl_frame[1].sps_len = getFrameLen(buf_idx_1, 6);
                    }
                }
                break;

            default:
                break;
        }

        buf_idx_1 = buf_idx_2;
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(addr, buf_size) == -1) {
        if (debug) fprintf(stderr, "error munmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, buf_size, addr);
    }

    return 0;
}
