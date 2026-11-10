/** @file wps_eapol.h
 *  @brief This file contains definition for EAPOL functions.
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

#ifndef _WPS_EAPOL_H_
#define _WPS_EAPOL_H_

#include "wps_def.h"
#include "mwu_if_manager.h"

/** IEEE 802.1x header */
struct ieee802_1x_hdr {
	/** Version */
	u8 version;
	/** Type */
	u8 type;
	/** Length */
	u16 length;
	/* followed by length octets of data */
} __attribute__((packed));

enum {
	IEEE802_1X_TYPE_EAP_PACKET = 0,
	IEEE802_1X_TYPE_EAPOL_START = 1,
	IEEE802_1X_TYPE_EAPOL_LOGOFF = 2,
	IEEE802_1X_TYPE_EAPOL_KEY = 3,
	IEEE802_1X_TYPE_EAPOL_ENCAPSULATED_ASF_ALERT = 4
};

void wps_rx_eapol(const u8 *src_addr, const u8 *buf, size_t len,
		  struct WPS_L2_INFO *l2);
int wps_eapol_send(struct mwu_iface_info *cur_if, int type, u8 *buf,
		   size_t len);
int wps_eapol_txStart(struct mwu_iface_info *cur_if);
int wps_eapol_txPacket(struct mwu_iface_info *cur_if, u8 *buf, size_t len);

#endif /* _WPS_EAPOL_H_ */
