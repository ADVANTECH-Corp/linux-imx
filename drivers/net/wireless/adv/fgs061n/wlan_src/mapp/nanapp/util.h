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
/* util.h: common utilities used by mwu modules
 *
 */
#ifndef __UTIL_H__
#define __UTIL_H__

#include "mwu_defs.h"

/* This is defined in wps_util.h, but we need an upper case one.  So we add it
 * here.  We also add UTIL_MAC2STR for completeness.
 */
#define UTIL_MACSTR "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX"
#define UTIL_MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define UTIL_MAC2SCAN(a) &(a)[0], &(a)[1], &(a)[2], &(a)[3], &(a)[4], &(a)[5]
#define BCAST_MAC "\xff\xff\xff\xff\xff\xff"
#define ZERO_MAC "\x00\x00\x00\x00\x00\x00"
#define CLEAR 0
#define SET 1

#define UTIL_IFACEIDSTR "%02hhX%02hhX:%02hhX%02hhX:%02hhX%02hhX:%02hhX%02hhX"
#define UTIL_IFACEID2STR(a)                                                    \
	(a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]

/**
 *  @brief Converts a string to hex value
 *
 *  @param str      A pointer to the string
 *  @param raw      A pointer to the raw data buffer
 *  @return         Number of bytes read
 */
int string2raw(char *str, unsigned char *raw);

/**
 *  @brief Converts a string to hex value
 *
 *  @param str      A pointer to the string
 *  @param raw      A pointer to the raw data buffer
 *  @return         Number of bytes read
 */
int string2raw(char *str, unsigned char *raw);

/*
 *  @brief convert hex char to integer
 *
 *  @param hex      A pointer to hex string
 *  @param buf      buffer to storage the data
 *  @param len
 *  @return         WPS_STATUS_SUCCESS--success, otherwise --fail
 */
int hexstr2bin(const char *hex, u8 *buf, size_t len);

/*
 *  @brief convert binary data to Hex string
 *
 *  @param bin      A pointer to binary data
 *  @param hex      A pointer to hex string
 *  @param len
 *  @return         WPS_STATUS_SUCCESS--success, otherwise --fail
 */

int bin2hexstr(const u8 *bin, s8 *hex, size_t len);

/* convenience macro for couting the elements in a SLIST.  Perhaps an inline
 * function would be better here, but then we have to find a way to pass the
 * type of cursor.
 */
#define SLIST_COUNT(cursor, list, list_item, count)                            \
	do {                                                                   \
		count = 0;                                                     \
		SLIST_FOREACH(cursor, list, list_item)                         \
		{                                                              \
			count++;                                               \
		}                                                              \
	} while (0)

#endif
