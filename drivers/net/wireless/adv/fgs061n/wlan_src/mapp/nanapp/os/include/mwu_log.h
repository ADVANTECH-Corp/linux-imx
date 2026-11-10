/** @file mwu_log.h
 *  @brief This file contains definitions for debugging print functions.
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

#ifndef __MWU_LOG_H__
#define __MWU_LOG_H__

#ifndef ANDROID
#include <stddef.h>
#endif

/*
 * Debugging function - conditional printf and hex dump.
 * Driver wrappers can use these for debugging purposes.
 */
/** Debug Message : None */
#define MSG_NONE 0x00000000
/** Debug Message : All */
#define DEBUG_ALL 0xFFFFFFFF

/** Debug Message : Message Dump */
#define MSG_MSGDUMP 0x00000001
/** Debug Message : Info */
#define MSG_INFO 0x00000002
/** Debug Message : Warning */
#define MSG_WARNING 0x00000004
/** Debug Message : Error */
#define MSG_ERROR 0x00000008

/** Debug Message : Init */
#define DEBUG_INIT 0x00000010
/** Debug Message : EAPOL */
#define DEBUG_EAPOL 0x00000020
/** Debug Message : WLAN */
#define DEBUG_WLAN 0x00000040
/** Debug Message : Call Flow  */
#define DEBUG_CALL_FLOW 0x00000080

/** Debug Message : WPS Message */
#define DEBUG_WPS_MSG 0x00000100
/** Debug Message : WPS State */
#define DEBUG_WPS_STATE 0x00000200
/** Debug Message : Post Run */
#define DEBUG_POST_RUN 0x00000400
/** Debug Message : Walk Time */
#define DEBUG_WALK_TIME 0x00000800
/** Debug Message : Event */
#define DEBUG_EVENT 0x00001000
/** Debug Message : Vendor TLV */
#define DEBUG_VENDOR_TLV 0x00002000
/** Debug Message : WIFIDIR Discovery */
#define DEBUG_WIFIDIR_DISCOVERY 0x00008000
/** Debug Message : Input */
#define DEBUG_INPUT 0x00010000
/** Debug Message: WPS UPNP*/
#define DEBUG_UPNP 0x00020000

/** Default Debug Message */
#define DEFAULT_MSG                                                            \
	(MSG_NONE | MSG_INFO | MSG_WARNING | MSG_ERROR | MSG_NONE | DEBUG_UPNP)

/**
 *  @brief Set debug level function
 *
 *  @return         None
 */
void mwu_set_debug_level(int level);

/**
 *  @brief Debug print function
 *         Note: New line '\n' is added to the end of the text when printing to
 * stdout.
 *
 *  @param level    Debugging flag
 *  @param fmt      Printf format string, followed by optional arguments
 *  @return         None
 */
#ifdef ANDROID
#define LOG_TAG "mwu"
#include "cutils/log.h"

extern int mwu_debug_level;
#define mwu_printf(level, ...)                                                 \
	do {                                                                   \
		if (level & mwu_debug_level)                                   \
			LOGD(__VA_ARGS__);                                     \
	} while (0)

#undef printf
#define printf(...) mwu_printf(DEBUG_ALL, __VA_ARGS__)

#else
void mwu_printf(int level, char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
#endif

/**
 *  @brief Debug buffer dump function
 *         Note: New line '\n' is added to the end of the text when printing to
 * stdout.
 *
 *  @param level    Debugging flag
 *  @param title    Title of for the message
 *  @param buf      Data buffer to be dumped
 *  @param len      Length of the buf
 *  @return         None
 *
 *  This function is used to print conditional debugging and error messages as
 * both the hex numbers and ASCII characters.
 */
void mwu_hexdump(int level, const char *title, const unsigned char *buf,
		 size_t len);
/** ENTER print */
#define ENTER()                                                                \
	mwu_printf(DEBUG_CALL_FLOW, "Enter: %s, %s:%i", __FUNCTION__,          \
		   __FILE__, __LINE__)
/** LEAVE print */
#define LEAVE()                                                                \
	mwu_printf(DEBUG_CALL_FLOW, "Leave: %s, %s:%i", __FUNCTION__,          \
		   __FILE__, __LINE__)

#ifndef UTIL_LOG_TAG
#define UTIL_LOG_TAG "<?>"
#endif

#define ERR(...) mwu_printf(MSG_ERROR, UTIL_LOG_TAG ": " __VA_ARGS__)
#define WARN(...) mwu_printf(MSG_WARNING, UTIL_LOG_TAG ": " __VA_ARGS__)
#define INFO(...) mwu_printf(MSG_INFO, UTIL_LOG_TAG ": " __VA_ARGS__)

#endif /*__MWU_LOG_H__ */
