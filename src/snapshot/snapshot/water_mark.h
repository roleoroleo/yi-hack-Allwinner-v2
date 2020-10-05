#ifndef COLOR_TYPE_H
#define COLOR_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_PIC 20
#define WM_SOURCE_PATH "/tmp/res/wm_540p_"

typedef struct BackGroudLayerInfo
{
    unsigned int width;
    unsigned int height;
    unsigned char* y;
    unsigned char* c;
}BackGroudLayerInfo;

typedef struct SinglePicture
{
    unsigned char  id; //picture id
    unsigned char* y;
    unsigned char* c;
    unsigned char* alph;
}SinglePicture;

typedef struct WaterMarkInfo
{
    unsigned int width; //single pic width
    unsigned int height; //single pic height
    unsigned int picture_number;
    SinglePicture single_pic[MAX_PIC];
}WaterMarkInfo;

typedef struct WaterMarkPositon
{
    unsigned int x;
    unsigned int y;
}WaterMarkPositon;

typedef struct ShowWaterMarkParam
{
    WaterMarkPositon  pos;     //the position of the waterMark
    unsigned char     number;
    unsigned char     id_list[MAX_PIC];  //the index of the picture of the waterMark
}ShowWaterMarkParam;

void argb2yuv420sp(unsigned char *src_p, unsigned char *alph, unsigned int width, unsigned int height,
                    unsigned char *dest_y, unsigned char *dest_c);
int watermark_blending(BackGroudLayerInfo *bg_info, WaterMarkInfo *wm_info, ShowWaterMarkParam *wm_Param);
int watermark_blending_ajust_brightness(BackGroudLayerInfo *bg_info, WaterMarkInfo *wm_info, ShowWaterMarkParam *wm_Param);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
