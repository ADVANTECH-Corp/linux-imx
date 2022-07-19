/*****************************************************************************/
/* Copyright (c) 2019 LONTIUM Inc.                                 */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program;                                        */
/*****************************************************************************/
#include <linux/backlight.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>
//#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>
#include "lontium-lt9211.h"

#include <linux/regulator/consumer.h>
#include <linux/media-bus-format.h>

/*******************************************************

   1、LT9211 IIC address is 0X5A(address+ RW Bit) , LT9211 chip ID is 0x18 0x01 0xe1;
   if under Linux，IIC address bit7 is Read/Write flag bit，then I2C_Adr is 0x2D;

   2、Only after MIPI signal send to LT9211 ，then initialize LT9211。

   3、It need the SOC's GPIO to reset LT9211(Pin LT9211_RSTN)；it need to toggle it LOW about 100ms and then HIGH 100ms for reset。

 *********************************************************/
//#define LT9211_DEBUG

#ifdef LT9211_DEBUG
#define lt9211_printk(x...) printk( "[lt9211 DEBUG]: " x )
#else
#define lt9211_printk(x...)
#endif

int hact, vact;
int hs, vs;
int hbp, vbp;
int htotal, vtotal;
int hfp, vfp;
u8 VideoFormat=0;

enum VideoFormat Video_Format;
#define MIPI_LANE_CNT  MIPI_4_LANE // 0: 4lane
#define MIPI_SETTLE_VALUE 0x0a //0x05  0x0a
#define PCR_M_VALUE 0x17 //0x14 , for display quality


                                          //hfp,   hs, hbp, hact,htotal,vfp,  vs, vbp, vact,vtotal,
struct video_timing g_video[] = 
{
    { 24,  72, 104,  800, 1000,   3,  10,  57,  480,  550,  33000},  // 800x480
    { 24, 136, 180, 1024, 1364,   3,   6,  29,  768,  806,  66000},  // 1024x768
    {110,  40, 220, 1280, 1650,   5,   5,  20,  720,  750,  74250},  // 1280x720
    { 64, 128, 192, 1280, 1664,   3,   7,  20,  768,  798,  79500},  // 1280x768
    { 48,  32,  80, 1280, 1440,   3,   6,  50,  800,  859,  74250},  // 1280x800
    { 26, 110, 110, 1366, 1592,  13,   6,  13,  768,  800,  81000},  // 1366x768
    { 88,  44, 148, 1920, 2200,   4,   5,  36, 1080, 1125, 148500},  // 1920x1080
    { 48,  32,  80, 1920, 2080,   3,   6,  26, 1200, 1235, 154000},  // 1920x1200
};


typedef  struct LT9211{
    int reset_pin;
    int enable_pin;
    int gpio_flags;
    int m_rst_delay;
    int blk_pwr_pin;
    int blk_en_pin;
    int m_blk_delay;
    struct work_struct Lt9211_resume_work;
    struct work_struct Lt9211_suspend_work;
    struct backlight_device *backlight;
    struct regulator *supply;
    int bus_format;
}LT9211_info_t;

struct i2c_client *g_client;
LT9211_info_t* g_LT9211;
u32 g_pclk_khz = 0;
//int g_LT9211_probe = 0;

static void lt9211_power_on(LT9211_info_t* lt9211);

static int LT9211_i2c_write_bytes(struct i2c_client *client,uint8_t *data,int len)
{
    struct i2c_msg msg;
    int ret=-1;
    int retries = 0;

    msg.flags=!I2C_M_RD;
    //printk("%s:i2c_addr:0x%02x--reg:0x%02x--val:0x%02x\n", __FUNCTION__, client->addr, *data, *(data+1));
    msg.addr=client->addr;
    msg.len=len;
    msg.buf=data;
    //msg.scl_rate=100 * 1000;

    while(retries<5)
    {
        ret=i2c_transfer(client->adapter,&msg, 1);
        if(ret == 1)break;
        retries++;
    }

    return ret;
}

static int _LT9211_mipi_write(struct i2c_client *client, uint8_t addr, uint8_t val)
{
    uint8_t buf[2] = {addr, val};
    int ret;

    ret = LT9211_i2c_write_bytes(client,buf,2);
    if (ret < 0) {
        //dev_err(&client->dev, "error %d writing to lvds addr 0x%x\n",ret, addr);
        return -1;
    }

    return 0;
}


#define LT9211_mipi_write(client, addr, val) \
do { \
    int ret; \
    ret = _LT9211_mipi_write(client, addr, val); \
} while(0)

