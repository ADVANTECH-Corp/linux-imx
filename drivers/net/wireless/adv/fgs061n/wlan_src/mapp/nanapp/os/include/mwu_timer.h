/** @file mwu_timer.h
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

#ifndef __MWU_TIMER_H__
#define __MWU_TIMER_H__

typedef long mwu_time_t;

struct mwu_timeval {
	mwu_time_t sec;
	mwu_time_t usec;
};

/**
 * mwu_get_time - Get current time (sec, usec)
 *
 * params
 *     t    pointer to buffer for the time
 *
 * return   MWU_ERR_SUCCESS--success, otherwise--fail
 */
int mwu_get_time(struct mwu_timeval *t);

/**
 * mwu_clear_time - clear time field of t (sec, usec)
 *
 * params
 *     t    pointer to buffer for the time
 *
 * return   MWU_ERR_SUCCESS--success, otherwise--fail
 */
int mwu_clear_time(struct mwu_timeval *t);

/**
 * mwu_time_is_set - return true if either field of t is non-zero
 *
 * params
 *     t    pointer to buffer for the time
 *
 * return   TRUE--true, FALSE--false
 */
int mwu_time_is_set(struct mwu_timeval *t);

/**
 * mwu_cmp_time - compare time values of t1 and t2
 *
 * params
 *     t1    pointer to buffer for the time
 *     t2    pointer to buffer for the time
 *
 * return   >0 -- t1 > t2, 0 -- t1 == t2, <0 -- t1 < t2
 */
int mwu_cmp_time(struct mwu_timeval *t1, struct mwu_timeval *t2);

/**
 * mwu_add_time - add the time values in t1 and t2, and place result into res
 *
 * params
 *     t1    pointer to buffer for the time
 *     t2    pointer to buffer for the time
 *     res   pointer to buffer for the time result
 *
 * return   MWU_ERR_SUCCESS--success, otherwise--fail
 */
int mwu_add_time(struct mwu_timeval *t1, struct mwu_timeval *t2,
		 struct mwu_timeval *res);

/**
 * mwu_sub_time - subtract the time value in t2 from t1, and place result into
 *                res
 *
 * params
 *     t1    pointer to buffer for the time
 *     t2    pointer to buffer for the time
 *     res   pointer to buffer for the time result
 *
 * return   MWU_ERR_SUCCESS--success, otherwise--fail
 */
int mwu_sub_time(struct mwu_timeval *t1, struct mwu_timeval *t2,
		 struct mwu_timeval *res);

/**
 * mwu_sleep - sleep specified seconds/useconds
 *
 * params
 *     sec    seconds to sleep
 *     usec   useconds to sleep
 */
void mwu_sleep(mwu_time_t sec, mwu_time_t usec);

/**
 * mwu_thread_sleep - sleep specified seconds/useconds. This sleep will not
 * block other threads.
 *
 * params
 *     sec    seconds to sleep
 *     usec   useconds to sleep
 *
 * return   0--success, otherwise--fail
 */
int mwu_thread_sleep(mwu_time_t sec, mwu_time_t usec);

#endif
