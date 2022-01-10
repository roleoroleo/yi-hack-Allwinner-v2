/*
 * Copyright (c) 2021 roleo.
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
 * Change volume in a 16 bit raw pcm.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#define BUFFER_SIZE 4096

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-g GAIN] [-G GAINDB] [-d]\n\n", progname);
    fprintf(stderr, "\t-g, --gain\n");
    fprintf(stderr, "\t\tgain multiplier (0.0 / 5.0)\n");
    fprintf(stderr, "\t-G, --gaindb\n");
    fprintf(stderr, "\t\tgain in dB (-12.0 / +12.0)\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv)
{
    double gain = 1.0;
    double gaindb = 0.0;
    int debug = 0;

    int errno;
    char *endptr;
    int c, i;
    int which = 0;

    double mult;
    char buffer_in[BUFFER_SIZE], buffer_out[BUFFER_SIZE];
    int16_t sample_in, sample_out;
    int32_t sample_out_32;
    int nread;

    while (1) {
        static struct option long_options[] =
        {
            {"gain",  required_argument, 0, 'g'},
            {"gaindb",  required_argument, 0, 'G'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "g:G:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'g':
            errno = 0;    /* To distinguish success/failure after call */
            gain = strtod(optarg, &endptr);

            /* Check for various possible errors */
            if ((errno == ERANGE) || (errno != 0 && gain == 0.0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((gain < 0.0) || (gain > 5.0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            which += 1;
            break;

        case 'G':
            errno = 0;    /* To distinguish success/failure after call */
            gaindb = strtod(optarg, &endptr);

            /* Check for various possible errors */
            if ((errno == ERANGE) || (errno != 0 && gaindb == 0.0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if ((gaindb < -12.0) || (gaindb > 12.0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            which += 2;
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

    if (which == 0) {
        print_usage(argv[0]);
        return -1;
    } else if (which == 2) {
        mult = pow(10.0, ((double) gaindb) / 20.0); //10^(x/20)
    } else {
        mult = gain;
    }
    if (debug) fprintf(stderr, "gain = %f, gaindb = %f, mult = %f\n", gain, gaindb, mult);

    while((nread = read(STDIN_FILENO, buffer_in, BUFFER_SIZE)) > 0)
    {
        for (i = 0; i < BUFFER_SIZE; i = i + 2) {
            // 16 bit LE (no cross platform!)
            memcpy(&sample_in, &buffer_in[i], 2);
            sample_out_32 = (int32_t) (((double) sample_in) * mult);
            if (sample_out_32 > 32767) sample_out_32 = 32767;
            if (sample_out_32 < -32768) sample_out_32 = -32768;
            sample_out = (int16_t) sample_out_32;
            memcpy(&buffer_out[i], &sample_out, 2);
        }
        write(STDOUT_FILENO, buffer_out, nread);
    }

    return 0;
}
