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
 * Read the last h264 i-frame from the buffer and convert it using libavcodec
 * and libjpeg.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <getopt.h>

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include "libavcodec/avcodec.h"

#include "convert2jpg.h"
#include "add_water.h"

//#define USE_SEMAPHORE 1
#ifdef USE_SEMAPHORE
#include <semaphore.h>
#endif

#define BUF_OFFSET_Y21GA 368
#define FRAME_HEADER_SIZE_Y21GA 28

#define BUF_OFFSET_Y211GA 368
#define FRAME_HEADER_SIZE_Y211GA 28

#define BUF_OFFSET_Y291GA 368
#define FRAME_HEADER_SIZE_Y291GA 28

#define BUF_OFFSET_H30GA 368
#define FRAME_HEADER_SIZE_H30GA 28

#define BUF_OFFSET_R30GB 300
#define FRAME_HEADER_SIZE_R30GB 22

#define BUF_OFFSET_R35GB 300
#define FRAME_HEADER_SIZE_R35GB 26

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

#define BUF_OFFSET_Q321BR_LSX 300
#define FRAME_HEADER_SIZE_Q321BR_LSX 26

#define BUF_OFFSET_QG311R 300
#define FRAME_HEADER_SIZE_QG311R 26

#define BUF_OFFSET_B091QP 300
#define FRAME_HEADER_SIZE_B091QP 26

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define BUFFER_SHM "fshare_frame_buf"
#define READ_LOCK_FILE "fshare_read_lock"
#define WRITE_LOCK_FILE "fshare_write_lock"
#define FF_INPUT_BUFFER_PADDING_SIZE 32

#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080

#define RESOLUTION_FHD  1080
#define RESOLUTION_3K   1296

#define PATH_RES_LOW  "/tmp/sd/yi-hack/etc/wm_res/low/wm_540p_"
#define PATH_RES_HIGH "/tmp/sd/yi-hack/etc/wm_res/high/wm_540p_"

#define W_LOW 640
#define H_LOW 360
#define W_FHD 1920
#define H_FHD 1080
#define W_3K 2304
#define H_3K 1296

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

int buf_offset;
int buf_size;
int frame_header_size;
int res;
int debug;

unsigned char *addr;

#ifdef USE_SEMAPHORE
sem_t *sem_fshare_read_lock = SEM_FAILED;
sem_t *sem_fshare_write_lock = SEM_FAILED;
#endif

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > addr + buf_size))
        buf -= (buf_size - buf_offset);
    if ((offset < 0) && (buf < addr + buf_offset))
        buf += (buf_size - buf_offset);

    return buf;
}

