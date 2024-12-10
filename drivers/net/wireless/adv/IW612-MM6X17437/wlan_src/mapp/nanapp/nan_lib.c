/*
 *  Copyright 2012-2020 NXP
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

/* nan_lib.c: helper library for NAN module */
#include "nan.h"
#define UTIL_LOG_TAG "NAN"
#include "wps_msg.h" /* for ifreq.h */
#include "util.h"
#include "wlan_hostcmd.h"
#include "nan_lib.h"
#include "mwu.h"
#include "mwu_ioctl.h"
#include "mwu_log.h"
#include "wps_os.h" /* for timer functions */
#include "mwu_timer.h"
#include "mwu_if_manager.h"
//#include "wifidir_lib.h"

#include "discovery_engine.h"
static u16 repeat_interval[] = {
	0, 128, 256, 512, 1024, 2048, 4096, 8192,
};

static u8 convert_awake_dw_interval_to_frame_field(u8 interval);

enum nan_error nancmd_set_fa(struct mwu_iface_info *cur_if,
			     struct nan_params_fa *fa)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_final_bitmap_tlv *fa_map_tlv = NULL;
	nan_avail_map *avail_map = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) + sizeof(nan_final_bitmap_tlv) +
			sizeof(nan_avail_map),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);

	ERR("FA: Interval is %d", fa->interval);
	ERR("FA: Repeat Entry is %d", fa->repeat_entry);
	ERR("FA: Op Class is %d", fa->op_class);
	ERR("FA: Op Chan is %d", fa->op_chan);
	ERR("FA: Availability Map is 0x%08x", fa->availability_map);
	fa_map_tlv = (nan_final_bitmap_tlv *)(nan_params->tlvs);
	fa_map_tlv->tag = NAN_FA_MAP_TLV_ID;
	fa_map_tlv->len =
		sizeof(nan_final_bitmap_tlv) + sizeof(nan_avail_map) - 4;

	avail_map = fa_map_tlv->avail_map;

	avail_map->interval = 16;
	avail_map->repeat_entry = 255;
	avail_map->repeat_interval = 1;
	avail_map->start_offset = 0;
	avail_map->op_class = (u8)fa->op_class;
	avail_map->op_chan = (u8)fa->op_chan;
	avail_map->availability_map = ((u32)fa->availability_map);

	avail_map = (nan_avail_map *)((u8 *)avail_map + sizeof(nan_avail_map));
	mwu_hexdump(MSG_ERROR, "nan fa set", (u8 *)nan_params,
		    sizeof(nan_params_config) + sizeof(nan_final_bitmap_tlv) +
			    (sizeof(nan_avail_map)));
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_set_final_bitmap(struct mwu_iface_info *cur_if,
				       u8 fav_type)
{
	int ret = 0, i, num_entries = 0, k = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_final_bitmap_tlv *final_bitmap_tlv = NULL;
	nan_avail_map *avail_map = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	/*Find number of committed entries*/
	for (i = MAX_SCHEDULE_ENTRIES - 1; i >= 0; i--) {
		if (cur_if->pnan_info->self_avail_info.committed_valid &
		    (1 << i)) {
			num_entries += cur_if->pnan_info->self_avail_info
					       .entry_committed[i]
					       .time_bitmap_count;
		}
	}
	INFO("num entries = %u", num_entries);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) + sizeof(nan_final_bitmap_tlv) +
			(sizeof(nan_avail_map) * num_entries),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);

	final_bitmap_tlv = (nan_final_bitmap_tlv *)(nan_params->tlvs);
	final_bitmap_tlv->tag = NAN_FA_MAP_TLV_ID;
	final_bitmap_tlv->len = sizeof(nan_final_bitmap_tlv) +
				(sizeof(nan_avail_map) * num_entries) -
				4; /* 4 = sieze of cmd header */
	avail_map = final_bitmap_tlv->avail_map;
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (cur_if->pnan_info->self_avail_info.committed_valid &
		    (1 << i)) {
			for (k = 0; k < cur_if->pnan_info->self_avail_info
						.entry_committed[i]
						.time_bitmap_count;
			     k++) {
				u8 temp;
				INFO("avail_map_ptr = 0x%x",
				     avail_map->availability_map);
				avail_map->type = fav_type; /*FA type is NDL*/
				avail_map->interval = 16; // ndp->final_interval;
				avail_map->repeat_entry =
					255; // ndp->final_repeat_entry;
				temp = cur_if->pnan_info->self_avail_info
					       .entry_committed[i]
					       .period;
				avail_map->repeat_interval =
					((repeat_interval[temp]) / 512);
				avail_map->start_offset =
					(cur_if->pnan_info->self_avail_info
						 .entry_committed[i]
						 .start_offset[k] /
					 512);
				avail_map->op_class =
					cur_if->pnan_info->self_avail_info
						.entry_committed[i]
						.op_class;
				avail_map->op_chan =
					cur_if->pnan_info->self_avail_info
						.entry_committed[i]
						.channels[0];

				// We don't want to set the DW window slots in
				// the final bitmap as it may mess up with the
				// synchronization
				avail_map->availability_map =
					((cur_if->pnan_info->self_avail_info
						  .entry_committed[i]
						  .time_bitmap[k]) &
					 (NAN_POTENTIAL_BITMAP));
				// avail_map->availability_map += 2;
				INFO("availability_map = 0x%x",
				     avail_map->availability_map);
				avail_map =
					(nan_avail_map *)((u8 *)avail_map +
							  sizeof(nan_avail_map));
			}
		}
	}
	mwu_hexdump(MSG_ERROR, "nan final_bitmap", (u8 *)nan_params,
		    sizeof(nan_params_config) + sizeof(nan_final_bitmap_tlv) +
			    (sizeof(nan_avail_map) * num_entries));
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