static int LT9211_i2c_read_bytes(struct i2c_client *client, uint8_t *buf, int len)
{
    struct i2c_msg msgs[2];
    int ret=-1;
    int retries = 0;

    msgs[0].flags = client->flags;
    msgs[0].addr=client->addr;
    msgs[0].len=1;
    msgs[0].buf=&buf[0];
    //msgs[0].scl_rate=200 * 1000;

    msgs[1].flags = client->flags | I2C_M_RD;
    msgs[1].addr=client->addr;
    msgs[1].len=len-1;
    msgs[1].buf=&buf[1];
    //msgs[1].scl_rate=200 * 1000;

    while(retries<5)
    {
        ret=i2c_transfer(client->adapter,msgs, 2);
        if(ret == 2)break;
        retries++;
    }
    return ret;
}

static int LT9211_read(struct i2c_client *client, uint8_t addr)
{
    int ret;
    unsigned char buf[]={addr,0};
    ret = LT9211_i2c_read_bytes(client,buf,2);
    if (ret < 0){
        //printk("LT9211_read is fail\n");
        goto fail;
    }

    return buf[1];
fail:
    //dev_err(&client->dev, "Error %d reading from subaddress 0x%x\n",ret, addr);
    return -1;
}

static int LT9211_ChipID(struct i2c_client *client)
{
    char val[3]= {0x0, 0x0, 0x0};
	int ret = -1;

	do 
	{
		ret = _LT9211_mipi_write( client, 0xff, 0x81);
	} while(0);

    val[0]=LT9211_read( client,0x00);
    val[1]=LT9211_read( client,0x01);
    val[2]=LT9211_read( client,0x02);
	printk("LT9211 Chip ID:0x%02x 0x%02x 0x%02x",val[0], val[1], val[2]);
	return ret;
}

static void LT9211_SystemInt(struct i2c_client *client)
{
    /* system clock init */   
    LT9211_mipi_write( client, 0xff, 0x82);
    LT9211_mipi_write( client, 0x01, 0x18);

    LT9211_mipi_write( client, 0xff, 0x86);
    LT9211_mipi_write( client, 0x06, 0x61); 
    LT9211_mipi_write( client, 0x07, 0xa8); //fm for sys_clk

    LT9211_mipi_write( client, 0xff, 0x87); //��ʼ�� txpll �Ĵ����б�Ĭ��ֵ������
    LT9211_mipi_write( client, 0x14, 0x08); //default value
    LT9211_mipi_write( client, 0x15, 0x00); //default value
    LT9211_mipi_write( client, 0x18, 0x0f);
    LT9211_mipi_write( client, 0x22, 0x08); //default value
    LT9211_mipi_write( client, 0x23, 0x00); //default value
    LT9211_mipi_write( client, 0x26, 0x0f); 
}

static void LT9211_MipiRxPhy(struct i2c_client *client)
{
    /* Mipi rx phy */
    LT9211_mipi_write( client, 0xff, 0x82);
    LT9211_mipi_write( client, 0x02, 0x44); //port A mipi rx enable

    LT9211_mipi_write( client, 0x05, 0x32); //port A CK lane swap 
    LT9211_mipi_write( client, 0x0d, 0x26);
    LT9211_mipi_write( client, 0x17, 0x0c);
    LT9211_mipi_write( client, 0x1d, 0x0c);

    LT9211_mipi_write( client, 0x0a, 0xf7);
    LT9211_mipi_write( client, 0x0b, 0x77);
#ifdef _Mipi_PortA_ 
    /*port a*/
    LT9211_mipi_write( client, 0x07, 0x9f); //port clk enable  ��ֻ��Portbʱ,porta��lane0 clkҪ�򿪣�
    LT9211_mipi_write( client, 0x08, 0xfc); //port lprx enable
#endif 
#ifdef _Mipi_PortB_
    /*port b*/
    LT9211_mipi_write( client, 0x0f, 0x9f); //port clk enable
    LT9211_mipi_write( client, 0x10, 0xfc); //port lprx enable
    LT9211_mipi_write( client, 0x04, 0xa1);
#endif
    /*port diff swap*/
    LT9211_mipi_write( client, 0x09, 0x01); //port a diff swap
    LT9211_mipi_write( client, 0x11, 0x01); //port b diff swap

    /*port lane swap*/
    LT9211_mipi_write( client, 0xff, 0x86);
    LT9211_mipi_write( client, 0x33, 0x1b); //port a lane swap	1b:no swap
    LT9211_mipi_write( client, 0x34, 0x1b); //port b lane swap 1b:no swap
}

static void LT9211_MipiRxDigital(struct i2c_client *client)
{
    LT9211_mipi_write( client,0xff,0x86);
#ifdef _Mipi_PortA_ 
    LT9211_mipi_write( client,0x30,0x85); //mipirx HL swap
#endif

#ifdef _Mipi_PortB_
    LT9211_mipi_write( client,0x30,0x8f); //mipirx HL swap
#endif

    LT9211_mipi_write( client,0xff,0xD8);
#ifdef _Mipi_PortA_
    LT9211_mipi_write( client,0x16,0x00); //mipirx HL swap 
#endif

#ifdef _Mipi_PortB_
    LT9211_mipi_write( client,0x16,0x80); //mipirx HL swap
#endif

    LT9211_mipi_write( client,0xff,0xd0);
    LT9211_mipi_write( client,0x43,0x12); //rpta mode enable,ensure da_mlrx_lptx_en=0

    LT9211_mipi_write( client,0x02,0x02); //mipi rx controller	//settleֵ
}

