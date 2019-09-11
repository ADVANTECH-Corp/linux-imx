/*
 * generic videomode helper
 *
 * Copyright (c) 2012 Steffen Trumtrar <s.trumtrar@pengutronix.de>, Pengutronix
 *
 * This file is released under the GPLv2
 */
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#ifdef CONFIG_ARCH_ADVANTECH
#include <linux/ctype.h>

#if 0
extern char *strrchr(const char *, int);
#define __FILENAME__ (strrchr("/" __FILE__, '/') + 1)
#endif

static char lvds_panel[80]="";

static struct panel_videomode {
	char id[80];
	struct videomode vm;
/*
{	.pixelclock=,
	.hactive=, .hfront_porch=, .hback_porch=, .hsync_len=, 
	.vactive=, .vfront_porch=, .vback_porch=, .vsync_len=,
	.flags=
}
*/
} panel_videomodes[] = {
	{"g070vw01v0", {  29500000,  800, 24,  96,  72,  480, 10,  3, 7, 0} },
	{"g150xgel04", {  63500000, 1024, 48, 152, 104,  768, 23,  3, 4, 0} },
	{"g215hvn0",   { 170000000, 1920, 30,  90,  60, 1080, 38,  5, 7, 0} },
};

static int predefined_panels_count=sizeof(panel_videomodes)/sizeof(struct panel_videomode);

static int get_lvds_panel_config(struct device_node *np, struct videomode *vm)
{
	const char *dp;
	char display_panel[80];
	struct panel_videomode *pv;
	int ret, i;
	char *p;

	if (*lvds_panel) {
		strcpy(display_panel, lvds_panel);
		goto processing_panel;
	}
	
	ret = of_property_read_string(np, "display-panel", &dp);
	if (ret < 0) {
		pr_warn("%pOFP: no display-panel specified\n", np);
		return 0;
	}
//	pr_info("%pOFP: display-panel : %s\n", np, dp);
	strcpy(display_panel, dp);

processing_panel:
	for (p=display_panel ; *p; p++) *p = tolower(*p);
	
	for (i=0, pv=panel_videomodes; i< predefined_panels_count; pv++, i++) {
		if (strcmp(display_panel, pv->id) == 0) {
			vm->hactive=pv->vm.hactive;
			vm->hback_porch=pv->vm.hback_porch;
			vm->hfront_porch=pv->vm.hfront_porch;
			vm->hsync_len	=pv->vm.hsync_len;
			vm->vactive=pv->vm.vactive;
			vm->vback_porch=pv->vm.vback_porch;
			vm->vfront_porch=pv->vm.vfront_porch;
			vm->vsync_len=pv->vm.vsync_len;
			vm->pixelclock=pv->vm.pixelclock;
			vm->flags=pv->vm.flags;
			pr_info("%pOFP: %s - %lu - %u %u %u %u - %u %u %u %u - %x", np, display_panel,
				vm->pixelclock,
				vm->hactive, vm->hfront_porch, vm->hback_porch, vm->hsync_len,
				vm->vactive, vm->vfront_porch, vm->vback_porch, vm->vsync_len,
				vm->flags);

			return 1;
		}
	}
	pr_warn("%pOFP: display-panel (%s) not in predefined panels\n", np, lvds_panel);

	return 0;
}
#endif

/**
 * of_get_videomode - get the videomode #<index> from devicetree
 * @np - devicenode with the display_timings
 * @vm - set to return value
 * @index - index into list of display_timings
 *	    (Set this to OF_USE_NATIVE_MODE to use whatever mode is
 *	     specified as native mode in the DT.)
 *
 * DESCRIPTION:
 * Get a list of all display timings and put the one
 * specified by index into *vm. This function should only be used, if
 * only one videomode is to be retrieved. A driver that needs to work
 * with multiple/all videomodes should work with
 * of_get_display_timings instead.
 **/
int of_get_videomode(struct device_node *np, struct videomode *vm,
		     int index)
{
	struct display_timings *disp;
	int ret;

#ifdef CONFIG_ARCH_ADVANTECH
	if (get_lvds_panel_config(np, vm)) return 0;
#endif
	disp = of_get_display_timings(np);
	if (!disp) {
		pr_err("%pOF: no timings specified\n", np);
		return -EINVAL;
	}
	if (index == OF_USE_NATIVE_MODE)
		index = disp->native_mode;

	ret = videomode_from_timings(disp, vm, index);
	display_timings_release(disp);

	return ret;
}

EXPORT_SYMBOL_GPL(of_get_videomode);

#ifdef CONFIG_ARCH_ADVANTECH
static int __init lvds_panel_setup(char *options)
{
	strcpy(lvds_panel, options);
	
	return 1;
}
__setup("lvds_panel=", lvds_panel_setup);
#endif

