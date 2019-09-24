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

#define SCAN_PROGRESSIVE 0
#define SCAN_INTERLACED DISPLAY_FLAGS_INTERLACED
#define SYNC_POL_HV_PP (DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH)
#define SYNC_POL_HV_NN (DISPLAY_FLAGS_HSYNC_LOW  | DISPLAY_FLAGS_VSYNC_LOW )
#define SYNC_POL_HV_PN (DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_LOW )
#define SYNC_POL_HV_NP (DISPLAY_FLAGS_HSYNC_LOW  | DISPLAY_FLAGS_VSYNC_HIGH)

#define FLAG_DELIM ','

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
	{"lvds_vmode",  {         0,    0,   0,   0,   0,    0,  0,  0, 0, 0} },
	{"g070vw01v0",  {  29500000,  800,  24,  96,  72,  480, 10,  3, 7, 0} },
	{"g150xgel04",  {  63500000, 1024,  48, 152, 104,  768, 23,  3, 4, 0} },
	{"g215hvn0",    { 170000000, 1920,  30,  90,  60, 1080, 38,  5, 7, 0} },
	{"640x480@60",  {  25175000,  640,  16,  48,  96,  480, 10, 33, 2, SCAN_PROGRESSIVE | SYNC_POL_HV_NN} },
	{"800x600@60",  {  36000000,  800,  24, 128,  72,  600,  1, 22, 2, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
	{"1024x600@60", {  43970000, 1024,  50,  78,  32,  600,  2, 11, 6, 0} },
	{"1024x768@60", {  65000000, 1024,  24, 160, 136,  768,  3, 29, 6, SCAN_PROGRESSIVE | SYNC_POL_HV_NN} },
	{"1280x720@60", {  74250000, 1280, 110, 220,  40,  720,  5, 20, 5, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
	{"1280x800@60", {  71000000, 1280,  48,  80,  32,  800,  3, 14, 6, SCAN_PROGRESSIVE | SYNC_POL_HV_PN} },
//	{"1280x960@60", { 108000000, 1280,  96, 312, 112,  960,  1, 36, 3, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
	{"1280x1024@60",{ 108000000, 1280,  48, 248, 112, 1024,  1, 38, 3, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
//	{"1360x768@60", {  85500000, 1360,  64, 256, 112,  768,  3, 18, 6, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
	{"1366x768@60", {  85500000, 1366,  70, 213, 143,  768,  3, 24, 3, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
//	{"1600x1200@60",{ 162000000, 1600,  64, 304, 192, 1200,  1, 46, 3, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
	{"1920x1080@60",{ 148500000, 1920,  88, 148,  44, 1080,  4, 36, 5, SCAN_PROGRESSIVE | SYNC_POL_HV_PP} },
	{}
};

//static int predefined_panels_count=sizeof(panel_videomodes)/sizeof(struct panel_videomode);

static struct flag_directive {
    enum display_flags flag;
    char *directive;
} flag_directives[] = {
    {DISPLAY_FLAGS_HSYNC_LOW,       "HSYNC_LOW"},
    {DISPLAY_FLAGS_HSYNC_HIGH,      "HSYNC_HIGH"},
    {DISPLAY_FLAGS_VSYNC_LOW,       "VSYNC_LOW"},
    {DISPLAY_FLAGS_VSYNC_HIGH,      "VSYNC_HIGH"},
    {DISPLAY_FLAGS_DE_LOW,          "DE_LOW"},
    {DISPLAY_FLAGS_DE_HIGH,         "DE_HIGH"},
    {DISPLAY_FLAGS_PIXDATA_POSEDGE, "PIXDATA_POSEDGE"},
    {DISPLAY_FLAGS_PIXDATA_NEGEDGE, "PIXDATA_NEGEDGE"},
    {DISPLAY_FLAGS_INTERLACED,      "INTERLACED"},
    {DISPLAY_FLAGS_DOUBLESCAN,      "DOUBLESCAN"},
    {DISPLAY_FLAGS_DOUBLECLK,       "DOUBLECLK"},
    {DISPLAY_FLAGS_SYNC_POSEDGE,    "SYNC_POSEDGE"},
    {DISPLAY_FLAGS_SYNC_NEGEDGE,    "SYNC_NEGEDGE"},
};

static int get_lvds_panel_config(struct device_node *np, struct videomode *vm)
{
	const char *dp;
	char display_panel[80];
	struct panel_videomode *pv;
	int ret;

	if (panel_videomodes[0].vm.pixelclock) {
		if (strcmp(lvds_panel, "lvds_vmode"))
			printk("lvds_vmode already defined, 'lvds_panel=%s' ignored\n", lvds_panel);
		strcpy(lvds_panel, "lvds_vmode");
	}
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
	
	for (pv=panel_videomodes; pv->id[0]; pv++) {
		if (strcasecmp(display_panel, pv->id) == 0) {
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
			pr_info("%pOFP: %s - %lu,%u,%u,%u,%u,%u,%u,%u,%u,%x", np, display_panel,
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

static u32 parse_flag_directives(char *directives)
{
	const int totals=sizeof(flag_directives)/sizeof(struct flag_directive);
	u32 flags=0;
	char str[128], *p=str, *pS;

	strcpy(str, directives);
	str[strlen(str)]=FLAG_DELIM;
	while (p && *p) {
		int i, matched=0;

		pS=p; p=strchr(pS,FLAG_DELIM);
		if (p) { *p='\0'; p++; }
		if (! *pS) continue;
		for (i=0; i < totals; i++) {
			if ( strcasecmp(flag_directives[i].directive, pS) ) continue;
			flags |= flag_directives[i].flag;
			matched=1;
			break;
		}
		if (!matched) printk("lvds_vmode - unknown flag directive - %s\n", pS);
	}
	return flags;
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

static int __init lvds_vmode_setup(char *options)
{
	struct videomode *vm=&panel_videomodes[0].vm;
	int *Vptr=(int *)(&panel_videomodes[0].vm.hactive);
	int i, ret;
	char *p=options;

	i=0;
	ret=sscanf(p,"%lu",&vm->pixelclock);
	while (p && *p) {
		if (i==0) ret=sscanf(p,"%lu",&vm->pixelclock);
		else ret=sscanf(p,"%d",Vptr+i-1);
		p=strchr(p,',');
		if (p) p++ ;
		if (++i > 8) break;  // last one (9th)
	}

	for (i=0, ret=1; *(p+i); i++) if (p[i] < '0' || p[i] > '9') { ret=0; break; }

	if (ret) {
        sscanf(p,"%d",Vptr+8);
	} else {
//		printk("lvds_vmode - flags : directive(s) conversion\n");
        *(Vptr+8)=parse_flag_directives(p);
		printk("lvds_vmode - flags : %u (%s)\n", *(Vptr+8), p);
	}

	strcpy(lvds_panel, "lvds_vmode");

	return 1;
}

__setup("lvds_panel=", lvds_panel_setup);
__setup("lvds_vmode=", lvds_vmode_setup);
#endif

