/*
 * Copyright (C) 2011-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/init.h>
#include <linux/ipu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mxcfb.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#include "mxc_dispdrv.h"

#ifdef CONFIG_ARCH_ADVANTECH
#ifdef CONFIG_FB_MXC_SYNC_PANEL_EDID
#include <linux/kthread.h>
#include <video/mxc_edid.h>
#include <linux/console.h>
#include <linux/ipu-v3.h>

//extern int enabled_video_mode_ext;
//extern int solo_display;
extern char fb_vga_fix_id[30];
u8 edid[512];
static struct mxc_edid_cfg edid_cfg;
static struct fb_info *fbi = NULL;
static struct task_struct *task_kthread;
#define POLLING_WAIT_TIME_MS    3000
static struct i2c_client *vga_i2c;
static int vga_i2c_initialized = 0;
extern const struct fb_videomode mxc_cea_mode[64];
int first_init_flag = 1;

static const struct fb_videomode xga_mode = {
	/* 13 1024x768-60 VESA */
	NULL, 60, 1024, 768, 15384, 160, 24, 29, 3, 136, 6,
	0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA
};

static const struct fb_videomode sxga_mode = {
	/* 20 1280x1024-60 VESA */
	NULL, 60, 1280, 1024, 9259, 248, 48, 38, 1, 112, 3,
	FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA
};

void vga_edid_rebuild_modelist(void)
{
	struct fb_videomode m;
	const struct fb_videomode *mode;
	int i;

	console_lock();
	//Only Destory the old EDID list when video extension is enabled
	//if (enabled_video_mode_ext || solo_display)
		fb_destroy_modelist(&fbi->modelist);

	for (i = 0; i < fbi->monspecs.modedb_len; i++)
	{
		//not support interface display and bitrate over 170Mhz
		if (!(fbi->monspecs.modedb[i].vmode & FB_VMODE_INTERLACED) && (fbi->monspecs.modedb[i].pixclock > 5882))
		{
			/*
			printk( "Added mode %d:", i);
			printk("xres = %d, yres = %d, freq = %d, vmode = %d, flag = %d\n",
				fbi->monspecs.modedb[i].xres,
				fbi->monspecs.modedb[i].yres,
				fbi->monspecs.modedb[i].refresh,
				fbi->monspecs.modedb[i].vmode,
				fbi->monspecs.modedb[i].flag);
			*/
			fb_add_videomode(&fbi->monspecs.modedb[i], &fbi->modelist);
		}
	}
	console_unlock();

	fb_var_to_videomode(&m, &fbi->var);

	mode = fb_find_nearest_mode(&m, &fbi->modelist);

	if(!mode)
	{
		printk("%s: could not find mode in modelist\n", __func__);
	}
	else if (fb_mode_is_equal(&m, mode))
	{
		printk("vga: video mode is same as previous\n");
	}
	else
	{
		fb_videomode_to_var(&fbi->var, mode);

		fbi->var.activate |= FB_ACTIVATE_FORCE;
		console_lock();
		fbi->flags |= FBINFO_MISC_USEREVENT;
		fb_set_var(fbi, &fbi->var);
		fbi->flags &= ~FBINFO_MISC_USEREVENT;
		console_unlock();
	}
}

