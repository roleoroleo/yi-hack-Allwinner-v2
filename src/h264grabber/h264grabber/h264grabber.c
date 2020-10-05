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
 * Dump h264 content from /dev/shm/fshare_frame_buffer to stdout
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <getopt.h>

#define BUF_OFFSET 300
#define BUF_SIZE 1786156
#define FRAME_HEADER_SIZE 22

#define USLEEP 100000

#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"

unsigned char IDR[]               = {0x65, 0xB8};
unsigned char NAL_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char PFR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char SPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char PPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char SPS_640X360[]       = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                       0x01, 0x01, 0x02};
unsigned char SPS_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                       0x04, 0x04, 0x04, 0x08};

unsigned char *addr;                      /* Pointer to shared memory region (header) */
int debug = 0;                            /* Set to 1 to debug this .c */
int resolution;

/* Locate a string in the circular buffer */
unsigned char * cb_memmem(unsigned char *src, int src_len, unsigned char *what, int what_len)
{
    unsigned char *p;
    unsigned char *buf = addr + BUF_OFFSET;
    int buf_size = BUF_SIZE;

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

// The second argument is the circular buffer
int cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
{
    int ret;

    if (str2 + n > addr + BUF_SIZE) {
        ret = memcmp(str1, str2, addr + BUF_SIZE - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (addr + BUF_SIZE - str2), addr + BUF_OFFSET, n - (addr + BUF_SIZE - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

// The second argument is the circular buffer
void cb_memcpy(unsigned char *dest, unsigned char *src, size_t n)
{
    if (src + n > addr + BUF_SIZE) {
        memcpy(dest, src, addr + BUF_SIZE - src);
        memcpy(dest + (addr + BUF_SIZE - src), addr + BUF_OFFSET, n - (addr + BUF_SIZE - src));
    } else {
        memcpy(dest, src, n);
    }
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-r RES] [-d]\n\n", progname);
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: LOW or HIGH (default HIGH)\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
}

int main(int argc, char **argv) {
    unsigned char *buf_idx_1, *buf_idx_2;
    unsigned char *buf_idx_w, *buf_idx_tmp;
    unsigned char *buf_idx_start, *buf_idx_end;
    unsigned char *sps_addr;
    int sps_len;
    FILE *fFid;

    int frame_res, frame_len, frame_counter, frame_counter_prev = -1;

    int i, c;
    int write_enable = 0;
    int sync_lost = 1;

    time_t ta, tb;

    resolution = RESOLUTION_HIGH;
    debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"resolution",  required_argument, 0, 'r'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "r:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
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

    if (resolution == RESOLUTION_LOW) {
        sps_addr = SPS_640X360;
        sps_len = sizeof(SPS_640X360);
    } else if (resolution == RESOLUTION_HIGH) {
        sps_addr = SPS_1920X1080;
        sps_len = sizeof(SPS_1920X1080);
    }

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

    memcpy(&i, addr + 16, sizeof(i));
    buf_idx_w = addr + BUF_OFFSET + i;
    buf_idx_1 = buf_idx_w;

    // Infinite loop
    while (1) {
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx_w = addr + BUF_OFFSET + i;
//        if (debug) fprintf(stderr, "buf_idx_w: %08x\n", (unsigned int) buf_idx_w);
        buf_idx_tmp = cb_memmem(buf_idx_1, buf_idx_w - buf_idx_1, NAL_START, sizeof(NAL_START));
        if (buf_idx_tmp == NULL) {
            usleep(USLEEP);
            continue;
        } else {
            buf_idx_1 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_1: %08x\n", (unsigned int) buf_idx_1);

        buf_idx_tmp = cb_memmem(buf_idx_1 + 1, buf_idx_w - (buf_idx_1 + 1), NAL_START, sizeof(NAL_START));
        if (buf_idx_tmp == NULL) {
            usleep(USLEEP);
            continue;
        } else {
            buf_idx_2 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_2: %08x\n", (unsigned int) buf_idx_2);

        if ((write_enable) && (!sync_lost)) {
            tb = time(NULL);
            if (buf_idx_start + frame_len > addr + BUF_SIZE) {
                fwrite(buf_idx_start, 1, addr + BUF_SIZE - buf_idx_start, stdout);
                fwrite(addr + BUF_OFFSET, 1, frame_len - (addr + BUF_SIZE - buf_idx_start), stdout);
            } else {
                fwrite(buf_idx_start, 1, frame_len, stdout);
            }
            ta = time(NULL);
            if ((frame_counter - frame_counter_prev != 1) && (frame_counter_prev != -1)) {
                fprintf(stderr, "frames lost: %d\n", frame_counter - frame_counter_prev);
            }
            if (ta - tb > 3) {
                sync_lost = 1;
                fprintf(stderr, "sync lost\n");
                sleep(3);
            }
            frame_counter_prev = frame_counter;
        }

        if (cb_memcmp(sps_addr, buf_idx_1, sps_len) == 0) {
            // SPS frame
            write_enable = 1;
            sync_lost = 0;
            buf_idx_1 = cb_move(buf_idx_1, - (6 + FRAME_HEADER_SIZE));
            if (buf_idx_1[17] == 8) {
                frame_res = RESOLUTION_LOW;
            } else if (buf_idx_1[17] == 4) {
                frame_res = RESOLUTION_HIGH;
            } else {
                write_enable = 0;
            }
            if (frame_res == resolution) {
                cb_memcpy((unsigned char *) &frame_len, buf_idx_1, 4);
                frame_len -= 6;                                                              // -6 only for SPS
                frame_counter = (int) buf_idx_1[18] + (int) buf_idx_1[19] *256;
                buf_idx_1 = cb_move(buf_idx_1, 6 + FRAME_HEADER_SIZE);
                buf_idx_start = buf_idx_1;
                if (debug) fprintf(stderr, "SPS detected - frame_res: %d - frame_len: %d - frame_counter: %d\n", frame_res, frame_len, frame_counter);
            } else {
                write_enable = 0;
            }
        } else if ((cb_memcmp(PPS_START, buf_idx_1, sizeof(PPS_START)) == 0) ||
                        (cb_memcmp(IDR_START, buf_idx_1, sizeof(IDR_START)) == 0) ||
                        (cb_memcmp(PFR_START, buf_idx_1, sizeof(PFR_START)) == 0)) {
            // PPS, IDR and PFR frames
            write_enable = 1;
            buf_idx_1 = cb_move(buf_idx_1, -FRAME_HEADER_SIZE);
            if (buf_idx_1[17] == 8) {
                frame_res = RESOLUTION_LOW;
            } else if (buf_idx_1[17] == 4) {
                frame_res = RESOLUTION_HIGH;
            } else {
                write_enable = 0;
            }
            if (frame_res == resolution) {
                cb_memcpy((unsigned char *) &frame_len, buf_idx_1, 4);
                frame_counter = (int) buf_idx_1[18] + (int) buf_idx_1[19] *256;
                buf_idx_1 = cb_move(buf_idx_1, FRAME_HEADER_SIZE);
                buf_idx_start = buf_idx_1;
                if (debug) fprintf(stderr, "frame detected - frame_res: %d - frame_len: %d - frame_counter: %d\n", frame_res, frame_len, frame_counter);
            } else {
                write_enable = 0;
            }
        } else {
            write_enable = 0;
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