static void LT9211_SetVideoTiming(struct i2c_client *client,struct video_timing *video_format)
{
    mdelay(100);
    LT9211_mipi_write( client,0xff,0xd0);
    LT9211_mipi_write( client,0x0d,(u8)(video_format->vtotal>>8)); //vtotal[15:8]
    LT9211_mipi_write( client,0x0e,(u8)(video_format->vtotal)); //vtotal[7:0]
    LT9211_mipi_write( client,0x0f,(u8)(video_format->vact>>8)); //vactive[15:8]
    LT9211_mipi_write( client,0x10,(u8)(video_format->vact)); //vactive[7:0]
    LT9211_mipi_write( client,0x15,(u8)(video_format->vs)); //vs[7:0]
    LT9211_mipi_write( client,0x17,(u8)(video_format->vfp>>8)); //vfp[15:8]
    LT9211_mipi_write( client,0x18,(u8)(video_format->vfp)); //vfp[7:0]

    LT9211_mipi_write( client,0x11,(u8)(video_format->htotal>>8)); //htotal[15:8]
    LT9211_mipi_write( client,0x12,(u8)(video_format->htotal)); //htotal[7:0]
    LT9211_mipi_write( client,0x13,(u8)(video_format->hact>>8)); //hactive[15:8]
    LT9211_mipi_write( client,0x14,(u8)(video_format->hact)); //hactive[7:0]
    LT9211_mipi_write( client,0x16,(u8)(video_format->hs)); //hs[7:0]
    LT9211_mipi_write( client,0x19,(u8)(video_format->hfp>>8)); //hfp[15:8]
    LT9211_mipi_write( client,0x1a,(u8)(video_format->hfp)); //hfp[7:0]

    g_pclk_khz = (u32)(video_format->pclk_khz);
}

static void LT9211_TimingSet(struct i2c_client *client)
{
    u32 hact ;
    u32 vact ;
    char fmt ;
    u32 pa_lpn = 0;
    char read_val=0,read_val1=0;
    int i;
    lt9211_printk("LT9211 LT9211_TimingSet \n");
    mdelay(300);
    LT9211_mipi_write( client,0xff,0xd0);

    read_val=LT9211_read( client,0x82);
    read_val1=LT9211_read( client,0x83);

    hact = (read_val<<8) + read_val1 ;
    hact = hact/3;

    fmt=LT9211_read( client,0x84);
    fmt = fmt & 0x0f;

    read_val=LT9211_read( client,0x85);
    read_val1=LT9211_read( client,0x86);
    vact = (read_val<<8) + read_val1 ;

    pa_lpn=LT9211_read( client,0x9c);
    lt9211_printk("hact = %d\n",hact);
    lt9211_printk("vact = %d\n",vact);
    lt9211_printk("fmt = 0x%x \n", fmt);
    lt9211_printk("pa_lpn = 0x%x \n", pa_lpn);

    mdelay(100);
    for(i = 0; i < video_none; i++)
    {
        if ((hact == g_video[i].hact ) &&( vact == g_video[i].vact ))
        {
            lt9211_printk("video_mode: %d*%d@60Hz.\n", hact, vact);
            VideoFormat = i;
            LT9211_SetVideoTiming(client,&g_video[i]);
            if((hact >=1920 ) &&( vact >=1080 ))
            {
                LT9211_OutPutModde = OUTPUT_LVDS_2_PORT;
                lt9211_printk("LT9211_OutPutModde = %d\n", LT9211_OutPutModde);
            }
            break;
        }
    }

    if(i == video_none)
    {
        VideoFormat = video_none;
        lt9211_printk("video_none \n");
    }
}

static void LT9211_MipiRxPll(struct i2c_client *client)
{
    /* dessc pll */
    LT9211_mipi_write( client,0xff,0x82);
    LT9211_mipi_write( client,0x2d,0x48);
    lt9211_printk("g_pclk_khz = %d \n", g_pclk_khz);
    if(g_pclk_khz < 44000)
    {
        LT9211_mipi_write( client,0x35,0x83);
        lt9211_printk("Set TxPll 0x35,0x83 \n");
    }
    else if(g_pclk_khz > 88000)
    {
        LT9211_mipi_write( client,0x35,0x81);
        lt9211_printk("Set TxPll 0x35,0x81 \n");
    }
    else
    {
        LT9211_mipi_write( client,0x35,0x82);//	PIXCLK_88M_176M= 0x81,PIXCLK_44M_88M = 0x82,	PIXCLK_22M_44M= 0x83   
        lt9211_printk("Set TxPll 0x35,0x82 \n");
    }
}

