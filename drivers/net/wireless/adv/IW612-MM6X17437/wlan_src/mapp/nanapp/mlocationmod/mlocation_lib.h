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
