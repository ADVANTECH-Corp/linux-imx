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

/* nan_lib.h: helper library for NAN module */

#ifndef __NAN_LIB_H__
#define __NAN_LIB_H__
#include "mwu_if_manager.h"
#include "wlan_hostcmd.h"
#include "nan.h"

#define NAN_CMD_RESP_CHECK 0x8000
#define NAN_PARAMS_CONFIG_CMD "hostcmd"
#define NAN_MODE_CONFIG_CMD "hostcmd"
#define NAN_STATE_INFO_CMD "hostcmd"
#define NAN_SERVICE_HASH_CMD "hostcmd"
#define NAN_SDF_CMD "hostcmd"
#define NAN_MODE_START 2
#define NAN_MODE_STOP 1
#define NAN_MODE_RESET 0

#ifdef NAN1_TESTBED
#define HostCmd_CMD_NAN_PARAMS_CONFIG 0x0211
#define HostCmd_CMD_NAN_MODE_CONFIG 0x0213
#define HostCmd_CMD_NAN_STATE_INFO 0x0300
#define HostCmd_CMD_NAN_SERVICE_HASH 0x0216
#define HostCmd_CMD_NAN_SDF 0x0215
#else
#define HostCmd_CMD_NAN_PARAMS_CONFIG 0x0228
#define HostCmd_CMD_NAN_MODE_CONFIG 0x0229
#define HostCmd_CMD_NAN_STATE_INFO 0x022c
#define HostCmd_CMD_NAN_SERVICE_HASH 0x022b
#define HostCmd_CMD_NAN_SDF 0x022a
#endif

#define ACTION_GET_CURRENT_PARAMETER 2
#define ACTION_ADD 1
#define ACTION_REMOVE 2
#define SERVICE_HASH_LEN 6

#define PUBLISH_ID_START 32
#define SUBSCRIBE_ID_START 64

#define NDP_REQ_ID_START 128

#define EV_ID_NAN_GENERIC_EVENT 0x00000075

#ifdef NAN1_TESTBED
#define NAN_MASTER_INDICATION_TLV_ID 0x01ca
#define NAN_DISC_BCN_PERIOD_TLV_ID 0x01c8
#define NAN_OPMODE_TLV_ID 0x01c7
#define NAN_SCAN_DWELL_TIME_TLV_ID 0x01c9
#define NAN_SDF_DELAY_TLV_ID 0x01cb
#define NAN_OP_CHAN_TLV_ID 0x01cf
#define NAN_HIGH_TSF_TLV_ID 0x01d0
#define NAN_WARMUP_TIME_TLV_ID 0x01d1
#define NAN_FA_MAP_TLV_ID 0x01ce
#else
#define NAN_MASTER_INDICATION_TLV_ID (PROPRIETARY_TLV_BASE_ID + 213)
#define NAN_DISC_BCN_PERIOD_TLV_ID (PROPRIETARY_TLV_BASE_ID + 211)
#define NAN_OPMODE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 210)
#define NAN_SCAN_DWELL_TIME_TLV_ID (PROPRIETARY_TLV_BASE_ID + 212)
#define NAN_SDF_DELAY_TLV_ID (PROPRIETARY_TLV_BASE_ID + 214)
#define NAN_OP_CHAN_TLV_ID (PROPRIETARY_TLV_BASE_ID + 216)
#define NAN_WARMUP_TIME_TLV_ID (PROPRIETARY_TLV_BASE_ID + 217)
#define NAN_FA_MAP_TLV_ID (PROPRIETARY_TLV_BASE_ID + 215)
#endif

#define NAN_AWAKE_DW_INTERVAL_TLV_ID (PROPRIETARY_TLV_BASE_ID + 218)
#define MRVL_NAN_UNALIGNED_SCHEDULE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 219)
#define NAN_PMF_AND_SECURE_DATA_PATH_TLV_ID (PROPRIETARY_TLV_BASE_ID + 221)

typedef struct _nan_master_pref_tlv {
	u16 tag;
	u16 len;
	u16 master_pref;
} __ATTRIB_PACK__ nan_master_pref_tlv;

/** HostCmd_CMD_NAN_SERVICE_HASH structure */
typedef struct _nan_service_hash {
	/** Action */
	u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	u8 hash[SERVICE_HASH_LEN];
} __ATTRIB_PACK__ nan_service_hash;

typedef struct _nan_sdf_delay_tlv {
	u16 tag;
	u16 len;
	u16 sdf_delay;
} __ATTRIB_PACK__ nan_sdf_delay_tlv;

typedef struct _nan_op_chan_tlv {
	u16 tag;
	u16 len;
	u8 op_chan_g;
	u8 op_chan_a;
} __ATTRIB_PACK__ nan_op_chan_tlv;

typedef struct _nan_high_tsf_tlv {
	u16 tag;
	u16 len;
	u8 high_tsf;
} __ATTRIB_PACK__ nan_high_tsf_tlv;

typedef struct _nan_warm_up_period_tlv {
	u16 tag;
	u16 len;
	u16 warm_up_period;
} __ATTRIB_PACK__ nan_warm_up_period_tlv;

