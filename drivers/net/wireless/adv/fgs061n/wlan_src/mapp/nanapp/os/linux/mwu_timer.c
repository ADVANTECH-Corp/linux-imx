/** @file mwu_timer.c
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

#include "mwu_timer.h"
#include "mwu_defs.h"
#include "mwu.h"

#include <sys/time.h>
#include <unistd.h>

int mwu_get_time(struct mwu_timeval *t)
{
	struct timeval tv;

	if (!t)
		return MWU_ERR_INVAL;

	if (gettimeofday(&tv, NULL))
		return MWU_ERR_COM;

	t->sec = tv.tv_sec;
	t->usec = tv.tv_usec;

	return MWU_ERR_SUCCESS;
}

int mwu_clear_time(struct mwu_timeval *t)
{
	if (!t)
		return MWU_ERR_INVAL;

	t->sec = 0;
	t->usec = 0;

	return MWU_ERR_SUCCESS;
}

int mwu_time_is_set(struct mwu_timeval *t)
{
	return (!t || (t->sec == 0 && t->usec == 0)) ? FALSE : TRUE;
}

int mwu_cmp_time(struct mwu_timeval *t1, struct mwu_timeval *t2)
{
	if (!t1 || !t2)
		return 1; /* if input is invalid, simply return 1 */

	return (t1->sec == t2->sec) ? t1->usec - t2->usec : t1->sec - t2->sec;
}

int mwu_add_time(struct mwu_timeval *t1, struct mwu_timeval *t2,
		 struct mwu_timeval *res)
{
	if (!t1 || !t2 || !res)
		return MWU_ERR_INVAL;

	res->sec = t1->sec + t2->sec;
	res->usec = t1->usec + t2->usec;
	if (res->usec >= 1000000) {
		res->usec -= 1000000;
		res->sec += 1;
	}

	return MWU_ERR_SUCCESS;
}

int mwu_sub_time(struct mwu_timeval *t1, struct mwu_timeval *t2,
		 struct mwu_timeval *res)
{
	if (!t1 || !t2 || !res)
		return MWU_ERR_INVAL;

	res->sec = t1->sec - t2->sec;
	res->usec = t1->usec - t2->usec;
	if (res->usec < 0) {
		res->usec += 1000000;
		res->sec -= 1;
	}
	/* If t1 < t2, simply set res to 0 */
	if (res->sec < 0)
		return mwu_clear_time(res);

	return MWU_ERR_SUCCESS;
}

void mwu_sleep(mwu_time_t sec, mwu_time_t usec)
{
	if (sec)
		sleep(sec);
	if (usec)
		usleep(usec);
}

int mwu_thread_sleep(mwu_time_t sec, mwu_time_t usec)
{
	struct timeval tv;
	fd_set rfds;
	int last_fd;

	tv.tv_sec = sec;
	tv.tv_usec = usec;
	FD_ZERO(&rfds);
	last_fd = -1;

	return select(last_fd + 1, &rfds, NULL, NULL, &tv);
}