void vga_default_modelist(void)
{
	const struct fb_videomode *temp_mode;
	struct fb_videomode m;
	const struct fb_videomode *mode;
	int i;

	console_lock();
	//Only Destory the old EDID list when video extension is enabled
	//if (enabled_video_mode_ext || solo_display)
		fb_destroy_modelist(&fbi->modelist);

	/*Add all no interlaced CEA mode to default modelist */
	for (i = 0; i < ARRAY_SIZE(mxc_cea_mode); i++) {
		temp_mode = &mxc_cea_mode[i];
		if (!(temp_mode->vmode & FB_VMODE_INTERLACED) && (temp_mode->pixclock > 5882))
		{
			/*
			printk( "Added default mode %d:", i);
			printk("xres = %d, yres = %d, freq = %d, vmode = %d, flag = %d\n",
				temp_mode->xres,
				temp_mode->yres,
				temp_mode->refresh,
				temp_mode->vmode,
				temp_mode->flag);
			*/
			fb_add_videomode(temp_mode, &fbi->modelist);
		}
	}

	/*Add XGA and SXGA to default modelist */
	fb_add_videomode(&xga_mode, &fbi->modelist);

	/*
	printk( "Added default mode XGA:");
	printk("xres = %d, yres = %d, freq = %d, vmode = %d, flag = %d\n",
				xga_mode.xres,
				xga_mode.yres,
				xga_mode.refresh,
				xga_mode.vmode,
				xga_mode.flag);
	*/

	fb_add_videomode(&sxga_mode, &fbi->modelist);

	/*
	printk( "Added default mode SXGA:");
	printk("xres = %d, yres = %d, freq = %d, vmode = %d, flag = %d\n",
				sxga_mode.xres,
				sxga_mode.yres,
				sxga_mode.refresh,
				sxga_mode.vmode,
				sxga_mode.flag);
	*/

	console_unlock();

	fb_var_to_videomode(&m, &fbi->var);
	mode = fb_find_nearest_mode(&m, &fbi->modelist);

	if(!mode)
	{
		printk("%s: could not find mode in modelist\n", __func__);
	}
	else if (fb_mode_is_equal(&m, mode))
	{
		printk("vga: video mode is same as previous\n");
	}
	else
	{
		fb_videomode_to_var(&fbi->var, mode);

		fbi->var.activate |= FB_ACTIVATE_FORCE;
		console_lock();
		fbi->flags |= FBINFO_MISC_USEREVENT;
		fb_set_var(fbi, &fbi->var);
		fbi->flags &= ~FBINFO_MISC_USEREVENT;
		console_unlock();
	}
}

static int vga_edid_work(void *data)
{
	int wake_up_flag = 0;
	wait_queue_head_t wait;
	init_waitqueue_head(&wait);

	while(1)
	{
		int i, j, ret;
		u8 edid_old[512];

		for (i = 0; i < num_registered_fb; i++)
		{
			if (strcmp(registered_fb[i]->fix.id, fb_vga_fix_id) == 0)
			{
				fbi = registered_fb[i];

				/* back up edid  */
				memcpy(edid_old, edid, 512);

				ret = mxc_edid_read(vga_i2c->adapter, vga_i2c->addr, edid, &edid_cfg, fbi);

				if (ret < 0)
				{
					memcpy(edid, edid_old, 512);
					//printk("vga: read edid failed\n");
					if(first_init_flag) {
						vga_default_modelist();
					}
				}
				else if(!memcmp(edid_old, edid, 512))
				{
					//printk("vga: same edid\n");
				}
				else if(fbi->monspecs.modedb_len == 0)
				{
					printk("vga:no modes read from edid\n");
				}
				else
				{
					/* receive new edid */
					printk("vga: new edid\n");
					/*
					for (j = 0; j < 512/16; j++)
					{
						for (i = 0; i < 16; i++)
						printk("0x%02X ", edid[j*16 + i]);
						printk("\n");
					}
					*/
					vga_edid_rebuild_modelist();
				}

				if(first_init_flag) {
					first_init_flag = 0;
				}
			}
		}

		wait_event_interruptible_timeout(wait, wake_up_flag == 1, msecs_to_jiffies(POLLING_WAIT_TIME_MS));
	}
	return 0;
}

static int mxc_vga_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE | I2C_FUNC_I2C))
		return -ENODEV;
	printk("**********mxc_vga_i2c_probe\n");
	vga_i2c = client;
	vga_i2c_initialized = 1;

	return 0;
}