u64 nancmd_get_tsf_for_ulw(struct mwu_iface_info *cur_if, nan_ulw_param *ulw)
{
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_ulw_tsf_tlv *ulw_tlv = 0;
	int tlv_offset = 0;
	u64 tsf = 0;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) + sizeof(nan_ulw_param_tlv),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_GET);
	ulw_tlv = (nan_ulw_tsf_tlv *)(nan_params->tlvs + tlv_offset);
	ulw_tlv->tag = MRVL_NAN_UNALIGNED_SCHEDULE_TLV_ID;

	if (MWU_ERR_SUCCESS !=
	    nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len))
		return NAN_ERR_COM;

	ERR("tlv length %u", ulw_tlv->len);
	tsf = ulw_tlv->tsf;
	ERR("current host tsf is %lld", tsf);
	return tsf;
}

int mwu_add_nan_peer(struct mwu_iface_info *cur_if)
{
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_pmf_and_secure_data_path_tlv *pmf_and_secure_data_path_tlv;
	int tlv_offset = 0;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) +
			sizeof(nan_pmf_and_secure_data_path_tlv),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);
	pmf_and_secure_data_path_tlv =
		(nan_pmf_and_secure_data_path_tlv *)(nan_params->tlvs +
						     tlv_offset);
	pmf_and_secure_data_path_tlv->tag = NAN_PMF_AND_SECURE_DATA_PATH_TLV_ID;
	pmf_and_secure_data_path_tlv->len =
		sizeof(nan_pmf_and_secure_data_path_tlv) -
		(2 * sizeof(u16)); // subtract the size of tlv header
	pmf_and_secure_data_path_tlv->pmf_required =
		cur_if->pnan_info->pmf_required ? 1 : 0;
	pmf_and_secure_data_path_tlv->peer_mac_preset = 1;
	memcpy(pmf_and_secure_data_path_tlv->peerMacAddr,
	       cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0].peer_ndi,
	       MAC_ADDR_LENGTH);

	tlv_offset += sizeof(nan_pmf_and_secure_data_path_tlv);

	if (MWU_ERR_SUCCESS !=
	    nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len)) {
		ERR("Security install : Error sending the NAN peer_mac to firmware\n");
		return NAN_ERR_COM;
	}

	mwu_hexdump(MSG_ERROR, "mwu_add_nan_peer", (u8 *)nan_params,
		    sizeof(nan_params_config) + tlv_offset);

	ERR("Security install : added the NAN peer_mac to firmware\n");

	return NAN_ERR_SUCCESS;
}

