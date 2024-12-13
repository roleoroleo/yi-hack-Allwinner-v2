#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "ptz.h"

#define MODEL_SUFFIX_FILE_MA "/home/yi-hack/model_suffix"
#define MODEL_SUFFIX_FILE_A2 "/tmp/sd/yi-hack/model_suffix"

int is_ptz(char *model) {
    if ((strcmp("h201c", model) == 0) ||
            (strcmp("h305r", model) == 0) ||
            (strcmp("y30", model) == 0) ||
            (strcmp("h307", model) == 0)) {

        return MSTAR;

    } else if (strcmp("y30qa", model) == 0) {

        return ALLWINNER;

    } else if ((strcmp("r30gb", model) == 0) ||
            (strcmp("r35gb", model) == 0) ||
            (strcmp("r37gb", model) == 0) ||
            (strcmp("r40ga", model) == 0) ||
            (strcmp("q321br_lsx", model) == 0) ||
            (strcmp("qg311r", model) == 0) ||
            (strcmp("b091qp", model) == 0)) {

        return ALLWINNER_V2;

    } else if ((strcmp("h30ga", model) == 0) ||
            (strcmp("h51ga", model) == 0) ||
            (strcmp("h52ga", model) == 0) ||
            (strcmp("h60ga", model) == 0)) {

        return ALLWINNER_V2_ALT;
    } else {
        return 0;
    }
}

int get_model_suffix(char *model, int size)
{
    FILE *fileStream;

    fileStream = fopen(MODEL_SUFFIX_FILE_MA, "r");
    if (fileStream == NULL) {
        fileStream = fopen(MODEL_SUFFIX_FILE_A2, "r");
        if (fileStream == NULL) {
            return -1;
        }
    }
    if (fgets (model, size, fileStream) == NULL)
        return -2;
    fclose(fileStream);
    model[strcspn(model, "\r\n")] = '\0';

    return 0;
}

int ssp_init()
{
    int fd, iRet;

    fd = open("/dev/ssp", O_RDONLY);

    return fd;
}

int read_ptz(int *px, int *py, int *pi)
{
    int fd_ssp;
    fd_ssp = ssp_init();
    if (fd_ssp == -1) {
        fprintf(stderr, "Error opening cpld\n");
        return -1;
    }

    int iRet;
    unsigned char pos[32];

    memset(pos, '\0', 32);
    iRet = ioctl(fd_ssp, 0x6d01, &pos);
    if (iRet < 0) {
        fprintf(stderr, "Error getting position\n");
        fprintf(stderr, "ioctl failed and returned errno %s\n", strerror(errno));
        close(fd_ssp);
        return -2;
    }

    close(fd_ssp);

    *px = *((int *) (&pos[0]));
    *py = *((int *) (&pos[4]));
    *pi = *((int *) (&pos[20]));

    return 0;
}