void *cb_memcpy(void * dest, const void * src, size_t n)
{
    unsigned char *uc_src = (unsigned char *) src;
    unsigned char *uc_dest = (unsigned char *) dest;

    if (uc_src + n > addr + buf_size) {
        memcpy(uc_dest, uc_src, addr + buf_size - uc_src);
        memcpy(uc_dest + (addr + buf_size - uc_src), addr + buf_offset, n - (addr + buf_size - uc_src));
    } else {
        memcpy(uc_dest, src, n);
    }
    return dest;
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

int frame_decode(unsigned char *outbuffer, unsigned char *p, int length, int h26x)
{
    AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *picture;
    int got_picture, len;
    FILE *fOut;
    uint8_t *inbuf;
    AVPacket avpkt;
    int i, j, size;

//////////////////////////////////////////////////////////
//                    Reading H264                      //
//////////////////////////////////////////////////////////

    if (debug) fprintf(stderr, "Starting decode\n");

    av_init_packet(&avpkt);

    if (h26x == 4) {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            if (debug) fprintf(stderr, "Codec h264 not found\n");
            return -2;
        }
    } else {
        codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        if (!codec) {
            if (debug) fprintf(stderr, "Codec hevc not found\n");
            return -2;
        }
    }

    c = avcodec_alloc_context3(codec);
    picture = av_frame_alloc();

    if((codec->capabilities) & AV_CODEC_CAP_TRUNCATED)
        (c->flags) |= AV_CODEC_FLAG_TRUNCATED;

    if (avcodec_open2(c, codec, NULL) < 0) {
        if (debug) fprintf(stderr, "Could not open codec h264\n");
        av_free(c);
        return -2;
    }

    // inbuf is already allocated in the main function
    inbuf = p;
    memset(inbuf + length, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    // Get only 1 frame
    memcpy(inbuf, p, length);
    avpkt.size = length;
    avpkt.data = inbuf;

    // Decode frame
    if (debug) fprintf(stderr, "Decode frame\n");
    if (c->codec_type == AVMEDIA_TYPE_VIDEO ||
         c->codec_type == AVMEDIA_TYPE_AUDIO) {

        len = avcodec_send_packet(c, &avpkt);
        if (len < 0 && len != AVERROR(EAGAIN) && len != AVERROR_EOF) {
            if (debug) fprintf(stderr, "Error decoding frame\n");
            return -2;
        } else {
            if (len >= 0)
                avpkt.size = 0;
            len = avcodec_receive_frame(c, picture);
            if (len >= 0)
                got_picture = 1;
        }
    }
    if(!got_picture) {
        if (debug) fprintf(stderr, "No input frame\n");
        av_frame_free(&picture);
        avcodec_close(c);
        av_free(c);
        return -2;
    }

    if (debug) fprintf(stderr, "Writing yuv buffer\n");
    memset(outbuffer, 0x80, c->width * c->height * 3 / 2);
    memcpy(outbuffer, picture->data[0], c->width * c->height);
    for(i=0; i<c->height/2; i++) {
        for(j=0; j<c->width/2; j++) {
            outbuffer[c->width * c->height + c->width * i +  2 * j] = *(picture->data[1] + i * picture->linesize[1] + j);
            outbuffer[c->width * c->height + c->width * i +  2 * j + 1] = *(picture->data[2] + i * picture->linesize[2] + j);
        }
    }

    // Clean memory
    if (debug) fprintf(stderr, "Cleaning ffmpeg memory\n");
    av_frame_free(&picture);
    avcodec_close(c);
    av_free(c);

    return 0;
}

int add_watermark(unsigned char *buffer, int w_res, int h_res)
{
    char path_res[1024];
    FILE *fBuf;
    WaterMarkInfo WM_info;

    if (w_res != W_LOW) {
        strcpy(path_res, PATH_RES_HIGH);
    } else {
        strcpy(path_res, PATH_RES_LOW);
    }

    if (WMInit(&WM_info, path_res) < 0) {
        fprintf(stderr, "water mark init error\n");
        return -1;
    } else {
        if (w_res != W_LOW) {
            AddWM(&WM_info, w_res, h_res, buffer,
                buffer + w_res*h_res, w_res-460, h_res-40, NULL);
        } else {
            AddWM(&WM_info, w_res, h_res, buffer,
                buffer + w_res*h_res, w_res-230, h_res-20, NULL);
        }
        WMRelease(&WM_info);
    }

    return 0;
}

pid_t proc_find(const char* process_name, pid_t process_pid)
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) {
            if (fgets(buf, sizeof(buf), fp) != NULL) {
                /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if ((strcmp(first, process_name) == 0) && ((pid_t) lpid != process_pid)) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t) lpid;
                }
            }
            fclose(fp);
        }

    }

    closedir(dir);
    return -1;
}

void usage(char *prog_name)
{
    fprintf(stderr, "Usage: %s [options]\n", prog_name);
    fprintf(stderr, "\t-m, --model MODEL       Set model: y21ga, y211ga, y291ga, h30ga, r30gb, r35gb, r40ga, h51ga, h52ga, h60ga, y28ga, y29ga, q321br_lsx, qg311r or b091qp (default y21ga)\n");
    fprintf(stderr, "\t-f, --file FILE         Ignore model and read frame from file FILE\n");
    fprintf(stderr, "\t-r, --res RES           Set resolution: \"low\" or \"high\" (default \"high\")\n");
    fprintf(stderr, "\t-w, --watermark         Add watermark to image\n");
    fprintf(stderr, "\t-d, --debug             Enable debug\n");
    fprintf(stderr, "\t-h, --help              Show this help\n");
}