enum nan_error nancmd_set_unaligned_sched(struct mwu_iface_info *cur_if,
					  nan_ulw_param *ulw)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_ulw_param_tlv *ulw_tlv = 0;
	int tlv_offset = 0;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) + sizeof(nan_ulw_param_tlv),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);

	ERR("ULW: start time %u", ulw->start_time);
	ERR("ULW: duration %d", ulw->duration);
	ERR("ULW: period %d", ulw->period);
	ERR("ULW: count down %d", ulw->count_down);
	ERR("ULW: channel %d", ulw->channel);
	ERR("ULW: type %d", ulw->avail_type);
	ulw_tlv = (nan_ulw_param_tlv *)(nan_params->tlvs + tlv_offset);
	ulw_tlv->tag = MRVL_NAN_UNALIGNED_SCHEDULE_TLV_ID;
	ulw_tlv->len = sizeof(nan_ulw_param_tlv) - 4; /* 4 = sieze of cmd header
						       */
	ulw_tlv->start_time = ulw->start_time;
	ulw_tlv->duration = ulw->duration;
	ulw_tlv->period = ulw->period;
	ulw_tlv->count_down = ulw->count_down;
	ulw_tlv->overwrite = 1;
	ulw_tlv->blacklist_valid = 0;
	if (ulw->channel) {
		u8 offset = 0, index = 0;
		offset = ulw->channel / 32;
		index = ulw->channel % 32;
		if (ulw->avail_type == 0) {
			/*Add channel to blacklist*/
			memset((u8 *)ulw_tlv->channel_blacklist, 0,
			       (sizeof(u32) * 8));
			ulw_tlv->channel_blacklist[offset] |= (1 << index);
			INFO("channel BL %x",
			     ulw_tlv->channel_blacklist[offset]);
		} else {
			/*Exclude channel from blacklist*/
			memset((u8 *)ulw_tlv->channel_blacklist, 0xff,
			       (sizeof(u32) * 8));
			ulw_tlv->channel_blacklist[offset] &= ~(1 << index);
			INFO("channel BL %x",
			     ulw_tlv->channel_blacklist[offset]);
		}
	} else {
		/*Add all channels to blacklist*/
		memset((u8 *)ulw_tlv->channel_blacklist, 0xff,
		       (sizeof(u32) * 8));
		INFO("All channels added to BL ");
	}
	tlv_offset += sizeof(nan_ulw_param_tlv);

	mwu_hexdump(MSG_ERROR, "nan unaligned sched", (u8 *)nan_params,
		    sizeof(nan_params_config) + tlv_offset);
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_set_ranging_bitmap(struct mwu_iface_info *cur_if,
					 struct nan_params_fa *fa)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_final_bitmap_tlv *fa_map_tlv = NULL;
	nan_avail_map *avail_map = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) + sizeof(nan_final_bitmap_tlv) +
			sizeof(nan_avail_map),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);

	ERR("Ranging: Interval is %d", fa->interval);
	ERR("Ranging: Repeat Entry is %d", fa->repeat_entry);
	ERR("Ranging: Op Class is %d", fa->op_class);
	ERR("Ranging: Op Chan is %d", fa->op_chan);
	ERR("Ranging: Availability Map is 0x%08x", fa->availability_map);
	fa_map_tlv = (nan_final_bitmap_tlv *)(nan_params->tlvs);
	fa_map_tlv->tag = NAN_FA_MAP_TLV_ID;
	fa_map_tlv->len =
		sizeof(nan_final_bitmap_tlv) + sizeof(nan_avail_map) - 4;

	avail_map = fa_map_tlv->avail_map;

	avail_map->type = NAN_RANGE; /*FA type is RANGING*/
	avail_map->interval = 16; // ndp->final_interval;
	avail_map->repeat_entry = 255; // ndp->final_repeat_entry;
	avail_map->repeat_interval = 1;
	avail_map->start_offset = 0;
	avail_map->op_class = (u8)fa->op_class;
	avail_map->op_chan = (u8)fa->op_chan;

	// We don't want to set the DW window slots in the final bitmap as it
	// may mess up with the synchronization
	avail_map->availability_map =
		(((u32)fa->availability_map) & NAN_POTENTIAL_BITMAP);

	avail_map = (nan_avail_map *)((u8 *)avail_map + sizeof(nan_avail_map));
	mwu_hexdump(MSG_ERROR, "nan ranging_bitmap", (u8 *)nan_params,
		    sizeof(nan_params_config) + sizeof(nan_final_bitmap_tlv) +
			    (sizeof(nan_avail_map)));
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_get_state_info(struct mwu_iface_info *cur_if,
				     struct nan_state_info *state)
{
	int ret;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_state_info *state_info = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_STATE_INFO_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(sizeof(nan_state_info), NAN_STATE_INFO_CMD,
				    HostCmd_CMD_NAN_STATE_INFO);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	state_info = (nan_state_info *)cmd->cmd_data;
	state_info->action = wlan_cpu_to_le16(ACTION_GET_CURRENT_PARAMETER);

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	if (ret == NAN_ERR_SUCCESS) {
		INFO("Query state info successful");
		state->hold_role_flag = state_info->hold_role_flag;
		state->cur_role = state_info->cur_role;
		state->hold_master_pref_flag =
			state_info->hold_master_pref_flag;
		state->cur_master_pref = state_info->cur_master_pref;
		state->hold_rfactor_flag = state_info->hold_rfactor_flag;
		state->cur_rfactor = state_info->cur_rfactor;
		state->hold_hop_cnt_flag = state_info->hold_hop_cnt_flag;
		state->cur_hop_cnt = state_info->cur_hop_cnt;
		state->disable_2g = state_info->disable_2g;
	} else {
		INFO("Failed to query state_info");
	}

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_set_state_info(struct mwu_iface_info *cur_if,
				     struct nan_state_info *state)
{
	int ret;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_state_info *state_info = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_STATE_INFO_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(sizeof(nan_state_info), NAN_STATE_INFO_CMD,
				    HostCmd_CMD_NAN_STATE_INFO);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	state_info = (nan_state_info *)cmd->cmd_data;
	state_info->action = wlan_cpu_to_le16(ACTION_SET); /* @TODO: Modify
						     firmware API for non
						     standard action 2 for set,
						     modify as = 0:get, 1:set,
						     2:get_current */

