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

#include "mlocation_lib.h"
#include "mwu_log.h"
#include <stdio.h>

mrvl_priv_cmd *mlocation_cmdbuf_alloc(int cmd_len, char *cmd_str, u16 code)
{
	u8 *buffer = NULL;
	u8 *pos = NULL;
	mrvl_cmd_head_buf *cmd;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int len = 0, mrvl_header_len = 0;

	mrvl_header_len = strlen(CMD_NXP) + strlen(cmd_str);
	len = sizeof(mrvl_cmd_head_buf) + cmd_len + mrvl_header_len +
	      sizeof(mrvl_priv_cmd);

	buffer = (u8 *)malloc(len);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return NULL;
	}
	memset(buffer, 0, len);
	pos = buffer;
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = len - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, cmd_str, strlen(cmd_str));

	cmd = (mrvl_cmd_head_buf *)((mrvl_cmd->buf) + mrvl_header_len);
	cmd->cmd_code = code;
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	return mrvl_cmd;
}

// Neeraj: Added to fix wifidir_ioctl not defined issue, but is this really
// needed?
extern int wifidir_ioctl(char *ifname, mrvl_priv_cmd *cmd, u16 *size,
			 u16 buf_size, u16 mrvl_header_size);

enum mlocation_error mlocation_cmdbuf_send(struct mwu_iface_info *cur_if,
					   mrvl_priv_cmd *mrvl_cmd,
					   u16 mrvl_header_size)
{
	mrvl_cmd_head_buf *cmd = NULL;
	int ret;
	u16 cmd_len;
	u16 cmd_code_in;
	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_size);
	cmd_len = cmd->size;
	cmd_code_in = cmd->cmd_code;
	ret = wifidir_ioctl(cur_if->ifname, mrvl_cmd, &cmd_len, cmd_len,
			    mrvl_header_size);
	if (ret != MLOCATION_ERR_SUCCESS) {
		ERR("Interface %s - Failed to send mlocation command to driver.\n",
		    cur_if->ifname);
		return MLOCATION_ERR_COM;
	}

	if (cmd->cmd_code != (cmd_code_in | MLOCATION_CMD_RESP_CHECK)) {
		ERR("Corrupted response from driver!\n");
		return MLOCATION_ERR_COM;
	}

	if (cmd->result != 0) {
		ERR("Non-zero result from driver: %d\n", cmd->result);
		return MLOCATION_ERR_COM;
	}

	return MLOCATION_ERR_SUCCESS;
}
