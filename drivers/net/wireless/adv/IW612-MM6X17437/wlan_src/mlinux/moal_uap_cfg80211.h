/** @file moal_uap_cfg80211.h
 *
 * @brief This file contains the uAP CFG80211 specific defines.
 *
 *
 * Copyright 2011-2021, 2024 NXP
 *
 * NXP CONFIDENTIAL
 * The source code contained or described herein and all documents related to
 * the source code (Materials) are owned by NXP, its
 * suppliers and/or its licensors. Title to the Materials remains with NXP,
 * its suppliers and/or its licensors. The Materials contain
 * trade secrets and proprietary and confidential information of NXP, its
 * suppliers and/or its licensors. The Materials are protected by worldwide
 * copyright and trade secret laws and treaty provisions. No part of the
 * Materials may be used, copied, reproduced, modified, published, uploaded,
 * posted, transmitted, distributed, or disclosed in any way without NXP's prior
 * express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by NXP in writing.
 *
 *  Alternatively, this software may be distributed under the terms of GPL v2.
 *  SPDX-License-Identifier:    GPL-2.0
 *
 */

#ifndef _MOAL_UAP_CFG80211_H_
#define _MOAL_UAP_CFG80211_H_

#include "moal_uap.h"

mlan_status woal_register_uap_cfg80211(struct net_device *dev, t_u8 bss_type);

#ifdef UAP_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
int woal_cfg80211_set_radar_background(struct wiphy *wiphy,
				       struct cfg80211_chan_def *chandef);
#endif
#endif

#endif /* _MOAL_UAP_CFG80211_H_ */