static void LT9211_MipiPcr(struct i2c_client *client)
{
    u8 loopx;
    u8 pcr_m;
 
    LT9211_mipi_write(client, 0xff,0xd0);
    LT9211_mipi_write(client, 0x0c,0x60);  //fifo position
    LT9211_mipi_write(client, 0x1c,0x60);  //fifo position
    LT9211_mipi_write(client, 0x24,0x70);  //pcr mode( de hs vs)

    LT9211_mipi_write(client,0x2d,0x30); //M up limit
    LT9211_mipi_write(client,0x31,0x0a); //M down limit

    /*stage1 hs mode*/
    LT9211_mipi_write(client,0x25,0xf0);  //line limit
    LT9211_mipi_write(client,0x2a,0x30);  //step in limit
    LT9211_mipi_write(client,0x21,0x4f);  //hs_step
    LT9211_mipi_write(client,0x22,0x00); 

    /*stage2 hs mode*/
    LT9211_mipi_write(client,0x1e,0x01);  //RGD_DIFF_SND[7:4],RGD_DIFF_FST[3:0]
    LT9211_mipi_write(client,0x23,0x80);  //hs_step
    /*stage2 de mode*/
    LT9211_mipi_write(client,0x0a,0x02); //de adjust pre line
    LT9211_mipi_write(client,0x38,0x02); //de_threshold 1
    LT9211_mipi_write(client,0x39,0x04); //de_threshold 2
    LT9211_mipi_write(client,0x3a,0x08); //de_threshold 3
    LT9211_mipi_write(client,0x3b,0x10); //de_threshold 4

    LT9211_mipi_write(client,0x3f,0x04); //de_step 1
    LT9211_mipi_write(client,0x40,0x08); //de_step 2
    LT9211_mipi_write(client,0x41,0x10); //de_step 3
    LT9211_mipi_write(client,0x42,0x20); //de_step 4

    LT9211_mipi_write(client,0x2b,0xa0); //stable out
    mdelay(100);
    LT9211_mipi_write(client,0xff,0xd0);   //enable HW pcr_m
    pcr_m = LT9211_read(client, 0x26);
    pcr_m &= 0x7f;
    LT9211_mipi_write(client,0x26,pcr_m);
    LT9211_mipi_write(client,0x27,0x0f);

    LT9211_mipi_write(client,0xff,0x81);  //pcr reset
    LT9211_mipi_write(client,0x20,0xbf); // mipi portB div issue
    LT9211_mipi_write(client,0x20,0xff);
    mdelay(5);
    LT9211_mipi_write(client,0x0B,0x6F);
    LT9211_mipi_write(client,0x0B,0xFF);

    mdelay(120);//800->120
    for(loopx = 0; loopx < 10; loopx++) //Check pcr_stable 10
    {
        mdelay(200);
        LT9211_mipi_write(client,0xff,0xd0);
        if(LT9211_read(client, 0x87)&0x08)
        {
            lt9211_printk("LT9211 pcr stable \n");
            break;
        }
        else
        {
            lt9211_printk("LT9211 pcr unstable!!!!\n");
        }
    }

    LT9211_mipi_write(client,0xff,0xd0);
    lt9211_printk("LT9211 pcr_stable_M=%x\n",(LT9211_read(client, 0x94)&0x7F));
}

static void LT9211_TxDigital(struct i2c_client *client)
{
    if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde == OUTPUT_LVDS_1_PORT) ) 
    {
        lt9211_printk("LT9211 set to OUTPUT_LVDS \n");
        LT9211_mipi_write( client,0xff,0x85); /* lvds tx controller */
        LT9211_mipi_write( client,0x59,0x50); 
        LT9211_mipi_write( client,0x5a,0xaa); 
        LT9211_mipi_write( client,0x5b,0xaa);
        if( LT9211_OutPutModde == OUTPUT_LVDS_2_PORT )
        {
            LT9211_mipi_write( client,0x5c,0x01);	//lvdstx port sel 01:dual;00:single
        }
        else
        {
            LT9211_mipi_write( client,0x5c,0x01);
        }
        LT9211_mipi_write( client,0x88,0x50);
        LT9211_mipi_write( client,0xa1,0x77); 
        LT9211_mipi_write( client,0xff,0x86);
        LT9211_mipi_write( client,0x40,0x40); //tx_src_sel
        /*port src sel*/
        LT9211_mipi_write( client,0x41,0x34);
        LT9211_mipi_write( client,0x42,0x10);
        LT9211_mipi_write( client,0x43,0x23); //pt0_tx_src_sel
        LT9211_mipi_write( client,0x44,0x41);
        LT9211_mipi_write( client,0x45,0x02); //pt1_tx_src_scl

    //#ifdef lvds_format_JEIDA
        if(g_LT9211->bus_format == MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA)
        {
            LT9211_mipi_write( client,0xff,0x85);
            LT9211_mipi_write( client,0x59,0xd0); 
            LT9211_mipi_write( client,0xff,0xd8);
            LT9211_mipi_write( client,0x11,0x40);
        }
    //#endif
    }
}

