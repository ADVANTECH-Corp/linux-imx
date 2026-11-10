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
/* nan_sm.h: Internal API for calling the NAN state machine
 *
 * The NAN state machine handles NAN events that come from the
 * driver.  This driver stream emits events of many types.  We should be able
 * to register our state machine as a handler for events of type
 * EV_ID_NAN_GENERIC, but that API doesn't exist yet.  So we call the state
 * machine explicitly.  For this, we need a proper header file.
 */

#ifndef __NAN_SM_H__
#define __NAN_SM_H__

#include "wps_def.h"
#include "mwu_if_manager.h"

#define NAN_EVENT_SUB_TYPE_SD_EVENT 0
#define NAN_EVENT_SUB_TYPE_NAN_STARTED 1
#define NAN_EVENT_SUB_TYPE_SDF_TX_DONE 2

/*
 * @brief process NAN events and advance the state machine
 *
 * This is the main NAN state machine.  It handles events from different event
 * spaces.  Events from the NAN_EVENTS_DRIVER space come from the driver and
 * have ID == EV_ID_NAN_GENERIC.
 *
 * Calls to this function must be serialized.  At this time, we depend on the
 * fact that there is a single event loop to serialize this.  If multiple event
 * sources emerge, some sort of locking will have to be implemented.
 *
 * @param cur_if      Pointer to current interface structure
 * @param buffer      Pointer to received buffer
 * @param size        Length of the received event data
 * @param es          Name of the event space from which the incoming event
 *                    was gathered.
 * @return            N/A
 */
int nan_state_machine(struct mwu_iface_info *cur_if, u8 *buffer, u16 size,
		      int es);

enum {
	NAN_EVENTS_DRIVER,
	NAN_EVENTS_UI,
	NAN_EVENTS_DRIVER_SDF_TX_DONE,
};

#endif
