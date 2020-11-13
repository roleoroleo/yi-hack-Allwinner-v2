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
 * Read the last h264 i-frame from the buffer and convert it using libavcodec
 * and libjpeg.
 * The position of the frame is written in /tmp/iframe.idx
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <getopt.h>

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include "libavcodec/avcodec.h"

#include "convert2jpg.h"
#include "add_water.h"

#define BUF_OFFSET 300
#define BUF_SIZE 1786156

#define BUFFER_FILE "/dev/shm/fshare_frame_buf"
#define I_FILE "/tmp/iframe.idx"
#define FF_INPUT_BUFFER_PADDING_SIZE 32

#define RESOLUTION_HIGH 0
#define RESOLUTION_LOW 1

#define PATH_RES_HIGH "/tmp/sd/yi-hack/etc/wm_res/high/wm_540p_"
#define PATH_RES_LOW  "/tmp/sd/yi-hack/etc/wm_res/low/wm_540p_"

#define W_LOW 640
#define H_LOW 360
#define W_HIGH 1920
#define H_HIGH 1080

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

int debug = 1;
unsigned char *addr;

void *cb_memcpy(void * dest, const void * src, size_t n)
{
    unsigned char *uc_src = (unsigned char *) src;
    unsigned char *uc_dest = (unsigned char *) dest;

    if (uc_src + n > addr + BUF_SIZE) {
        memcpy(uc_dest, uc_src, addr + BUF_SIZE - uc_src);
        memcpy(uc_dest + (addr + BUF_SIZE - uc_src), addr + BUF_OFFSET, n - (addr + BUF_SIZE - uc_src));
    } else {
        memcpy(dest, src, n);
    }
    return dest;
}

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

    inbuf = (uint8_t *) malloc(length + FF_INPUT_BUFFER_PADDING_SIZE);
    if (inbuf == NULL) {
        if (debug) fprintf(stderr, "Error allocating memory\n");
        avcodec_close(c);
        av_free(c);
        return -2;
    }
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
        free(inbuf);
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
    free(inbuf);
    av_frame_free(&picture);
    avcodec_close(c);
    av_free(c);

    return 0;
}

int add_watermark(char *buffer, int resolution)
{
    int w_res, h_res;
    char path_res[1024];
    FILE *fBuf;
    WaterMarkInfo WM_info;

    if (resolution == RESOLUTION_LOW) {
        w_res = W_LOW;
        h_res = H_LOW;
        strcpy(path_res, PATH_RES_LOW);
    } else {
        w_res = W_HIGH;
        h_res = H_HIGH;
        strcpy(path_res, PATH_RES_HIGH);
    }

    if (WMInit(&WM_info, path_res) < 0) {
        fprintf(stderr, "water mark init error\n");
        free(buffer);
        return -1;
    } else {
        if (resolution == RESOLUTION_LOW) {
            AddWM(&WM_info, w_res, h_res, buffer,
                buffer + w_res*h_res, w_res-230, h_res-20, NULL);
        } else {
            AddWM(&WM_info, w_res, h_res, buffer,
                buffer + w_res*h_res, w_res-460, h_res-40, NULL);
        }
        WMRelease(&WM_info);
    }

    return 0;
}

void usage(char *prog_name)
{
    fprintf(stderr, "Usage: %s [options]\n", prog_name);
    fprintf(stderr, "\t-o, --output FILE       Set output file name (stdout to print on stdout)\n");
    fprintf(stderr, "\t-r, --res RES           Set resolution: \"low\" or \"high\" (default \"high\")\n");
    fprintf(stderr, "\t-w, --watermark         Add watermark to image\n");
    fprintf(stderr, "\t-h, --help              Show this help\n");
}