static void LT9211_TxPhy(struct i2c_client *client)
{
    LT9211_mipi_write( client,0xff,0x82);
    if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde ==OUTPUT_LVDS_1_PORT) )
    {
         /* dual-port lvds tx phy */
        LT9211_mipi_write( client,0x62,0x00); //ttl output disable
        if(LT9211_OutPutModde == OUTPUT_LVDS_2_PORT)
        {
            LT9211_mipi_write( client,0x3b,0xb8);
        }
        else
        {
            LT9211_mipi_write( client,0x3b,0xb8);
        }
        // HDMI_WriteI2C_Byte(0x3b,0xb8); //dual port lvds enable
        LT9211_mipi_write( client,0x3e,0x92); 
        LT9211_mipi_write( client,0x3f,0x48);
        LT9211_mipi_write( client,0x40,0x31); 
        LT9211_mipi_write( client,0x43,0x80); 
        LT9211_mipi_write( client,0x44,0x00);
        LT9211_mipi_write( client,0x45,0x00); 
        LT9211_mipi_write( client,0x49,0x00);
        LT9211_mipi_write( client,0x4a,0x01);
        LT9211_mipi_write( client,0x4e,0x00);
        LT9211_mipi_write( client,0x4f,0x00);
        LT9211_mipi_write( client,0x50,0x00);
        LT9211_mipi_write( client,0x53,0x00);
        LT9211_mipi_write( client,0x54,0x01);
        LT9211_mipi_write( client,0xff,0x81);
        LT9211_mipi_write( client,0x20,0x7b); 
        LT9211_mipi_write( client,0x20,0xff); //mlrx mltx calib reset
    }
}

static void LT9211_Txpll(struct i2c_client *client)
{
    u8 loopx;
    char val;
    if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde == OUTPUT_LVDS_1_PORT) 
		|| (LT9211_OutPutModde == OUTPUT_RGB888) || (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) )
    {
        LT9211_mipi_write( client,0xff,0x82);
        LT9211_mipi_write( client,0x36,0x01); //b7:txpll_pd
        if( LT9211_OutPutModde == OUTPUT_LVDS_1_PORT )
        {
            LT9211_mipi_write( client,0x37,0x29);
        }
        else
        {
            LT9211_mipi_write( client,0x37,0x2a);
        }
        LT9211_mipi_write( client,0x38,0x06);
        LT9211_mipi_write( client,0x39,0x30);
        LT9211_mipi_write( client,0x3a,0x8e);
        LT9211_mipi_write( client,0xff,0x87);
        LT9211_mipi_write( client,0x37,0x14);
        LT9211_mipi_write( client,0x13,0x00);
        LT9211_mipi_write( client,0x13,0x80);
        mdelay(50);
        for(loopx = 0; loopx < 10; loopx++) //Check Tx PLL cal
        {
            LT9211_mipi_write( client,0xff,0x87);
            val=LT9211_read( client,0x1f);
            if(val & 0x80)
            {
                val=LT9211_read( client,0x20);
                if(val & 0x80)
                {
                    lt9211_printk("LT9211 tx pll lock \n");
                }
                else
                {
                    lt9211_printk("LT9211 tx pll unlocked\n");
                }
                lt9211_printk("LT9211 tx pll cal done\n");
                break;
            }
            else
            {
                lt9211_printk("LT9211 tx pll unlocked\n");
            }
        }
    }
    lt9211_printk("system success\n");
}

#ifdef LT9211_DEBUG
static void LT9211_ClockCheckDebug(struct i2c_client *client)
{
    int fm_value;
    char val=0;
    LT9211_mipi_write( client,0xff,0x86);
    LT9211_mipi_write( client,0x00,0x0a);
    mdelay(100);
    fm_value = 0;
    val=LT9211_read( client,0x08);
    fm_value = (val & 0x0f);
    fm_value = (fm_value<<8) ;
    val=LT9211_read( client,0x09);
    fm_value = fm_value + val;    
    fm_value = (fm_value<<8) ;
    val=LT9211_read( client,0x0a);
    fm_value = fm_value + val;
    lt9211_printk("dessc pixel clock: %d \n",fm_value);

#if 1//MIPIRX_PORTA_BYTE_CLOCK
    LT9211_mipi_write( client,0x00,0x01);
    mdelay(300);
    fm_value = 0;
    fm_value = LT9211_read( client,0x08) &(0x0f);
    fm_value = (fm_value<<8) ;
    fm_value = fm_value + LT9211_read( client,0x09);
    fm_value = (fm_value<<8) ;
    fm_value = fm_value + LT9211_read( client,0x0a);
    printk("\r\nMipiRX portA byte clock: %d\n",fm_value);
    //printdec_u32(fm_value);
#endif
}