	state_info->hold_role_flag = state->hold_role_flag;
	state_info->cur_role = state->cur_role;
	state_info->hold_master_pref_flag = state->hold_master_pref_flag;
	state_info->cur_master_pref = state->cur_master_pref;
	state_info->hold_rfactor_flag = state->hold_rfactor_flag;
	state_info->cur_rfactor = state->cur_rfactor;
	state_info->hold_hop_cnt_flag = state->hold_hop_cnt_flag;
	state_info->cur_hop_cnt = state->cur_hop_cnt;
	state_info->disable_2g = state->disable_2g;

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_add_service_hash(struct mwu_iface_info *cur_if,
				       unsigned char *computed_service_hash)
{
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_service_hash *service_hash = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SERVICE_HASH_CMD);

	mrvl_cmd =
		nan_cmdbuf_alloc(sizeof(nan_service_hash), NAN_SERVICE_HASH_CMD,
				 HostCmd_CMD_NAN_SERVICE_HASH);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	service_hash = (nan_service_hash *)cmd->cmd_data;
	service_hash->action = wlan_cpu_to_le16(ACTION_ADD);
	memcpy(service_hash->hash, computed_service_hash, SERVICE_HASH_LEN);

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_remove_service_hash(struct mwu_iface_info *cur_if,
					  unsigned char *hash)
{
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_service_hash *service_hash = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SERVICE_HASH_CMD);

	mrvl_cmd =
		nan_cmdbuf_alloc(sizeof(nan_service_hash), NAN_SERVICE_HASH_CMD,
				 HostCmd_CMD_NAN_SERVICE_HASH);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	service_hash = (nan_service_hash *)cmd->cmd_data;
	service_hash->action = wlan_cpu_to_le16(ACTION_REMOVE);
	memcpy(service_hash->hash, hash, SERVICE_HASH_LEN);

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_set_mode_config(struct mwu_iface_info *cur_if, u8 mode)
{
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_mode_config *nan_mode = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_MODE_CONFIG_CMD);

	mrvl_cmd =
		nan_cmdbuf_alloc(sizeof(nan_mode_config), NAN_MODE_CONFIG_CMD,
				 HostCmd_CMD_NAN_MODE_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_mode = (nan_mode_config *)cmd->cmd_data;
	nan_mode->action = wlan_cpu_to_le16(ACTION_SET);
	nan_mode->mode = mode;

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_set_config(struct mwu_iface_info *cur_if,
				 struct nan_params_cfg *cfg)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_opmode_tlv *opmode_tlv = NULL;
	nan_disc_bcn_period_tlv *disc_bcn_period_tlv = NULL;
	nan_scan_dwell_time_tlv *scan_dwell_time_tlv = NULL;
	nan_awake_dw_interval_tlv *awake_dw_interval = NULL;
	nan_pmf_and_secure_data_path_tlv *pmf_and_secure_data_path_tlv = NULL;
	nan_master_pref_tlv *master_pref_tlv = NULL;
	nan_sdf_delay_tlv *sdf_delay_tlv = NULL;
	nan_op_chan_tlv *op_chan_tlv = NULL;

#ifdef NAN1_TESTBED
	nan_high_tsf_tlv *high_tsf_tlv = NULL;
#endif

	nan_warm_up_period_tlv *warm_up_period_tlv = NULL;
	int tlv_offset = 0;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) +
			((cfg->a_band == -1) ? 0 : sizeof(nan_opmode_tlv)) +
			((cfg->disc_bcn_period == -1) ?
				 0 :
				 sizeof(nan_disc_bcn_period_tlv)) +
			((cfg->scan_time == -1) ?
				 0 :
				 sizeof(nan_scan_dwell_time_tlv)) +
			((cfg->master_pref == -1) ?
				 0 :
				 sizeof(nan_master_pref_tlv)) +
			((cfg->sdf_delay == -1) ? 0 :
						  sizeof(nan_sdf_delay_tlv)) +
			((cfg->op_chan_a == -1) ? 0 : sizeof(nan_op_chan_tlv)) +
			((cfg->hightsf == -1) ? 0 : sizeof(nan_high_tsf_tlv)) +
			((cfg->awake_dw_interval == -1) ?
				 0 :
				 sizeof(nan_awake_dw_interval_tlv)) +
			((cfg->pmf_required == -1) ?
				 0 :
				 sizeof(nan_pmf_and_secure_data_path_tlv)) +
			((cfg->warm_up_period == -1) ?
				 0 :
				 sizeof(nan_warm_up_period_tlv)),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);

