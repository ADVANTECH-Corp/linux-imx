
#ifndef		_LONTIUM_LT9211_H
#define		_LONTIUM_LT9211_H

//////////////////////LT9211 Config////////////////////////////////
#define _Mipi_PortA_
//#define _Mipi_PortB_


enum LT9211_OUTPUTMODE_ENUM
{
    OUTPUT_RGB888=0,
    OUTPUT_BT656_8BIT=1,
    OUTPUT_BT1120_16BIT=2,
    OUTPUT_LVDS_2_PORT=3,
    OUTPUT_LVDS_1_PORT=4,
    OUTPUT_YCbCr444=5,
    OUTPUT_YCbCr422_16BIT
};
//#define LT9211_OutPutModde  OUTPUT_LVDS_2_PORT
u8 LT9211_OutPutModde=OUTPUT_LVDS_1_PORT;

typedef enum VIDEO_INPUTMODE_ENUM
{
    Input_RGB888,
    Input_YCbCr444,
    Input_YCbCr422_16BIT
}
_Video_Input_Mode;

#define Video_Input_Mode  Input_RGB888

//#define lvds_format_JEIDA

//#define lvds_sync_de_only


//////////option for debug///////////


struct video_timing{
    u16 hfp;
    u16 hs;
    u16 hbp;
    u16 hact;
    u16 htotal;
    u16 vfp;
    u16 vs;
    u16 vbp;
    u16 vact;
    u16 vtotal;
    u32 pclk_khz;
};

enum VideoFormat
{
    video_800x480_60Hz_vic,
    video_1024x768_60Hz_vic,
    video_1280x720_60Hz_vic,
    video_1280x768_60Hz_vic,
    video_1280x800_60Hz_vic,
    video_1366x768_60Hz_vic,
    video_1920x1080_60Hz_vic,
    video_1920x1200_60Hz_vic,
    video_none
};

struct Lane_No{
    u8 swing_high_byte;
    u8 swing_low_byte;
    u8 emph_high_byte;
    u8 emph_low_byte;
};

#endif