static void LT9211_VideoCheckDebug(struct i2c_client *client)
{
    char sync_polarity=0;
    char val=0,val1=0;
    //lt9211_printk(" @@@@@@@@@@@@@@ _____ %s \n",__FUNCTION__);
    LT9211_mipi_write( client,0xff,0x86);
    
    sync_polarity=LT9211_read( client,0x70);

    vs=LT9211_read( client,0x71);

    val=LT9211_read( client,0x72);
    val1=LT9211_read( client,0x73);
    hs = (val<<8) + val1;

    vbp=LT9211_read( client,0x74);
    vfp=LT9211_read( client,0x75);

    val=LT9211_read( client,0x76);
    val1=LT9211_read( client,0x77);
    hbp = (val<<8) + val1;

    val=LT9211_read( client,0x78);
    val1=LT9211_read( client,0x79);
    hfp = (val<<8) + val1;

    val=LT9211_read( client,0x7A);
    val1=LT9211_read( client,0x7B);
    vtotal = (val<<8) + val1;

    val=LT9211_read( client,0x7C);
    val1=LT9211_read( client,0x7D);
    htotal = (val<<8) + val1;
    
    val=LT9211_read( client,0x7E);
    val1=LT9211_read( client,0x7F);
    vact = (val<<8) + val1;

    val=LT9211_read( client,0x80);
    val1=LT9211_read( client,0x81);
    hact = (val<<8) + val1;

    lt9211_printk("sync_polarity = %d\n", sync_polarity);
    lt9211_printk("hfp %d hs %d , hbp %d , hact %d, htotal =%d\n",hfp,hs,hbp,hact,htotal);
    lt9211_printk("vfp %d, vs %d, vbp %d, vact %d, vtotal = %d\n",vfp,vs,vbp,vact,vtotal);
}
#endif

static void lt9211_init(struct i2c_client *client)
{
    int err = 0;
    //lt9211_power_on(g_LT9211);

    //LT9211_ChipID(client);
    LT9211_SystemInt(client);
    LT9211_MipiRxPhy(client);
    LT9211_MipiRxDigital(client); 
    LT9211_TimingSet(client);
    LT9211_MipiRxPll(client);
    LT9211_MipiPcr(client);
    LT9211_TxDigital(client);
    LT9211_TxPhy(client);
    if(g_LT9211->supply)
    {
        err = regulator_enable(g_LT9211->supply);
        lt9211_printk("regulator_enable supply err = %d\n",err);
    }
    mdelay(10);
    LT9211_Txpll(client);

    if(LT9211_OutPutModde == OUTPUT_LVDS_2_PORT)
    {
        LT9211_mipi_write(client, 0xff,0x81);
        LT9211_mipi_write(client, 0x20,0xFB); 
        LT9211_mipi_write(client, 0x20,0xFF); 

        LT9211_mipi_write(client, 0xff,0x81);
        LT9211_mipi_write(client, 0x20,0xF1); 
        LT9211_mipi_write(client, 0x20,0xFF); 

        LT9211_mipi_write(client,0xff,0x81);
        LT9211_mipi_write(client,0x0D,0xfB); 
        mdelay(5);
        LT9211_mipi_write(client,0x0D,0xff); 
    }

#ifdef LT9211_DEBUG
    LT9211_ClockCheckDebug(client);
    LT9211_VideoCheckDebug(client);
#endif

    if (g_LT9211->backlight) 
    {
        if (gpio_is_valid(g_LT9211->blk_pwr_pin))
        {
            lt9211_printk("enable blk_pwr_pin\n");
            gpio_direction_output(g_LT9211->blk_pwr_pin,1);
        }
        msleep(10);

        g_LT9211->backlight->props.power = FB_BLANK_UNBLANK;
        backlight_update_status(g_LT9211->backlight);
        lt9211_printk("enable blk pwm\n");

        msleep(10);
        if (gpio_is_valid(g_LT9211->blk_en_pin))
        {
            lt9211_printk("enable blk_en_pin\n");
            gpio_direction_output(g_LT9211->blk_en_pin,1);
        }
    }
}

static void lt9211_shutdown(struct i2c_client *client)
{
    int err;
    lt9211_printk("LT9211_shutdown enter\n");

    if (g_LT9211->backlight) 
    {

        if (gpio_is_valid(g_LT9211->blk_en_pin))
        {
			lt9211_printk("LT9211_shutdown disable blk_en_pin\n");
            gpio_direction_output(g_LT9211->blk_en_pin,0);
        }
        msleep(10);

        g_LT9211->backlight->props.brightness = 0;
        backlight_update_status(g_LT9211->backlight);
		lt9211_printk("LT9211_shutdown disable blk pwm\n");

        msleep(10);
        if (gpio_is_valid(g_LT9211->blk_pwr_pin))
        {
			lt9211_printk("LT9211_shutdown disable blk_pwr_pin\n");
            gpio_direction_output(g_LT9211->blk_pwr_pin,0);
        }
        msleep(300);
    }

    lt9211_printk("LT9211_shutdown disable LT9211\n");
    gpio_direction_output(g_LT9211->reset_pin, !g_LT9211->gpio_flags);

    if(g_LT9211->enable_pin > 0)
    {
        if (gpio_is_valid(g_LT9211->enable_pin))
        {
            gpio_direction_output(g_LT9211->enable_pin,0);
        }
    }
    msleep(10);
    if(g_LT9211->supply)
    {
        err = regulator_disable(g_LT9211->supply);
        lt9211_printk("regulator_disable supply err = %d\n",err);
    }

}