	if (cfg->a_band != -1) {
		ERR("a_band %d", cfg->a_band);
		opmode_tlv = (nan_opmode_tlv *)nan_params->tlvs;
		opmode_tlv->tag = NAN_OPMODE_TLV_ID;
		opmode_tlv->len = sizeof(u16);
		opmode_tlv->opMode = wlan_cpu_to_le16(cfg->a_band);
		tlv_offset += sizeof(nan_opmode_tlv);
	}

	if (cfg->disc_bcn_period != -1) {
		ERR("disc_bcn_period %d", cfg->disc_bcn_period);
		disc_bcn_period_tlv =
			(nan_disc_bcn_period_tlv *)(nan_params->tlvs +
						    tlv_offset);
		disc_bcn_period_tlv->tag = NAN_DISC_BCN_PERIOD_TLV_ID;
		disc_bcn_period_tlv->len = sizeof(u16);
		disc_bcn_period_tlv->discBcnPeriod =
			wlan_cpu_to_le16(cfg->disc_bcn_period);
		tlv_offset += sizeof(nan_disc_bcn_period_tlv);
	}

	if (cfg->scan_time != -1) {
		ERR("scan_time %d", cfg->scan_time);
		scan_dwell_time_tlv =
			(nan_scan_dwell_time_tlv *)(nan_params->tlvs +
						    tlv_offset);
		scan_dwell_time_tlv->tag = NAN_SCAN_DWELL_TIME_TLV_ID;
		scan_dwell_time_tlv->len = sizeof(u16);
		scan_dwell_time_tlv->scanDwellTime =
			wlan_cpu_to_le16(cfg->scan_time);
		tlv_offset += sizeof(nan_scan_dwell_time_tlv);
	}

	if (cfg->awake_dw_interval != -1) {
		ERR("awake_dw_interval %d", cfg->awake_dw_interval);
		awake_dw_interval =
			(nan_awake_dw_interval_tlv *)(nan_params->tlvs +
						      tlv_offset);
		awake_dw_interval->tag = NAN_AWAKE_DW_INTERVAL_TLV_ID;
		awake_dw_interval->len = sizeof(u16);
		awake_dw_interval->awake_dw_interval =
			wlan_cpu_to_le16(cfg->awake_dw_interval);
		tlv_offset += sizeof(nan_awake_dw_interval_tlv);
	}

	if (cfg->master_pref != -1) {
		ERR("master pref %d", cfg->master_pref);
		master_pref_tlv =
			(nan_master_pref_tlv *)(nan_params->tlvs + tlv_offset);
		master_pref_tlv->tag = NAN_MASTER_INDICATION_TLV_ID;
		master_pref_tlv->len = sizeof(u16);
		master_pref_tlv->master_pref =
			wlan_cpu_to_le16(cfg->master_pref);
		tlv_offset += sizeof(nan_master_pref_tlv);
	}

	if (cfg->sdf_delay != -1) {
		ERR("sdf delay %d", cfg->sdf_delay);
		sdf_delay_tlv =
			(nan_sdf_delay_tlv *)(nan_params->tlvs + tlv_offset);
		sdf_delay_tlv->tag = NAN_SDF_DELAY_TLV_ID;
		sdf_delay_tlv->len = sizeof(u16);
		sdf_delay_tlv->sdf_delay = wlan_cpu_to_le16(cfg->sdf_delay);
		tlv_offset += sizeof(nan_sdf_delay_tlv);
	}

	if (cfg->op_chan_a != -1) {
		ERR("Op Channel in A band is %d", cfg->op_chan_a);
		op_chan_tlv =
			(nan_op_chan_tlv *)(nan_params->tlvs + tlv_offset);
		op_chan_tlv->tag = NAN_OP_CHAN_TLV_ID;
		op_chan_tlv->len = sizeof(u16);
		op_chan_tlv->op_chan_g = wlan_cpu_to_le16(6);
		op_chan_tlv->op_chan_a = wlan_cpu_to_le16(cfg->op_chan_a);
		tlv_offset += sizeof(nan_op_chan_tlv);
	}

