/*
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
#ifndef __LOCKF_H__
#define __LOCKF_H__

#include <fcntl.h>

#ifdef ANDROID
#if !HAVE_GLIBC

#define F_ULOCK 1
#define F_LOCK 2
#define F_TLOCK 3
#define F_TEST 4

static int lockf(int fildes, int function, off_t size)
{
	struct flock lock;

	memset(&lock, 0, sizeof(struct flock));
	lock.l_type = F_WRLCK;
	if (fcntl(fildes, F_GETLK, &lock) < 0)
		return -1;

	switch (function) {
	case F_ULOCK:
		lock.l_type = F_UNLCK;
		if (fcntl(fildes, F_SETLK, &lock) < 0)
			return -1;
		break;
	case F_LOCK:
		lock.l_whence = SEEK_CUR;
		lock.l_len = size;
		if (fcntl(fildes, F_SETLK, &lock) < 0)
			return -1;
		break;
	case F_TLOCK:
		if (lock.l_pid != 0) {
			errno = EAGAIN;
			return -1;
		} else {
			lock.l_whence = SEEK_CUR;
			lock.l_len = size;
			if (fcntl(fildes, F_SETLK, &lock) < 0)
				return -1;
		}
		break;
	case F_TEST:
		if (lock.l_pid != 0) {
			errno = EAGAIN;
			return -1;
		}
		break;
	}

	return 0;
}

#endif /* HAVE_GLIBC */
#endif

#endif