//EXPORT_SYMBOL(lt9211_init);
//EXPORT_SYMBOL(lt9211_shutdown);
//EXPORT_SYMBOL(g_client);
//EXPORT_SYMBOL(g_LT9211_probe);

static void lt9211_power_on(LT9211_info_t* lt9211)
{
    if(lt9211->enable_pin > 0)
    {
        if (gpio_is_valid(lt9211->enable_pin))
        {
            gpio_direction_output(lt9211->enable_pin,1);
        }
    }
    //gpio_direction_output(lt9211->enable_pin,1);
    //msleep(100);
    //gpio_direction_output(lt9211->reset_pin,1-(lt9211->gpio_flags));
    //if (lt9211->m_rst_delay) 
    //{
    //    printk("LT9211->m_rst_delay : %d\n",lt9211->m_rst_delay);
    //    msleep(lt9211->m_rst_delay);
    //}
    msleep(10);
    gpio_direction_output(lt9211->reset_pin,lt9211->gpio_flags);
}

#if 0
static void lt9211_power_off(LT9211_info_t* lt9211)
{
    gpio_direction_output(lt9211->reset_pin, 0);//1
    msleep(100);
    gpio_direction_output(lt9211->enable_pin,0);
}

static void LT9211_resume_work(struct work_struct *work)
{
    //printk("################## %s \n",__FUNCTION__);
    lt9211_power_on(g_LT9211);
    lt9211_init(g_client);
}

static void LT9211_suspend_work(struct work_struct *work)
{
    //printk("################## %s \n",__FUNCTION__);
    lt9211_power_off(g_LT9211);
}
#endif