	if (cfg->hightsf != -1) {
#ifdef NAN1_TESTBED
		ERR("High TSF flag is %d", cfg->hightsf);
		high_tsf_tlv =
			(nan_high_tsf_tlv *)(nan_params->tlvs + tlv_offset);
		high_tsf_tlv->tag = NAN_HIGH_TSF_TLV_ID;
		high_tsf_tlv->len = sizeof(u8);
		high_tsf_tlv->high_tsf = (u8)cfg->hightsf;
		tlv_offset += sizeof(nan_high_tsf_tlv);
#else
		/* Please note - HighTSF is the TestBed specific feature and is
		 * no more supported in MERGE_CM firmwares. So remove it from
		 * mainline MWU. However it is still supported under WFA branch
		 */
		ERR("ALERT: HighTSF configuration is not supported with this release");
		cfg->hightsf = -1;
#endif
	}

	if (cfg->warm_up_period != -1) {
		ERR("WarmUp period is %ds", cfg->warm_up_period);
		warm_up_period_tlv =
			(nan_warm_up_period_tlv *)(nan_params->tlvs +
						   tlv_offset);
		warm_up_period_tlv->tag = NAN_WARMUP_TIME_TLV_ID;
		warm_up_period_tlv->len = sizeof(u16);
		warm_up_period_tlv->warm_up_period = (u16)cfg->warm_up_period;
		tlv_offset += sizeof(nan_warm_up_period_tlv);
	}

	/* these configuration are managed at MWU level (FW is not configured
	 * here ) */
	if (cfg->include_fa_attr != -1) {
		ERR("Include FA attr is %d", cfg->include_fa_attr);
		cur_if->pnan_info->include_fa_attr = cfg->include_fa_attr;
	}
	if (cfg->process_fa_attr != -1) {
		ERR("process FA attr is %d", cfg->process_fa_attr);
		cur_if->pnan_info->process_fa_attr = cfg->process_fa_attr;
	}

	/* Store awake_dw_interval to be used in SDF device_capability
	 * attribute*/
	if (cfg->awake_dw_interval != -1) {
		ERR("awake DW interval attr is %d", cfg->awake_dw_interval);
		cur_if->pnan_info->awake_dw_interval =
			convert_awake_dw_interval_to_frame_field(
				cfg->awake_dw_interval);
	}

	/* Store data_path_needed to be used in SDF device_capability
	 * attribute*/
	if (cfg->data_path_needed != -1) {
		ERR("Data path needed attr is %d", cfg->data_path_needed);
		cur_if->pnan_info->data_path_needed = cfg->data_path_needed;
	}

	/* Store ranging_required to be used in SDF SDEA attribute*/
	if (cfg->ranging_required != -1) {
		ERR("ranging_required attr is %d", cfg->ranging_required);
		cur_if->pnan_info->ranging_required = cfg->ranging_required;
	}

	/* Store security_required to be used in SDF device_capability
	 * attribute*/
	if (cfg->security != -1) {
		ERR("security_required attr is %d", cfg->security);
		cur_if->pnan_info->security_required = cfg->security;
	}

	/* Store pmf_required to be used in SDF device_capability attribute*/
	if (cfg->pmf_required != -1) {
		pmf_and_secure_data_path_tlv =
			(nan_pmf_and_secure_data_path_tlv *)(nan_params->tlvs +
							     tlv_offset);
		pmf_and_secure_data_path_tlv->tag =
			NAN_PMF_AND_SECURE_DATA_PATH_TLV_ID;
		pmf_and_secure_data_path_tlv->len = sizeof(u16);
		pmf_and_secure_data_path_tlv->pmf_required =
			wlan_cpu_to_le16(cfg->pmf_required);
		pmf_and_secure_data_path_tlv->peer_mac_preset = 0;
		tlv_offset += sizeof(nan_pmf_and_secure_data_path_tlv);
		ERR("pmf_required attr is %d", cfg->pmf_required);
		cur_if->pnan_info->pmf_required = cfg->pmf_required;
	}

	if (cfg->dual_map != -1) {
		ERR("dual_map attr is %d", cfg->dual_map);
		cur_if->pnan_info->dual_map = cfg->dual_map;
	}

	if (cfg->qos_enabled != -1) {
		ERR("qos_enabled attr is %d", cfg->qos_enabled);
		cur_if->pnan_info->qos_enabled = cfg->qos_enabled;
	}

	if (cfg->invalid_sched_required != -1) {
		ERR("invalid_sched_required attr is %d",
		    cfg->invalid_sched_required);
		cur_if->pnan_info->invalid_sched_required =
			cfg->invalid_sched_required;
	}