static int mxc_vga_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id mxc_vga_i2c_id[] = {
	{ "mxc_vga_i2c", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mxc_vga_i2c_id);

static struct i2c_driver mxc_vga_i2c_driver = {
	.driver = {
		   .name = "mxc_vga_i2c",
	},
	.probe = mxc_vga_i2c_probe,
	.remove = mxc_vga_i2c_remove,
	.id_table = mxc_vga_i2c_id,
};

static int __init mxc_vga_i2c_init(void)
{
	return i2c_add_driver(&mxc_vga_i2c_driver);
}

static void __exit mxc_vga_i2c_exit(void)
{
	i2c_del_driver(&mxc_vga_i2c_driver);
}

module_init(mxc_vga_i2c_init);
module_exit(mxc_vga_i2c_exit);
#endif
#endif


struct mxc_lcd_platform_data {
	u32 default_ifmt;
	u32 ipu_id;
	u32 disp_id;
};

struct mxc_lcdif_data {
	struct platform_device *pdev;
	struct mxc_dispdrv_handle *disp_lcdif;
};

#define DISPDRV_LCD	"lcd"

static struct fb_videomode lcdif_modedb[] = {
	{
	/* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	"CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	FB_SYNC_CLK_LAT_FALL,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	/* 800x480 @ 60 Hz , pixel clk @ 32MHz */
	"SEIKO-WVGA", 60, 800, 480, 29850, 89, 164, 23, 10, 10, 10,
	FB_SYNC_CLK_LAT_FALL,
	FB_VMODE_NONINTERLACED,
	0,},
};
static int lcdif_modedb_sz = ARRAY_SIZE(lcdif_modedb);

static int lcdif_init(struct mxc_dispdrv_handle *disp,
	struct mxc_dispdrv_setting *setting)
{
	int ret, i;
	struct mxc_lcdif_data *lcdif = mxc_dispdrv_getdata(disp);
	struct device *dev = &lcdif->pdev->dev;
	struct mxc_lcd_platform_data *plat_data = dev->platform_data;
	struct fb_videomode *modedb = lcdif_modedb;
	int modedb_sz = lcdif_modedb_sz;

	/* use platform defined ipu/di */
	ret = ipu_di_to_crtc(dev, plat_data->ipu_id,
			     plat_data->disp_id, &setting->crtc);
	if (ret < 0)
		return ret;

	ret = fb_find_mode(&setting->fbi->var, setting->fbi, setting->dft_mode_str,
				modedb, modedb_sz, NULL, setting->default_bpp);
	if (!ret) {
		fb_videomode_to_var(&setting->fbi->var, &modedb[0]);
		setting->if_fmt = plat_data->default_ifmt;
	}

#if defined(CONFIG_ARCH_ADVANTECH) && defined(CONFIG_FB_MXC_SYNC_PANEL_EDID)
	task_kthread = kthread_run(vga_edid_work, NULL, "vga_edid_work");
	if (IS_ERR(task_kthread))
	{
		ret = PTR_ERR(task_kthread);
		printk(KERN_INFO "kthread is  NOT created!!\n");
	}
#else
	INIT_LIST_HEAD(&setting->fbi->modelist);
	for (i = 0; i < modedb_sz; i++) {
		struct fb_videomode m;
		fb_var_to_videomode(&m, &setting->fbi->var);
		if (fb_mode_is_equal(&m, &modedb[i])) {
			fb_add_videomode(&modedb[i],
					&setting->fbi->modelist);
			break;
		}
	}
#endif

	return ret;
}

void lcdif_deinit(struct mxc_dispdrv_handle *disp)
{
	/*TODO*/
}

static struct mxc_dispdrv_driver lcdif_drv = {
	.name 	= DISPDRV_LCD,
	.init 	= lcdif_init,
	.deinit	= lcdif_deinit,
};

static int lcd_get_of_property(struct platform_device *pdev,
				struct mxc_lcd_platform_data *plat_data)
{
	struct device_node *np = pdev->dev.of_node;
	int err;
	u32 ipu_id, disp_id;
	const char *default_ifmt;

	err = of_property_read_string(np, "default_ifmt", &default_ifmt);
	if (err) {
		dev_dbg(&pdev->dev, "get of property default_ifmt fail\n");
		return err;
	}
	err = of_property_read_u32(np, "ipu_id", &ipu_id);
	if (err) {
		dev_dbg(&pdev->dev, "get of property ipu_id fail\n");
		return err;
	}
	err = of_property_read_u32(np, "disp_id", &disp_id);
	if (err) {
		dev_dbg(&pdev->dev, "get of property disp_id fail\n");
		return err;
	}

	plat_data->ipu_id = ipu_id;
	plat_data->disp_id = disp_id;
	if (!strncmp(default_ifmt, "RGB24", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_RGB24;
	else if (!strncmp(default_ifmt, "BGR24", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_BGR24;
	else if (!strncmp(default_ifmt, "GBR24", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_GBR24;
	else if (!strncmp(default_ifmt, "RGB565", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_RGB565;
	else if (!strncmp(default_ifmt, "RGB666", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_RGB666;
	else if (!strncmp(default_ifmt, "YUV444", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_YUV444;
	else if (!strncmp(default_ifmt, "LVDS666", 7))
		plat_data->default_ifmt = IPU_PIX_FMT_LVDS666;
	else if (!strncmp(default_ifmt, "YUYV16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_YUYV;
	else if (!strncmp(default_ifmt, "UYVY16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_UYVY;
	else if (!strncmp(default_ifmt, "YVYU16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_YVYU;
	else if (!strncmp(default_ifmt, "VYUY16", 6))
				plat_data->default_ifmt = IPU_PIX_FMT_VYUY;
	else {
		dev_err(&pdev->dev, "err default_ifmt!\n");
		return -ENOENT;
	}

	return err;
}

static int mxc_lcdif_probe(struct platform_device *pdev)
{
	int ret;
	struct pinctrl *pinctrl;
	struct mxc_lcdif_data *lcdif;
	struct mxc_lcd_platform_data *plat_data;

	dev_dbg(&pdev->dev, "%s enter\n", __func__);
	lcdif = devm_kzalloc(&pdev->dev, sizeof(struct mxc_lcdif_data),
				GFP_KERNEL);
	if (!lcdif)
		return -ENOMEM;
	plat_data = devm_kzalloc(&pdev->dev,
				sizeof(struct mxc_lcd_platform_data),
				GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;
	pdev->dev.platform_data = plat_data;

	ret = lcd_get_of_property(pdev, plat_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "get lcd of property fail\n");
		return ret;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "can't get/select pinctrl\n");
		return PTR_ERR(pinctrl);
	}

	lcdif->pdev = pdev;
	lcdif->disp_lcdif = mxc_dispdrv_register(&lcdif_drv);
	mxc_dispdrv_setdata(lcdif->disp_lcdif, lcdif);

	dev_set_drvdata(&pdev->dev, lcdif);
	dev_dbg(&pdev->dev, "%s exit\n", __func__);

	return ret;
}

static int mxc_lcdif_remove(struct platform_device *pdev)
{
	struct mxc_lcdif_data *lcdif = dev_get_drvdata(&pdev->dev);

	mxc_dispdrv_puthandle(lcdif->disp_lcdif);
	mxc_dispdrv_unregister(lcdif->disp_lcdif);
	kfree(lcdif);
	return 0;
}

static const struct of_device_id imx_lcd_dt_ids[] = {
	{ .compatible = "fsl,lcd"},
	{ /* sentinel */ }
};
static struct platform_driver mxc_lcdif_driver = {
	.driver = {
		.name = "mxc_lcdif",
		.of_match_table	= imx_lcd_dt_ids,
	},
	.probe = mxc_lcdif_probe,
	.remove = mxc_lcdif_remove,
};

static int __init mxc_lcdif_init(void)
{
	return platform_driver_register(&mxc_lcdif_driver);
}

static void __exit mxc_lcdif_exit(void)
{
	platform_driver_unregister(&mxc_lcdif_driver);
}

module_init(mxc_lcdif_init);
module_exit(mxc_lcdif_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("i.MX ipuv3 LCD extern port driver");
MODULE_LICENSE("GPL");
