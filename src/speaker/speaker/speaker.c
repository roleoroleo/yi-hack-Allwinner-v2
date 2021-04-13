#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

int DEVICE_NUM = 0x70;

int open_cpld() {
    int fd;
    if (access("/dev/cpld_periph", F_OK) == 0) {
        fd = open("/dev/cpld_periph", O_RDWR);
    } else {
        fd = -1;
    }

    return fd;
}

void close_cpld(int fd) {
    close(fd);
}

void print_usage() {
    printf("speaker\n\n");
    printf("Usage: speaker on|off\n\n");
}

void run_io(int fd, int n) {
    ioctl(fd, _IOC(0, 0x70, n, 0x00), 0);
}

int main(int argc, char *argv[])
{
    int num = -1;

    if (argc != 2) {
        print_usage();
        return -1;
    }

    if (strcmp("on", argv[1]) == 0) {
        num = 16;
    } else if (strcmp("off", argv[1]) == 0) {
        num = 17;
    }

    if (num == -1) {
        print_usage();
        return -2;
    }

    int fd = open_cpld();
    if (fd < 0) {
        puts("Error: cannot open /dev/cpld_periph");
        return 1;
    }

    printf("Running ioctl: num %d\n", num);
    run_io(fd, num);

    close_cpld(fd);

    return 0;
}
