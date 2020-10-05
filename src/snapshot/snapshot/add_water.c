#include "add_water.h"

int WMInit(WaterMarkInfo *WM_info, char WMPath[30])
{
    int i;
    int watermark_pic_num = 13;
    char filename[64];
    FILE *icon_hdle = NULL;
    unsigned char *tmp_argb = NULL;
    int width = 0;
    int height = 0;
    int start_bmp = 0;

    WM_info->width = 0;
    WM_info->height = 0;

    /* init watermark pic info */
    for (i = 0; i < watermark_pic_num; i++) {
        sprintf(filename, "%s%d.bmp", WMPath, i);
        icon_hdle = fopen(filename, "r");
        if (icon_hdle == NULL) {
            fprintf(stderr, "get watermark %s error\n", filename);
            return -1;
        }

        /* get watermark picture size */
        fseek(icon_hdle, 10, SEEK_SET);
        fread(&start_bmp, 1, 4, icon_hdle);
        fseek(icon_hdle, 18, SEEK_SET);
        fread(&width, 1, 4, icon_hdle);
        fread(&height, 1, 4, icon_hdle);
        fseek(icon_hdle, start_bmp, SEEK_SET);

        if (WM_info->width == 0) {
            WM_info->width = width;
            WM_info->height = height * (-1);
        }

        WM_info->single_pic[i].id = i;
        WM_info->single_pic[i].y = (unsigned char *)malloc(WM_info->width * WM_info->height * 5 / 2);
        WM_info->single_pic[i].alph = WM_info->single_pic[i].y + WM_info->width * WM_info->height;
        WM_info->single_pic[i].c = WM_info->single_pic[i].alph + WM_info->width * WM_info->height;

        if (tmp_argb == NULL)
            tmp_argb = (unsigned char *)malloc(WM_info->width * WM_info->height * 4);

        fread(tmp_argb, WM_info->width * WM_info->height * 4, 1, icon_hdle);
        argb2yuv420sp(tmp_argb, WM_info->single_pic[i].alph, WM_info->width, WM_info->height,
                        WM_info->single_pic[i].y, WM_info->single_pic[i].c);

        fclose(icon_hdle);
        icon_hdle = NULL;
    }
    WM_info->picture_number = i;

    if (tmp_argb != NULL)
        free(tmp_argb);

    return 0;
}

int WMRelease(WaterMarkInfo *WM_info)
{
    int watermark_pic_num = 13;
    int i;

    for (i = 0; i < watermark_pic_num; i++) {
        if (WM_info->single_pic[i].y) {
            free(WM_info->single_pic[i].y);
            WM_info->single_pic[i].y = NULL;
        }
    }

    return 0;
}

int AddWM (WaterMarkInfo *WM_info, unsigned int bg_width, unsigned int bg_height, void *bg_y_vir,
            void *bg_c_vir, unsigned int wm_pos_x, unsigned int wm_pos_y, struct tm *time_data)
{
    time_t rawtime;
    struct tm *time_info;
    BackGroudLayerInfo BG_info;
    ShowWaterMarkParam WM_Param;

    memset(&BG_info, 0, sizeof(BackGroudLayerInfo));
    /* init backgroud info */
    BG_info.width = bg_width;
    BG_info.height = bg_height;
    BG_info.y = (unsigned char *)bg_y_vir;
    BG_info.c = (unsigned char *)bg_c_vir;

    /* init watermark show para */
    WM_Param.pos.x = wm_pos_x;
    WM_Param.pos.y = wm_pos_y;

    if (time_data == NULL){
        time(&rawtime);
        time_info = localtime(&rawtime);
    }else{
        time_info = time_data;
        time_info->tm_year = time_info->tm_year - 1900;
    }

    WM_Param.id_list[0] = (time_info->tm_year + 1900) / 1000;
    WM_Param.id_list[1] = ((time_info->tm_year + 1900) / 100) % 10;
    WM_Param.id_list[2] = ((time_info->tm_year + 1900) / 10) % 10;
    WM_Param.id_list[3] = (time_info->tm_year + 1900) % 10;
    WM_Param.id_list[4] = 11;
    WM_Param.id_list[5] = (1 + time_info->tm_mon) / 10 ;
    WM_Param.id_list[6] = (1 + time_info->tm_mon) % 10;
    WM_Param.id_list[7] = 11;
    WM_Param.id_list[8] = time_info->tm_mday / 10;
    WM_Param.id_list[9] = time_info->tm_mday % 10;
    WM_Param.id_list[10] = 10;
    WM_Param.id_list[11] = time_info->tm_hour / 10;
    WM_Param.id_list[12] = time_info->tm_hour % 10;
    WM_Param.id_list[13] = 12;
    WM_Param.id_list[14] = time_info->tm_min / 10;
    WM_Param.id_list[15] = time_info->tm_min % 10;
    WM_Param.id_list[16] = 12;
    WM_Param.id_list[17] = time_info->tm_sec / 10;
    WM_Param.id_list[18] = time_info->tm_sec % 10;
    WM_Param.number = 19;

    watermark_blending(&BG_info, WM_info, &WM_Param);

    return 0;
}