int main(int argc, char **argv)
{
    FILE *fFS, *fHF;
    int fshm;
    uint32_t offset, length;

    unsigned char *buf_idx, *buf_idx_cur, *buf_idx_end;
    unsigned char *bufferh26x, *bufferyuv;
    char file[256];
    int watermark = 0;
    int model_high_res;
    int width, height;

    int c;

    struct frame_header fh, fhs, fhp, fhv, fhi;
    unsigned char *fhs_addr, *fhp_addr, *fhv_addr, *fhi_addr;

    int sps_start_found = -1, sps_end_found = -1;
    int pps_start_found = -1, pps_end_found = -1;
    int vps_start_found = -1, vps_end_found = -1;
    int idr_start_found = -1;
    int i, j, f, start_code;
    unsigned char *h26x_file_buffer;
    long h26x_file_size;
    size_t nread;

    buf_offset = BUF_OFFSET_Y21GA;
    frame_header_size = FRAME_HEADER_SIZE_Y21GA;
    memset(file, '\0', sizeof(file));
    res = RESOLUTION_HIGH;
    model_high_res = RESOLUTION_FHD;
    width = W_FHD;
    height = H_FHD;
    debug = 0;

    while (1) {
        static struct option long_options[] = {
            {"model",     required_argument, 0, 'm'},
            {"file",      required_argument, 0, 'f'},
            {"res",       required_argument, 0, 'r'},
            {"watermark", no_argument,       0, 'w'},
            {"debug",     no_argument,       0, 'd'},
            {"help",      no_argument,       0, 'h'},
            {0,           0,                 0,  0 }
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "m:f:r:wdh",
            long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'm':
                if (strcasecmp("y21ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_Y21GA;
                    frame_header_size = FRAME_HEADER_SIZE_Y21GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("y211ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_Y211GA;
                    frame_header_size = FRAME_HEADER_SIZE_Y211GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("y291ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_Y291GA;
                    frame_header_size = FRAME_HEADER_SIZE_Y291GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("h30ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_H30GA;
                    frame_header_size = FRAME_HEADER_SIZE_H30GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("r30gb", optarg) == 0) {
                    buf_offset = BUF_OFFSET_R30GB;
                    frame_header_size = FRAME_HEADER_SIZE_R30GB;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("r35gb", optarg) == 0) {
                    buf_offset = BUF_OFFSET_R35GB;
                    frame_header_size = FRAME_HEADER_SIZE_R35GB;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("r40ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_R40GA;
                    frame_header_size = FRAME_HEADER_SIZE_R40GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("h51ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_H51GA;
                    frame_header_size = FRAME_HEADER_SIZE_H51GA;
                    model_high_res = RESOLUTION_3K;
                } else if (strcasecmp("h52ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_H52GA;
                    frame_header_size = FRAME_HEADER_SIZE_H52GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("h60ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_H60GA;
                    frame_header_size = FRAME_HEADER_SIZE_H60GA;
                    model_high_res = RESOLUTION_3K;
                } else if (strcasecmp("y28ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_Y28GA;
                    frame_header_size = FRAME_HEADER_SIZE_Y28GA;
                    model_high_res = RESOLUTION_FHD;
                } else if (strcasecmp("y29ga", optarg) == 0) {
                    buf_offset = BUF_OFFSET_Y29GA;
                    frame_header_size = FRAME_HEADER_SIZE_Y29GA;
                    model_high_res = RESOLUTION_3K;
                } else if (strcasecmp("q321br_lsx", optarg) == 0) {
                    buf_offset = BUF_OFFSET_Q321BR_LSX;
                    frame_header_size = FRAME_HEADER_SIZE_Q321BR_LSX;
                    model_high_res = RESOLUTION_3K;
                } else if (strcasecmp("qg311r", optarg) == 0) {
                    buf_offset = BUF_OFFSET_QG311R;
                    frame_header_size = FRAME_HEADER_SIZE_QG311R;
                    model_high_res = RESOLUTION_3K;
                } else if (strcasecmp("b091qp", optarg) == 0) {
                    buf_offset = BUF_OFFSET_B091QP;
                    frame_header_size = FRAME_HEADER_SIZE_B091QP;
                    model_high_res = RESOLUTION_FHD;
                }
                break;

            case 'f':
                if (strlen(optarg) < sizeof(file)) {
                    strcpy(file, optarg);
                }

            case 'r':
                if (strcasecmp("low", optarg) == 0)
                    res = RESOLUTION_LOW;
                else
                    res = RESOLUTION_HIGH;
                break;

            case 'w':
                watermark = 1;
                break;

            case 'd':
                debug = 1;
                break;

            case 'h':
            default:
                usage(argv[0]);
                exit(-1);
                break;
        }
    }

    if (debug) fprintf(stderr, "Starting program\n");

    // Set low priority
    setpriority(PRIO_PROCESS, 0, 10);

    // Check if snapshot is disabled
    if (access("/tmp/snapshot.disabled", F_OK ) == 0 ) {
        fprintf(stderr, "Snapshot is disabled\n");
        return 0;
    }

    // Check if snapshot is low res
    if (access("/tmp/snapshot.low", F_OK ) == 0 ) {
        fprintf(stderr, "Snapshot is low res\n");
        res = RESOLUTION_LOW;
    }

    // Check if the process is already running
    pid_t my_pid = getpid();
    if (proc_find(basename(argv[0]), my_pid) != -1) {
        fprintf(stderr, "Process is already running\n");
        return 0;
    }

    if (res == RESOLUTION_LOW) {
        width = W_LOW;
        height = H_LOW;
    } else {
        if (model_high_res == RESOLUTION_FHD) {
            width = W_FHD;
            height = H_FHD;
        } else {
            width = W_3K;
            height = H_3K;
        }
    }
    if (debug) fprintf(stderr, "Resolution %d x %d\n", width, height);

    if (file[0] == '\0') {
        // Read frames from frame buffer
        fFS = fopen(BUFFER_FILE, "r");
        if ( fFS == NULL ) {
            fprintf(stderr, "Could not get size of %s\n", BUFFER_FILE);
            exit(-2);
        }
        fseek(fFS, 0, SEEK_END);
        buf_size = ftell(fFS);
        fclose(fFS);
        if (debug) fprintf(stderr, "The size of the buffer is %d\n", buf_size);

#ifdef USE_SEMAPHORE
        if (sem_fshare_open() != 0) {
            fprintf(stderr, "Could not open semaphores\n") ;
            exit(-3);
        }
#endif

        // Opening an existing file
        fshm = shm_open(BUFFER_SHM, O_RDWR, 0);
        if (fshm == -1) {
            fprintf(stderr, "Could not open file %s\n", BUFFER_FILE) ;
            exit(-4);
        }

        // Map file to memory
        addr = (unsigned char*) mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fshm, 0);
        if (addr == MAP_FAILED) {
            fprintf(stderr, "Error mapping file %s\n", BUFFER_FILE);
            close(fshm);
            exit(-5);
        }
        if (debug) fprintf(stderr, "Mapping file %s, size %d, to %08x\n", BUFFER_FILE, buf_size, addr);

        // Closing the file
        close(fshm);

        fhs.len = 0;
        fhp.len = 0;
        fhv.len = 0;
        fhi.len = 0;
        fhs_addr = NULL;
        fhp_addr = NULL;
        fhv_addr = NULL;
        fhi_addr = NULL;

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

            buf_idx_cur = buf_idx;

            while (buf_idx_cur != buf_idx_end) {
                cb2s_headercpy((unsigned char *) &fh, buf_idx_cur, frame_header_size);
                // Check the len
                if (fh.len > buf_size - buf_offset - frame_header_size) {
                    fhs_addr = NULL;
                    break;
                }
                if (((res == RESOLUTION_LOW) && (fh.type & 0x0800)) || ((res == RESOLUTION_HIGH) && (fh.type & 0x0400))) {
                    if (fh.type & 0x0002) {
                        memcpy((unsigned char *) &fhs, (unsigned char *) &fh, sizeof(struct frame_header));
                        fhs_addr = buf_idx_cur;
                    } else if (fh.type & 0x0004) {
                        memcpy((unsigned char *) &fhp, (unsigned char *) &fh, sizeof(struct frame_header));
                        fhp_addr = buf_idx_cur;
                    } else if (fh.type & 0x0008) {
                        memcpy((unsigned char *) &fhv, (unsigned char *) &fh, sizeof(struct frame_header));
                        fhv_addr = buf_idx_cur;
                    } else if (fh.type & 0x0001) {
                        memcpy((unsigned char *) &fhi, (unsigned char *) &fh, sizeof(struct frame_header));
                        fhi_addr = buf_idx_cur;
                    }
                }
                buf_idx_cur = cb_move(buf_idx_cur, fh.len + frame_header_size);
            }

#ifdef USE_SEMAPHORE
            sem_write_unlock();
#endif
            if (fhs_addr != NULL) break;
            usleep(10000);
        }

        // Remove headers
        if (fhv_addr != NULL) fhv_addr = cb_move(fhv_addr, frame_header_size);
        fhs_addr = cb_move(fhs_addr, frame_header_size + 6);
        fhs.len -= 6;
        fhp_addr = cb_move(fhp_addr, frame_header_size);
        fhi_addr = cb_move(fhi_addr, frame_header_size);

        if (munmap(addr, buf_size) == -1) {
            fprintf(stderr, "Error - unmapping file\n");
        } 
    } else {
        // Read frames from h26x file
        fhs.len = 0;
        fhp.len = 0;
        fhv.len = 0;
        fhi.len = 0;
        fhs_addr = NULL;
        fhp_addr = NULL;
        fhv_addr = NULL;
        fhi_addr = NULL;

        fHF = fopen(file, "r");
        if ( fHF == NULL ) {
            fprintf(stderr, "Could not get size of %s\n", file);
            exit(-6);
        }
        fseek(fHF, 0, SEEK_END);
        h26x_file_size = ftell(fHF);
        fseek(fHF, 0, SEEK_SET);
        h26x_file_buffer = (unsigned char *) malloc(h26x_file_size);
        nread = fread(h26x_file_buffer, 1, h26x_file_size, fHF);
        fclose(fHF);
        if (debug) fprintf(stderr, "The size of the file is %d\n", h26x_file_size);

        if (nread != h26x_file_size) {
            fprintf(stderr, "Read error %s\n", file);
            if (h26x_file_buffer != NULL) free(h26x_file_buffer);
            exit(-7);
        }

        for (f=0; f<h26x_file_size; f++) {
            for (i=f; i<h26x_file_size; i++) {
                if(h26x_file_buffer[i] == 0 && h26x_file_buffer[i+1] == 0 && h26x_file_buffer[i+2] == 0 && h26x_file_buffer[i+3] == 1) {
                    start_code = 4;
                } else {
                    continue;
                }

                if ((h26x_file_buffer[i+start_code]&0x7E) == 0x40) {
                    vps_start_found = i;
                    break;
                } else if (((h26x_file_buffer[i+start_code]&0x1F) == 0x7) || ((h26x_file_buffer[i+start_code]&0x7E) == 0x42)) {
                    sps_start_found = i;
                    break;
                } else if (((h26x_file_buffer[i+start_code]&0x1F) == 0x8) || ((h26x_file_buffer[i+start_code]&0x7E) == 0x44)) {
                    pps_start_found = i;
                    break;
                } else if (((h26x_file_buffer[i+start_code]&0x1F) == 0x5) || ((h26x_file_buffer[i+start_code]&0x7E) == 0x26)) {
                    idr_start_found = i;
                    break;
                }
            }

            for (j = i + 4; j<h26x_file_size; j++) {
                if (h26x_file_buffer[j] == 0 && h26x_file_buffer[j+1] == 0 && h26x_file_buffer[j+2] == 0 && h26x_file_buffer[j+3] == 1) {
                    start_code = 4;
                } else {
                    continue;
                }

                if ((h26x_file_buffer[j+start_code]&0x7E) == 0x42) {
                    vps_end_found = j;
                    break;
                } else if (((h26x_file_buffer[j+start_code]&0x1F) == 0x8) || ((h26x_file_buffer[j+start_code]&0x7E) == 0x44)) {
                    sps_end_found = j;
                    break;
                } else if (((h26x_file_buffer[j+start_code]&0x1F) == 0x5) || ((h26x_file_buffer[j+start_code]&0x7E) == 0x26)) {
                    pps_end_found = j;
                    break;
                }
            }
            f = j - 1;
        }

        if ((sps_start_found >= 0) && (pps_start_found >= 0) && (idr_start_found >= 0) &&
                (sps_end_found >= 0) && (pps_end_found >= 0)) {

            if ((vps_start_found >= 0) && (vps_end_found >= 0)) {
                fhv.len = vps_end_found - vps_start_found;
                fhv_addr = &h26x_file_buffer[vps_start_found];
            }
            fhs.len = sps_end_found - sps_start_found;
            fhp.len = pps_end_found - pps_start_found;
            fhi.len = h26x_file_size - idr_start_found;
            fhs_addr = &h26x_file_buffer[sps_start_found];
            fhp_addr = &h26x_file_buffer[pps_start_found];
            fhi_addr = &h26x_file_buffer[idr_start_found];

            if (debug) {
                fprintf(stderr, "Found SPS at %d, len %d\n", sps_start_found, fhs.len);
                fprintf(stderr, "Found PPS at %d, len %d\n", pps_start_found, fhp.len);
                if (fhv_addr != NULL) {
                    fprintf(stderr, "Found VPS at %d, len %d\n", vps_start_found, fhv.len);
                }
                fprintf(stderr, "Found IDR at %d, len %d\n", idr_start_found, fhi.len);
            }
        } else {
            if (debug) fprintf(stderr, "No frame found\n");
            if (h26x_file_buffer != NULL) free(h26x_file_buffer);
            exit(-8);
        }
    }

    // Add FF_INPUT_BUFFER_PADDING_SIZE to make the size compatible with ffmpeg conversion
    bufferh26x = (unsigned char *) malloc(fhv.len + fhs.len + fhp.len + fhi.len + FF_INPUT_BUFFER_PADDING_SIZE);
    if (bufferh26x == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        if (h26x_file_buffer != NULL) free(h26x_file_buffer);
        exit(-9);
    }

    bufferyuv = (unsigned char *) malloc(width * height * 3 / 2);
    if (bufferyuv == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        if (h26x_file_buffer != NULL) free(h26x_file_buffer);
        if (bufferh26x != NULL) free(bufferh26x);
        exit(-10);
    }

    if (file[0] == '\0') {
        if (fhv_addr != NULL) {
            cb_memcpy(bufferh26x, fhv_addr, fhv.len);
        }
        cb_memcpy(bufferh26x + fhv.len, fhs_addr, fhs.len);
        cb_memcpy(bufferh26x + fhv.len + fhs.len, fhp_addr, fhp.len);
        cb_memcpy(bufferh26x + fhv.len + fhs.len + fhp.len, fhi_addr, fhi.len);
    } else {
        if (fhv_addr != NULL) {
            memcpy(bufferh26x, fhv_addr, fhv.len);
        }
        memcpy(bufferh26x + fhv.len, fhs_addr, fhs.len);
        memcpy(bufferh26x + fhv.len + fhs.len, fhp_addr, fhp.len);
        memcpy(bufferh26x + fhv.len + fhs.len + fhp.len, fhi_addr, fhi.len);
    }

    if (h26x_file_buffer != NULL) free(h26x_file_buffer);

    if (fhv_addr == NULL) {
        if (debug) fprintf(stderr, "Decoding h264 frame\n");
        if(frame_decode(bufferyuv, bufferh26x, fhs.len + fhp.len + fhi.len, 4) < 0) {
            fprintf(stderr, "Error decoding h264 frame\n");
            if (bufferh26x != NULL) free(bufferh26x);
            if (bufferyuv != NULL) free(bufferyuv);
            exit(-11);
        }
    } else {
        if (debug) fprintf(stderr, "Decoding h265 frame\n");
        if(frame_decode(bufferyuv, bufferh26x, fhv.len + fhs.len + fhp.len + fhi.len, 5) < 0) {
            fprintf(stderr, "Error decoding h265 frame\n");
            if (bufferh26x != NULL) free(bufferh26x);
            if (bufferyuv != NULL) free(bufferyuv);
            exit(-11);
        }
    }
    if (bufferh26x != NULL) free(bufferh26x);

    if (watermark) {
        if (debug) fprintf(stderr, "Adding watermark\n");
        if (add_watermark(bufferyuv, width, height) < 0) {
            fprintf(stderr, "Error adding watermark\n");
            if (bufferyuv != NULL) free(bufferyuv);
            exit(-12);
        }
    }

    if (debug) fprintf(stderr, "Encoding jpeg image\n");
    if(YUVtoJPG("stdout", bufferyuv, width, height, width, height) < 0) {
        fprintf(stderr, "Error encoding jpeg file\n");
        if (bufferyuv != NULL) free(bufferyuv);
        exit(-13);
    }

    if (bufferyuv != NULL) free(bufferyuv);

    if (file[0] == '\0') {
        // Unmap file from memory
        if (munmap(addr, buf_size) == -1) {
            fprintf(stderr, "Error munmapping file\n");
        } else {
            if (debug) fprintf(stderr, "Unmapping file %s, size %d, from %08x\n", BUFFER_FILE, buf_size, addr);
        }

#ifdef USE_SEMAPHORE
        sem_fshare_close();
#endif
    }

    return 0;
}
