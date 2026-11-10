/** @file wps_os.h
 *  @brief This file contains definition for timer and socket read functions.
 *
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

#ifndef _WPS_OS_H_
#define _WPS_OS_H_

#include "mwu_defs.h"
#include "wps_msg.h"
#include "mwu_if_manager.h"

/**
 *  @brief Process event handling initialization
 *
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_event_init(char *cfg_path);

/**
 *  @brief Process event handling free
 *
 *  @return             None
 */
void wps_event_deinit(void);

/**
 *  @brief Process main loop initialization
 *
 *  @param none
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_loop_init(void);

/**
 *  @brief Process main loop free
 *
 *  @return             None
 */
void wps_loop_deinit(void);

/**
 *  @brief Main loop procedure for socket read and timer functions
 *
 *  @return             None
 */
void wps_main_loop_proc(void);

/**
 *  @brief Disable main loop
 *
 *  @return         None
 */
void wps_main_loop_shutdown(void);

/**
 *  @brief Enable main loop
 *
 *  @return         None
 */
void wps_main_loop_enable(void);

/**
 *  @brief Register a handler to main loop socket read function
 *
 *  @param sock             Socket number
 *  @param handler          A function pointer of read handler
 *  @param callback_data    Private data for callback
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_register_rdsock_handler(int sock,
				void (*handler)(int sock, void *sock_ctx),
				void *callback_data);

/**
 *  @brief Unregister a handler to main loop socket read function
 *
 *  @param sock     Socket number
 *  @return         None
 */
void wps_unregister_rdsock_handler(int sock);

/**
 *  @brief Register a time-out handler to main loop timer function
 *
 *  @param secs             Time-out value in seconds
 *  @param usecs            Time-out value in micro-seconds
 *  @param handler          A function pointer of time-out handler
 *  @param callback_data    Private data for callback
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_start_timer(unsigned int secs, unsigned int usecs,
		    void (*handler)(void *user_data), void *callback_data);

/**
 *  @brief Cancel time-out handler to main loop timer function
 *
 *  @param handler          Time-out handler to be canceled
 *  @param callback_data    Private data for callback
 *  @return         Number of timer being removed
 */
int wps_cancel_timer(void (*handler)(void *timeout_ctx), void *callback_data);

/**
 *  @brief  Generate 16 Bytes Nonce
 *
 *  @param buf      A pointer to store nonce
 *  @return         None
 */
void wps_generate_nonce_16B(u8 *buf);

/* mwu_l2_init: Initialize l2 socket for l2 packets
 *
 * param:
 *  cur_if      Current interface
 *
 * return:
 *  WPS_STATUS_SUCCESS - Success
 *  WPS_STATUS_FAIL    - Failure
 */
int mwu_l2_init(struct mwu_iface_info *cur_if);

#endif /* _WPS_OS_H_ */