static int LT9211_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = -1;
    int val=-1,gpio_flags=-1;
    LT9211_info_t* LT9211 = NULL;
    struct device *dev = &client->dev;
    struct device_node *backlight;
    int bus_format;
    g_client=client;

    lt9211_printk(" mipi to lvds LT9211_probe!!!!!!!!!!!!\n");

    LT9211 = devm_kzalloc(&client->dev, sizeof(*LT9211), GFP_KERNEL);
    if (!LT9211)
        return -ENOMEM;

    g_LT9211=LT9211;

    LT9211->bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG;
    if(!of_property_read_u32(client->dev.of_node, "bus-format",&bus_format))
    {
        LT9211->bus_format = bus_format;
        lt9211_printk("bus format 0x%x\n", bus_format);
    }


    if (!of_property_read_u32(client->dev.of_node, "delay_ms", &val))
        LT9211->m_rst_delay=val;

    LT9211->reset_pin = of_get_named_gpio_flags(client->dev.of_node, "reset_gpio", 0,(enum of_gpio_flags *)&gpio_flags);
    LT9211->enable_pin = of_get_named_gpio_flags(client->dev.of_node, "enable_gpio", 0,(enum of_gpio_flags *)&gpio_flags);
    LT9211->gpio_flags = 1-gpio_flags;

    LT9211->blk_pwr_pin = of_get_named_gpio_flags(client->dev.of_node, "bkl_pwr_gpio", 0,(enum of_gpio_flags *)&gpio_flags);
    LT9211->blk_en_pin = of_get_named_gpio_flags(client->dev.of_node, "bkl_en_gpio", 0,(enum of_gpio_flags *)&gpio_flags);

    backlight = of_parse_phandle(dev->of_node, "backlight", 0);
    if (backlight) 
    {
        LT9211->backlight = of_find_backlight_by_node(backlight);
        of_node_put(backlight);

        if (!LT9211->backlight)
        {
            lt9211_printk("backlight doesn't probe, lt9211 will probe later\n");
            return -EPROBE_DEFER;
        }
    }

    lt9211_printk("LT9211->m_rst_delay : %d\n",LT9211->m_rst_delay);
    lt9211_printk("LT9211->gpio_flags	 :%d\n",LT9211->gpio_flags);
    lt9211_printk("LT9211->reset_pin	 :%d\n",LT9211->reset_pin);
    lt9211_printk("LT9211->enable_pin	 :%d\n",LT9211->enable_pin);
    lt9211_printk("LT9211->blk_pwr_pin	 :%d\n",LT9211->blk_pwr_pin);
    lt9211_printk("LT9211->blk_en_pin	 :%d\n",LT9211->blk_en_pin);
    if(LT9211->enable_pin > 0)
    {
        if (!gpio_is_valid(LT9211->enable_pin))
        {
            printk(" enable_pin err %d %s \n",__LINE__,__func__);
            ret = -EINVAL;
        }
        else
        {
            ret = gpio_request(LT9211->enable_pin, "LT9211_enable_gpio");
            if (ret < 0) 
            {
                printk("%s(): LT9211_rst_gpio request failed %d\n", __func__, ret);
                return ret;
            }
        }
    }

    if(LT9211->reset_pin > 0)
    {
        if (!gpio_is_valid(LT9211->reset_pin))
        {
            printk(" reset_pin err %d %s \n",__LINE__,__func__);
            ret = -EINVAL;
        }
        else
        {
            ret = gpio_request(LT9211->reset_pin, "LT9211_rst_gpio");
            if (ret < 0) {
                printk("%s(): LT9211_rst_gpio request failed %d\n",__func__, ret);
                return ret;
            }
        }
    }

    if(LT9211->blk_pwr_pin > 0)
    {
        if (!gpio_is_valid(LT9211->blk_pwr_pin))
        {
            printk(" bkl_pwr_gpio err %d %s \n",__LINE__,__func__);
            ret = -EINVAL;
        }
        else
        {
            ret = gpio_request(LT9211->blk_pwr_pin, "bkl_pwr_gpio");
            if (ret < 0) {
                printk("%s(): bkl_pwr_gpio request failed %d\n",__func__, ret);
                return ret;
            }
        }
    }

    if(LT9211->blk_en_pin > 0)
    {
        if (!gpio_is_valid(LT9211->blk_en_pin))
        {
            printk(" blk_en_pin err %d %s \n",__LINE__,__func__);
            ret = -EINVAL;
        }
        else
        {
            ret = gpio_request(LT9211->blk_en_pin, "blk_en_pin");
            if (ret < 0) {
                printk("%s(): blk_en_pin request failed %d\n",__func__, ret);
                return ret;
            }
        }
    }

    LT9211->supply = devm_regulator_get(dev, "power");
    if (IS_ERR(LT9211->supply))
    {
        ret = PTR_ERR(LT9211->supply);
        dev_err(dev, "failed to get power regulator: %d\n", ret);
    }

    lt9211_power_on(LT9211);
	ret = LT9211_ChipID(client);
	if(ret < 0)
		return ret;
    lt9211_init(client);
    //if (LT9211->backlight) {
    //LT9211->backlight->props.power = FB_BLANK_UNBLANK;
    //backlight_update_status(LT9211->backlight);
    //}
    //g_LT9211_probe = 1;
    lt9211_printk(" mipi to lvds LT9211_probe end !!!!!!!!!!!!\n");

//    INIT_WORK(&LT9211->Lt9211_resume_work,LT9211_resume_work);
//    INIT_WORK(&LT9211->Lt9211_suspend_work,LT9211_suspend_work);

    return 0;
}

/*
static struct of_device_id lt8911exb_dt_ids[] = {
    { .compatible = "LT9211" },
    { }
};

static struct i2c_device_id lt8911exb_id[] = {
    {"LT9211", 0 },
    { }
};

static int LT9211_suspend(void)
{
    //printk("################## %s \n",__FUNCTION__);
    schedule_work(&g_LT9211->Lt9211_suspend_work);
    return 0;
}

static int LT9211_resume(void)
{
    //printk("################## %s \n",__FUNCTION__);
    schedule_work(&g_LT9211->Lt9211_resume_work);
    return 0;
}

static const struct dev_pm_ops lt8911_pm_ops = {
#ifdef CONFIG_PM_SLEEP
    .suspend = LT9211_suspend,
    .resume = LT9211_resume,
    .poweroff = LT9211_suspend,
    .restore = LT9211_resume,
#endif
};
*/

static const struct of_device_id LT9211_dt_ids[] = {
    {.compatible = "lontium,lt9211",},
    {}
};

static const struct i2c_device_id LT9211_id[] = {
    {"LT9211", 0 },
    { }
};
struct i2c_driver LT9211_driver  = {
    .driver = {
        .owner	= THIS_MODULE,
        //.pm		= &lt8911_pm_ops,
        .name	= "LT9211",
        .of_match_table = of_match_ptr(LT9211_dt_ids),
    },
    .id_table	= LT9211_id,
    .probe   	= LT9211_probe,
    .remove 	= NULL,
    .shutdown	= lt9211_shutdown,
};

static int __init LT9211_init(void)
{
    //printk("%s,%d\n",__func__,__LINE__);
    return i2c_add_driver(&LT9211_driver);
}

static void __exit LT9211_exit(void)
{
    i2c_del_driver(&LT9211_driver);
}

MODULE_AUTHOR("jxye@lontium.com");
module_init(LT9211_init);
module_exit(LT9211_exit);
MODULE_LICENSE("GPL");