typedef struct _nan_avail_map {
	u8 type;
	u8 interval;
	u8 repeat_entry;
	u8 repeat_interval;
	u8 start_offset;
	u8 op_class;
	u8 op_chan;
	u32 availability_map;
} __ATTRIB_PACK__ nan_avail_map;

typedef struct _nan_final_bitmap_tlv {
	u16 tag;
	u16 len;
	nan_avail_map avail_map[0];
} __ATTRIB_PACK__ nan_final_bitmap_tlv;

typedef struct _nan_ulw_tsf_tlv {
	u16 tag;
	u16 len;
	u64 tsf;
} __ATTRIB_PACK__ nan_ulw_tsf_tlv;

typedef struct _nan_ulw_param_tlv {
	u16 tag;
	u16 len;
	u32 start_time;
	u32 duration;
	u32 period;
	u8 count_down;
	u8 overwrite;
	u8 blacklist_valid;
	u32 channel_blacklist[8];
} __ATTRIB_PACK__ nan_ulw_param_tlv;

typedef struct _nan_awake_dw_interval_tlv {
	u16 tag;
	u16 len;
	u16 awake_dw_interval;
} __ATTRIB_PACK__ nan_awake_dw_interval_tlv;

typedef struct _nan_pmf_and_secure_data_path_tlv {
	u16 tag;
	u16 len;
	u16 pmf_required;
	u8 peer_mac_preset;
	u8 peerMacAddr[MAC_ADDR_LENGTH];
} __ATTRIB_PACK__ nan_pmf_and_secure_data_path_tlv;

typedef struct _nan_opmode_tlv {
	u16 tag;
	u16 len;
	u16 opMode;
} __ATTRIB_PACK__ nan_opmode_tlv;

typedef struct _nan_disc_bcn_period_tlv {
	u16 tag;
	u16 len;
	u16 discBcnPeriod;
} __ATTRIB_PACK__ nan_disc_bcn_period_tlv;

typedef struct _nan_scan_dwell_time_tlv {
	u16 tag;
	u16 len;
	u16 scanDwellTime;
} __ATTRIB_PACK__ nan_scan_dwell_time_tlv;

/** HostCmd_CMD_NAN_STATE_INFO structure */
typedef struct _nan_params_config {
	/** Action */
	u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	u8 tlvs[0];
} __ATTRIB_PACK__ nan_params_config;

/** HostCmd_CMD_NAN_PARAMS_CONFIG structure */
typedef struct _nan_state_info {
	/** Action */
	u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	u8 hold_role_flag;
	u8 cur_role;
	u8 hold_master_pref_flag;
	u8 cur_master_pref;
	u8 hold_rfactor_flag;
	u8 cur_rfactor;
	u8 hold_hop_cnt_flag;
	u8 cur_hop_cnt;
	u8 reserved1;
} __ATTRIB_PACK__ nan_state_info;

/** HostCmd_CMD_NAN_MODE_CONFIG structure */
typedef struct _nan_mode_config {
	/** Action */
	u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	u8 mode;
} __ATTRIB_PACK__ nan_mode_config;

typedef struct _nan_generic_event {
	u8 peer_mac[ETH_ALEN];
	u32 rssi;
	u8 entire_sd_frame[0];
} __ATTRIB_PACK__ nan_generic_event;

mrvl_priv_cmd *nan_cmdbuf_alloc(int cmd_len, char *cmd_str, u16 code);

enum mwu_error nan_cmdbuf_send(struct mwu_iface_info *cur_if,
			       mrvl_priv_cmd *mrvl_cmd, u16 mrvl_header_size);

enum nan_error nancmd_set_mode_config(struct mwu_iface_info *cur_if,
				      u8 nan_mode);
enum nan_error nancmd_config_download(struct mwu_iface_info *cur_if,
				      struct nan_cfg *cfg);
enum nan_error nancmd_get_state_info(struct mwu_iface_info *cur_if,
				     struct nan_state_info *state);
enum nan_error nancmd_set_state_info(struct mwu_iface_info *cur_if,
				     struct nan_state_info *state);
enum nan_error nancmd_add_service_hash(struct mwu_iface_info *cur_if,
				       unsigned char *computed_service_hash);
enum nan_error nancmd_remove_service_hash(struct mwu_iface_info *cur_if,
					  unsigned char *service_hash);
void nan_driver_event(char *ifname, u8 *buffer, u16 size);
enum nan_error nancmd_set_config(struct mwu_iface_info *cur_if,
				 struct nan_params_cfg *cfg);
enum nan_error nancmd_set_fa(struct mwu_iface_info *cur_if,
			     struct nan_params_fa *fa);

enum nan_error nancmd_set_unaligned_sched(struct mwu_iface_info *cur_if,
					  nan_ulw_param *ulw);
u64 nancmd_get_tsf_for_ulw(struct mwu_iface_info *cur_if, nan_ulw_param *ulw);
enum nan_error nancmd_set_ranging_bitmap(struct mwu_iface_info *cur_if,
					 struct nan_params_fa *fa);

#endif