int main(int argc, char **argv)
{
    FILE *fIdx, *fBuf;
    uint32_t offset, length;
    int res = RESOLUTION_HIGH;
    frame hl_frame[2];
    unsigned char *bufferh26x, *bufferyuv;
    int watermark = 0;

    int c;

    while (1) {
        static struct option long_options[] = {
            {"res",       required_argument, 0, 'r'},
            {"watermark", no_argument,       0, 'w'},
            {"help",      no_argument,       0, 'h'},
            {0,           0,                 0,  0 }
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "r:wh",
            long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'r':
                if (strcasecmp("low", optarg) == 0)
                    res = RESOLUTION_LOW;
                else
                    res = RESOLUTION_HIGH;
                break;

            case 'w':
                watermark = 1;
                break;

            case 'h':
            default:
                usage(argv[0]);
                exit(-1);
                break;
        }
    }

    if (debug) fprintf(stderr, "Starting program\n");

    fIdx = fopen(I_FILE, "r");
    if ( fIdx == NULL ) {
        fprintf(stderr, "Could not open file %s\n", I_FILE);
        exit(-1);
    }
    if (fread(hl_frame, 1, 2 * sizeof(frame), fIdx) != 2 * sizeof(frame)) {
        fprintf(stderr, "Error reading file %s\n", I_FILE);
        exit(-1);
    }

    fBuf = fopen(BUFFER_FILE, "r") ;
    if (fBuf == NULL) {
        fprintf(stderr, "Could not open file %s\n", BUFFER_FILE);
        exit(-1);
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, BUF_SIZE, PROT_READ, MAP_SHARED, fileno(fBuf), 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Error mapping file %s\n", BUFFER_FILE);
        exit(-1);
    }
    if (debug) fprintf(stderr, "Mapping file %s, size %d, to %08x\n", BUFFER_FILE, BUF_SIZE, addr);

    // Closing the file
    fclose(fBuf) ;

    bufferh26x = (unsigned char *) malloc(hl_frame[res].vps_len + hl_frame[res].sps_len + hl_frame[res].pps_len + hl_frame[res].idr_len);
    if (bufferh26x == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit -1;
    }
    if (res == RESOLUTION_LOW) {
        bufferyuv = (unsigned char *) malloc(W_LOW * H_LOW * 3 / 2);
    } else {
        bufferyuv = (unsigned char *) malloc(W_HIGH * H_HIGH * 3 / 2);
    }
    if (bufferyuv == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit -1;
    }

    if (hl_frame[res].vps_len != 0) {
        memcpy(bufferh26x, addr + hl_frame[res].vps_addr, hl_frame[res].vps_len);
    }
    memcpy(bufferh26x + hl_frame[res].vps_len, addr + hl_frame[res].sps_addr, hl_frame[res].sps_len);
    memcpy(bufferh26x + hl_frame[res].vps_len + hl_frame[res].sps_len, addr + hl_frame[res].pps_addr, hl_frame[res].pps_len);
    memcpy(bufferh26x + hl_frame[res].vps_len + hl_frame[res].sps_len + hl_frame[res].pps_len, addr + hl_frame[res].idr_addr, hl_frame[res].idr_len);

    if (hl_frame[res].vps_len == 0) {
        if (debug) fprintf(stderr, "Encoding h264 frame\n");
        if(frame_decode(bufferyuv, bufferh26x, hl_frame[res].sps_len + hl_frame[res].pps_len + hl_frame[res].idr_len, 4) < 0) {
            fprintf(stderr, "Error encoding h264 frame\n");
            exit(-2);
        }
    } else {
        if (debug) fprintf(stderr, "Encoding h265 frame\n");
        if(frame_decode(bufferyuv, bufferh26x, hl_frame[res].vps_len + hl_frame[res].sps_len + hl_frame[res].pps_len + hl_frame[res].idr_len, 5) < 0) {
            fprintf(stderr, "Error encoding h265 frame\n");
            exit(-2);
        }
    }
    free(bufferh26x);

    if (watermark) {
        if (debug) fprintf(stderr, "Adding watermark\n");
        if (add_watermark(bufferyuv, res) < 0) {
            fprintf(stderr, "Error adding watermark\n");
            exit -3;
        }
    }

    if (debug) fprintf(stderr, "Encoding jpeg image\n");
    if (res == RESOLUTION_LOW) {
        if(YUVtoJPG("stdout", bufferyuv, W_LOW, H_LOW, W_LOW, H_LOW) < 0) {
            fprintf(stderr, "Error encoding jpeg file\n");
            exit(-4);
        }
    } else {
        if(YUVtoJPG("stdout", bufferyuv, W_HIGH, H_HIGH, W_HIGH, H_HIGH) < 0) {
            fprintf(stderr, "Error encoding jpeg file\n");
            exit(-4);
        }
    }

    free(bufferyuv);

    // Unmap file from memory
    if (munmap(addr, BUF_SIZE) == -1) {
        fprintf(stderr, "Error munmapping file\n");
    } else {
        if (debug) fprintf(stderr, "Unmapping file %s, size %d, from %08x\n", BUFFER_FILE, BUF_SIZE, addr);
    }

    return 0;
}
