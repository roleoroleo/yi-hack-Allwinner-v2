#ifndef __ADD_WATER_H__
#define __ADD_WATER_H__

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <ctype.h>
#include <errno.h>
#include "water_mark.h"

int WMInit(WaterMarkInfo *WM_info, char WMPath[30]);
int WMRelease(WaterMarkInfo *WM_info);
int AddWM (WaterMarkInfo *WM_info, unsigned int bg_width, unsigned int bg_height, void *bg_y_vir,
            void *bg_c_vir, unsigned int wm_pos_x, unsigned int wm_pos_y, struct tm *time_data);

#endif
