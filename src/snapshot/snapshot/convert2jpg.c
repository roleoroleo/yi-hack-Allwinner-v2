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
 * Reads the YUV file and converts it to jpg.
 */

#include "convert2jpg.h"

extern int camera_dbg_en;

/**
 * Converts a YUYV raw buffer to a JPEG buffer.
 * Input is YUYV (YUV 420SP NV12). Output is JPEG binary.
 */
int YUVtoJPG(char *output_file, unsigned char *input, const int width, const int height, const int dest_width, const int dest_height)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];
    int row_stride;

    FILE *fp;

    uint8_t* outbuffer = NULL;
    unsigned long outlen = 0;

    unsigned int wsl, hsl, i, j;
    unsigned int offset;
    unsigned int uv;

    // width != dest_width currently not supported
    if (width < dest_width) return -1;

    // height < dest_height currently not supported
    if (height < dest_height) return -1;

    wsl = width - dest_width;
    hsl = height - dest_height;

    // width - dest_width must be even
    if ((wsl % 2) == 1) return -1;

    // height - dest_height must be even
    if ((hsl % 2) == 1) return -1;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuffer, &outlen);

    // jrow is a libjpeg row of samples array of 1 row pointer
    cinfo.image_width = dest_width & -1;
    cinfo.image_height = dest_height & -1;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr; //libJPEG expects YUV 3bytes, 24bit

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    uint8_t tmprowbuf[dest_width * 3];

    JSAMPROW row_pointer[1];
    row_pointer[0] = &tmprowbuf[0];

    while (cinfo.next_scanline < cinfo.image_height) {
        offset = (cinfo.next_scanline + hsl/2) * width + wsl/2;                 //offset to the correct y row
        uv = width * (height - (cinfo.next_scanline + hsl/2 + 1) / 2) + wsl/2;  //offset to the correct uv row

        for (i = 0, j = 0; i < cinfo.image_width; i += 2, j += 6) { //input strides by 2 bytes, output strides by 6 (2 pixels)
            tmprowbuf[j + 0] = input[offset + i];          // Y (unique to this pixel)
            tmprowbuf[j + 1] = input[offset + uv + i];     // U (shared between pixels)
            tmprowbuf[j + 2] = input[offset + uv + i + 1]; // V (shared between pixels)
            tmprowbuf[j + 3] = input[offset + i + 1];      // Y (unique to this pixel)
            tmprowbuf[j + 4] = input[offset + uv + i];     // U (shared between pixels)
            tmprowbuf[j + 5] = input[offset + uv + i + 1]; // V (shared between pixels)
        }
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    if (strcmp("stdout", output_file) == 0)
        fwrite(outbuffer, 1, outlen, stdout);
    else {
        fp = fopen(output_file, "wb");
        if (fp == NULL)
            return -1;
        fwrite(outbuffer, 1, outlen, fp);
        fclose(fp);
    }

    return outlen;
}

int convert2jpg(char *output_file, char *input_file, const int width, const int height, const int dest_width, const int dest_height)
{
    FILE *fp;
    char *buffer;
    int size;

    fp = fopen(input_file, "wb");
    if (fp == NULL)
        return -1;

    size = width * height * 3 / 2;
    buffer = (unsigned char *) malloc(size * sizeof(unsigned char));
    if (fread(buffer, 1, size, fp) != size)
        return -2;

    YUVtoJPG(output_file, buffer, width, height, dest_width, dest_height);
    free(buffer);

    fclose(fp);
}