	if (cfg->set_apple_dut_test != -1) {
		ERR("set_apple_dut_test attr is %d", cfg->set_apple_dut_test);
		cur_if->pnan_info->set_apple_dut_test = cfg->set_apple_dut_test;
	}

	/* Store data_path_needed to be used in SDF device_capability
	 * attribute*/
	if (cfg->data_path_type != -1) {
		ERR("Data path type attr is %d", cfg->data_path_type);
		cur_if->pnan_info->data_path_type = cfg->data_path_type;
	}

	if (cfg->ndpe_attr_supported != -1) {
		ERR("ndpe_attr_supported is %d", cfg->ndpe_attr_supported);
		cur_if->pnan_info->ndpe_attr_supported =
			cfg->ndpe_attr_supported;
	}
	if (cfg->ndpe_attr_protocol != -1) {
		ERR("ndpe_attr_protocol is %d", cfg->ndpe_attr_protocol);
		cur_if->pnan_info->ndpe_attr_protocol = cfg->ndpe_attr_protocol;
	}
	if (cfg->ndpe_attr_iface_identifier != -1) {
		ERR("ndpe_attr_iface_identifier is %d",
		    cfg->ndpe_attr_iface_identifier);
		cur_if->pnan_info->ndpe_attr_iface_identifier =
			cfg->ndpe_attr_iface_identifier;
	}
	if (cfg->ndpe_attr_trans_port != -1) {
		ERR("ndpe_attr_trans_port is %d", cfg->ndpe_attr_trans_port);
		cur_if->pnan_info->ndpe_attr_trans_port =
			cfg->ndpe_attr_trans_port;
	}
	if (cfg->ndpe_attr_negative != -1) {
		ERR("ndpe_attr_negative is %d", cfg->ndpe_attr_negative);
		cur_if->pnan_info->ndpe_attr_negative = cfg->ndpe_attr_negative;
	}
	if (cfg->ndp_attr_present != -1) {
		ERR("ndp_attr_present is %d", cfg->ndp_attr_present);
		cur_if->pnan_info->ndp_attr_present = cfg->ndp_attr_present;
	}

	mwu_hexdump(MSG_ERROR, "nan params", (u8 *)nan_params,
		    sizeof(nan_params_config) + tlv_offset);
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nancmd_config_download(struct mwu_iface_info *cur_if,
				      struct nan_cfg *cfg)
{
	int ret;
	mrvl_cmd_head_buf *cmd = NULL;
	nan_params_config *nan_params = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	nan_opmode_tlv *opmode_tlv = NULL;
	nan_disc_bcn_period_tlv *disc_bcn_period_tlv = NULL;
	nan_scan_dwell_time_tlv *scan_dwell_time_tlv = NULL;
	nan_master_pref_tlv *master_pref_tlv = NULL;
	int tlv_offset = 0;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_PARAMS_CONFIG_CMD);

	mrvl_cmd = nan_cmdbuf_alloc(
		sizeof(nan_params_config) + sizeof(nan_opmode_tlv) +
			sizeof(nan_disc_bcn_period_tlv) +
			sizeof(nan_scan_dwell_time_tlv) +
			sizeof(nan_master_pref_tlv),
		NAN_PARAMS_CONFIG_CMD, HostCmd_CMD_NAN_PARAMS_CONFIG);
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	nan_params = (nan_params_config *)cmd->cmd_data;
	nan_params->action = wlan_cpu_to_le16(ACTION_SET);

	opmode_tlv = (nan_opmode_tlv *)nan_params->tlvs;
	opmode_tlv->tag = NAN_OPMODE_TLV_ID;
	opmode_tlv->len = sizeof(u16);
	opmode_tlv->opMode = wlan_cpu_to_le16(cfg->a_band);
	tlv_offset += sizeof(nan_opmode_tlv);

	disc_bcn_period_tlv =
		(nan_disc_bcn_period_tlv *)(nan_params->tlvs + tlv_offset);
	disc_bcn_period_tlv->tag = NAN_DISC_BCN_PERIOD_TLV_ID;
	disc_bcn_period_tlv->len = sizeof(u16);
	disc_bcn_period_tlv->discBcnPeriod =
		wlan_cpu_to_le16(cfg->disc_bcn_period);
	tlv_offset += sizeof(nan_disc_bcn_period_tlv);

	scan_dwell_time_tlv =
		(nan_scan_dwell_time_tlv *)(nan_params->tlvs + tlv_offset);
	scan_dwell_time_tlv->tag = NAN_SCAN_DWELL_TIME_TLV_ID;
	scan_dwell_time_tlv->len = sizeof(u16);
	scan_dwell_time_tlv->scanDwellTime = wlan_cpu_to_le16(cfg->scan_time);
	tlv_offset += sizeof(nan_scan_dwell_time_tlv);

