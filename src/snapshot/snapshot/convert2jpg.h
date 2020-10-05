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
 * Reads the YUV buffer, extracts the last frame and converts it to jpg.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <jpeglib.h>

#define JPEG_QUALITY 90

int YUVtoJPG(char * output_file, unsigned char *input, const int width, const int height, const int dest_width, const int dest_height);
int convert2jpg(char *output_file, char *input_file, const int width, const int height, const int dest_width, const int dest_height);
