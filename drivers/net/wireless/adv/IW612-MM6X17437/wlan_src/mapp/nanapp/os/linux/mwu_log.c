/** @file mwu_Log.c
 *  @brief This file contains functions for debugging print.
 *
 *  Copyright 2012-2020, 2024 NXP
 *
 *  NXP CONFIDENTIAL
 *  The source code contained or described herein and all documents related to
 *  the source code ("Material") are owned by NXP or its
 *  suppliers or licensors. Title to the Material remains with NXP
 *  or its suppliers and licensors. The Material contains trade secrets and
 *  proprietary and confidential information of NXP or its suppliers and
 *  licensors. The Material is protected by worldwide copyright and trade secret
 *  laws and treaty provisions. No part of the Material may be used, copied,
 *  reproduced, modified, published, uploaded, posted, transmitted, distributed,
 *  or disclosed in any way without NXP's prior express written permission.
 *
 *  No license under any patent, copyright, trade secret or other intellectual
 *  property right is granted to or conferred upon you by disclosure or delivery
 *  of the Materials, either expressly, by implication, inducement, estoppel or
 *  otherwise. Any license under such intellectual property rights must be
 *  express and approved by NXP in writing.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include "mwu_log.h"
#include "mwu_timer.h"

/********************************************************
	Local Variables
********************************************************/
static int wps_debug_timestamp = 0;

/********************************************************
	Global Variables
********************************************************/
int mwu_debug_level = DEFAULT_MSG; /* declared in mwu_log.h */

/********************************************************
	Local Functions
********************************************************/

static void mwu_debug_print_timestamp(void)
{
	struct mwu_timeval tv;
	char buf[16];

	if (!wps_debug_timestamp)
		return;

	mwu_get_time(&tv);
	if (strftime(buf, sizeof(buf), "%b %d %H:%M:%S",
		     localtime((const time_t *)&tv.sec)) <= 0) {
		snprintf(buf, sizeof(buf), "%u", (int)tv.sec);
	}
	printf("%s.%06u: ", buf, (unsigned int)tv.usec);
}

/********************************************************
	Global Functions
********************************************************/

void mwu_set_debug_level(int level)
{
	mwu_debug_level = level;
}

#ifndef ANDROID
void mwu_printf(int level, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (level & mwu_debug_level) {
		mwu_debug_print_timestamp();
		vprintf(fmt, ap);
		printf("\n");
		fflush(stdout);
	}
	va_end(ap);
}
#endif

void mwu_hexdump(int level, const char *title, const unsigned char *buf,
		 size_t len)
{
	size_t i, llen;
	const unsigned char *pos = buf;
	const size_t line_len = 16;

	if (level < mwu_debug_level)
		return;

	mwu_debug_print_timestamp();

	if (buf == NULL) {
		printf("%s - hexdump_ascii(len=%lu): [NULL]\n", title,
		       (unsigned long)len);
		return;
	}
	printf("%s - hexdump_ascii(len=%lu):\n", title, (unsigned long)len);
	while (len) {
		llen = len > line_len ? line_len : len;
		printf(" ");
		for (i = 0; i < llen; i++)
			printf(" %02x", pos[i]);
		for (i = llen; i < line_len; i++)
			printf("   ");
		printf("   ");
		for (i = 0; i < llen; i++) {
			if (isprint(pos[i]))
				printf("%c", pos[i]);
			else
				printf("_");
		}
		for (i = llen; i < line_len; i++)
			printf(" ");
		printf("\n");
		pos += llen;
		len -= llen;
	}
}