	master_pref_tlv =
		(nan_master_pref_tlv *)(nan_params->tlvs + tlv_offset);
	master_pref_tlv->tag = NAN_MASTER_INDICATION_TLV_ID;
	master_pref_tlv->len = sizeof(u16);
	master_pref_tlv->master_pref = wlan_cpu_to_le16(cfg->master_pref);
	tlv_offset += sizeof(nan_master_pref_tlv);

	mwu_hexdump(MSG_ERROR, "nan params", (u8 *)nan_params,
		    sizeof(nan_params_config) + tlv_offset);

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

mrvl_priv_cmd *nan_cmdbuf_alloc(int cmd_len, char *cmd_str, u16 code)
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
// RD : To be cleanup

/** 4 byte header to store buf len*/
#define BUF_HEADER_SIZE 4
/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE 1024
/** Host Command ID bit mask (bit 11:0) */
#define HostCmd_CMD_ID_MASK 0x0fff
/** WIFIDIRCMD response check */
#define WIFIDIRCMD_RESP_CHECK 0x8000

enum wifidir_error {
	WIFIDIR_ERR_SUCCESS = 0,
	WIFIDIR_ERR_BUSY,
	WIFIDIR_ERR_INVAL,
	WIFIDIR_ERR_NOMEM,
	WIFIDIR_ERR_COM,
	WIFIDIR_ERR_UNSUPPORTED,
	WIFIDIR_ERR_RANGE,
	WIFIDIR_ERR_NOENT,
	WIFIDIR_ERR_TIMEOUT,
	WIFIDIR_ERR_NOTREADY,
};

/** WIFIDIRCMD buffer */
typedef struct _wifidircmdbuf {
	/** Command header */
	mrvl_cmd_head_buf cmd_head;
} __ATTRIB_PACK__ wifidircmdbuf;

int wifidir_ioctl(char *ifname, mrvl_priv_cmd *cmd, u16 *size, u16 buf_size,
		  u16 mrvl_header_size)
{
	wifidircmdbuf *header = NULL;

	if (buf_size < *size) {
		mwu_printf(MSG_WARNING,
			   "buf_size should not less than cmd buffer size\n");
		return WIFIDIR_ERR_INVAL;
	}
	/* Locate actual command buffer after MRVL private command header */
	header = (wifidircmdbuf *)((u8 *)cmd->buf + mrvl_header_size);

	*(u32 *)header = buf_size - BUF_HEADER_SIZE;

	header->cmd_head.size = *size - BUF_HEADER_SIZE;
	endian_convert_request_header(header->cmd_head);

	if (mwu_privcmd(ifname, (u8 *)cmd) == MWU_ERR_SUCCESS) {
		endian_convert_response_header(header->cmd_head);
		header->cmd_head.cmd_code &= HostCmd_CMD_ID_MASK;
		header->cmd_head.cmd_code |= WIFIDIRCMD_RESP_CHECK;
		*size = header->cmd_head.size;

		/* Validate response size */
		if (*size > (buf_size - BUF_HEADER_SIZE)) {
			mwu_printf(
				MSG_WARNING,
				"ERR:Response size (%d) greater than buffer size (%d)! Aborting!\n",
				*size, buf_size);
			return WIFIDIR_ERR_COM;
		}
	} else {
		return WIFIDIR_ERR_COM;
	}

	return WIFIDIR_ERR_SUCCESS;
}

enum mwu_error nan_cmdbuf_send(struct mwu_iface_info *cur_if,
			       mrvl_priv_cmd *mrvl_cmd, u16 mrvl_header_size)
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
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Interface %s - Failed to send wifidir command to driver.\n",
		    cur_if->ifname);
		return NAN_ERR_COM;
	}

	if (cmd->cmd_code != (cmd_code_in | NAN_CMD_RESP_CHECK)) {
		ERR("Corrupted response from driver!\n");
		return NAN_ERR_COM;
	}

	if (cmd->result != 0) {
		ERR("Non-zero result from driver: %d\n", cmd->result);
		return NAN_ERR_COM;
	}

	return NAN_ERR_SUCCESS;
}

static u8 convert_awake_dw_interval_to_frame_field(u8 interval)
{
	/*For attribute field value (2.4GHZ DW) n,
	 * Awake DW interval calculated as 2^(n-1)*/

	switch (interval) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 4:
		return 3;
	case 8:
		return 4;
	case 16:
		return 5;
	default:
		return 0;
	}
}
