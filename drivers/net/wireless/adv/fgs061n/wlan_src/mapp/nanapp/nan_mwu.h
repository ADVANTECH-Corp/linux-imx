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

#ifndef __MWU_NAN__
#define __MWU_NAN__

#include "mwu_internal.h"
#include "nan.h"

/* nan_mwu_launch: launch the nan mwu interface
 *
 * returns: whatever return code comes back from
 * mwu_internal_register_module().
 */
enum mwu_error nan_mwu_launch(void);

/* nan_mwu_halt: halt the mwu interface for nan */
void nan_mwu_halt(void);

/* nan_mwu_event_cb: mwu handler for wifidirect events
 *
 * This callback should be passed in the module data structure passed to
 * nan_init().
 */
void nan_mwu_event_cb(struct event *event, void *priv);

#endif
