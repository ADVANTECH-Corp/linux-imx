/*
 *  Copyright 2024-2025 NXP
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

#ifndef __MLOCATION_LIB_H__
#define __MLOCATION_LIB_H__

#include "mwu_defs.h"
#include "util.h"
#include "wlan_hostcmd.h"
#include "mwu_if_manager.h"

#define MLOCATION_CMD_RESP_CHECK 0x8000
#define MLOCATION_HOST_CMD "hostcmd"
#define HostCmd_MLOCATION_SESSION_CONFIG 0x024d
#define HostCmd_MLOCATION_SESSION_CTRL 0x024e
#define HostCmd_MLOCATION_NEIGHBOR_REQ 0x0231

#define MLOCATION_ACTION_FRAME "hostcmd"
#define WIFI_CATEGORY_PUBLIC_ACTION_FRAME 4
#define HostCmd_CMD_802_11_ACTION_FRAME 0x00f4

#define EV_ID_MLOCATION_COMPLETE_EVENT 0x00000086

#define EV_ID_FW_CSI_EVENT 0x0000008D
#define CUS_EVT_MLAN_CSI "EVENT=MLAN_CSI"

#define MLOCATION_INIT_TLV_ID (PROPRIETARY_TLV_BASE_ID + 273)
#define MLOCATION_NTB_RANGING_TLV_ID (PROPRIETARY_TLV_BASE_ID + 343)

#define MLOCATION_RESP_TLV_ID (PROPRIETARY_TLV_BASE_ID + 272)
#define MLOCATION_LOCATION_CIVIC_TLV_ID (PROPRIETARY_TLV_BASE_ID + 271)
#define MLOCATION_LOCATION_CFG_LCI_TLV_ID (PROPRIETARY_TLV_BASE_ID + 270)

enum mlocation_error {
	MLOCATION_ERR_SUCCESS = 0,
	MLOCATION_ERR_BUSY,
	MLOCATION_ERR_INVAL,
	MLOCATION_ERR_NOMEM,
	MLOCATION_ERR_COM,
	MLOCATION_ERR_UNSUPPORTED,
	MLOCATION_ERR_RANGE,
	MLOCATION_ERR_NOENT,
	MLOCATION_ERR_TIMEOUT,
	MLOCATION_ERR_NOTREADY
};

enum mlocation_error mlocation_cmdbuf_send(struct mwu_iface_info *cur_if,
					   mrvl_priv_cmd *mrvl_cmd,
					   u16 mrvl_header_size);

mrvl_priv_cmd *mlocation_cmdbuf_alloc(int cmd_len, char *cmd_str, u16 code);

#endif
