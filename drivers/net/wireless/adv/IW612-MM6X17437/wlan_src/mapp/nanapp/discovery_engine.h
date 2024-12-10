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

#ifndef __DISCOVERY_ENGINE_H__
#define __DISCOVERY_ENGINE_H__

#include "nan.h"
/*#include "nan_lib.h"*/
#include "queue.h"

#include "data_engine.h"

#define PUBLISH 1
#define SUBSCRIBE 2
#define FOLLOW_UP 3
#define NAN_CATEGORY_PUBLIC_ACTION_FRAME 4

#define NAN_SD_FRAME 0x13

#define NAN_ACTION_FRAME 0x18

// Action frame subtypes
#define RANGING_REQUEST 1
#define RANGING_RESPONSE 2
#define RANGING_TERMINATION 3
#define RANGING_REPORT 4
#define NDP_REQ 5
#define NDP_RESP 6
#define NDP_CONFIRM 7
#define NDP_KEY_INSTALL 8
#define NDP_TERMINATE 9
#define SCHEDULE_REQUEST 10
#define SCHEDULE_RESPONSE 11
#define SCHEDULE_CONFIRM 12
#define SCHEDULE_UPDATE 13

#define DEFAULT_BITMAP1 0x7ffffffe
#define PREFERRED_BITMAP1 0x7fff0000
#define PREFERRED_BITMAP2 0x0000fffe
#define RANDOM_BITMAP1 0x00ff0000
#define RANDOM_BITMAP2 0x0000ff00
#define BITMAP_RESET 0x00000000

#define NDP_FRAME 1

#define SD_FRAME 0

#define BLOOM_FILTER_SIZE 5 // 32 bits

#define PUBLISH_TYPE_UNSOLICITED (1 << 0)
#define PUBLISH_TYPE_SOLICITED (1 << 1)

#define SUBSCRIBE_TYPE_UNSOLICITED (1 << 0)
#define SUBSCRIBE_TYPE_SOLICITED (1 << 1)

#define SUBSCRIBE_TYPE_ACTIVE (1 << 0)

#define TX_TYPE_BCAST 1
#define TX_TYPE_UCAST 0

#define SERVICE_CTRL_BITMAP_PUBLISH (0)
#define SERVICE_CTRL_BITMAP_SUBSCRIBE (1 << 0)
#define SERVICE_CTRL_BITMAP_FOLLOW_UP (1 << 1)

#define SERVICE_CTRL_BITMAP_MF_PRESENT (1 << 2);
#define SERVICE_CTRL_BITMAP_SRF_PRESENT (1 << 3);

#define SERVICE_CTRL_BITMAP_DISC_RANGE_LTD (1 << 5);

#define NAN_RSSI_LOW_THRESHOLD                                                 \
	(-60) /* low rssi value. So any rssi < NAN_RSSI_LOW would be           \
		 considered as far device */
#define NAN_ATTR_HDR_LEN 3

#define NAN_SERVICE_DISCRIPTOR_ATTR 0x03
#define NAN_FURTHER_AVAIL_MAP_ATTR 0x0a
#define NAN_FURTHER_SD_ATTR 0x09
#define NAN_FURTHER_P2P_ATTR 0x06
#define NAN_RANGING_ATTR 0x0c

#define NAN_DEVICE_CAPA_ATTR 0x0f
#define NAN_SERVICE_DISC_EXT_ATTR 0x0e
#define NAN_AVAILABILITY_ATTR 0x12
#define NAN_RANGING_INFO_ATTR 0x1a
#define NAN_RANGING_SETUP_ATTR 0x1b
#define SERVICE_DESC_EXT_ATTR 0x0e
#define NAN_RANGING_FTM_REPORT_ATTR 0x1c

#define SDEA_CTRL_BITMAP_DATAPATH_REQUIRED (1 << 2)
#define SDEA_CTRL_RANGING_REQUIRED 0x0080
#define SDEA_CTRL_QOS_REQUIRED (1 << 5)

#define SDEA_CTRL_BITMAP_SECURITY_REQUIRED (1 << 6)
/** HostCmd_CMD_WIFIDIR_ACTION_FRAME request */
typedef struct _nan_sd_frame {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	u32 ttl;
	u8 tx_type; /* 1 - NAN Action Frame (Main SD Queue), 2 - Further avail
		       probe Tx frame (alternate queue in FW) */
	u8 category;
	u8 action;
	u8 oui[3];
	u8 oui_type;
	u8 sd_payload_frame[0];
} __ATTRIB_PACK__ nan_sd_frame;

typedef struct _nan_ndp {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	u32 ttl;
	u8 tx_type; /* 1 - NAN Action Frame (Main SD Queue), 2 - Further avail
		       probe Tx frame (alternate queue in FW) */
	u8 category;
	u8 action;
	u8 oui[3];
	u8 oui_type;
	u8 subtype;
	u8 ndp_payload_frame[0];
} __ATTRIB_PACK__ nan_ndp;

typedef struct _nan_action_frame {
	u8 peer_mac_addr[ETH_ALEN];
	u32 ttl;
	u8 tx_type; /* 1 - NAN Action Frame (Main SD Queue), 2 - Further avail
		       probe Tx frame (alternate queue in FW) */
	u8 category;
	u8 action;
	u8 oui[3];
	u8 oui_type;
	u8 oui_sub_type;
	u8 information_content[0];
} __ATTRIB_PACK__ nan_action_frame;

typedef struct _nan_ranging_info_attr {
	u8 attribute_id;
	u16 len;
	u8 location_info;
	u8 last_movement_indication[0];
} __ATTRIB_PACK__ nan_ranging_info_attr;

typedef struct _ftm_error_entry {
	u16 start_time : 4;
	u16 bssid : 6;
	u16 err_code;
} __ATTRIB_PACK__ ftm_error_entry;

typedef struct _ftm_range_entry {
	u8 start_time[4];
	u8 bssid[6];
	u32 range : 24;
	u8 exponent;
	u8 reserved;
} __ATTRIB_PACK__ ftm_range_entry;

typedef struct _ftm_range_report {
	u8 range_entry_count;
	ftm_range_entry range_entry;
	u8 error_entry_count;
	ftm_error_entry error_entry;
	u8 optional[0];
} __ATTRIB_PACK__ ftm_range_report;

typedef struct _nan_ranging_ftm_report_attr {
	u8 attribute_id;
	u16 len;
	u8 range_report[0];
} __ATTRIB_PACK__ nan_ranging_ftm_report_attr;

typedef struct _nan_ranging_control {
	u8 report_required : 1;
	u8 ftm_params_present : 1;
	u8 bitmap_params_present : 1;
	u8 reserved : 5;
} __ATTRIB_PACK__ nan_ranging_control;

typedef struct _nan_ranging_setup_attr {
	u8 attribute_id;
	u16 len;
	u8 dialog_token;
	u8 type_and_status; // Bit 0 to Bit 3: Type subfield  And  Bit 4 to Bit
			    // 7: Status subfield
	u8 reason_code;
	nan_ranging_control ranging_control;
	//    NAN_FTM_PARAMS nan_ftm_params;
	//    u8 ranging_schedule_entry[0];
} __ATTRIB_PACK__ nan_ranging_setup_attr;

typedef struct _nan_service_descrciptor_attribute {
	u8 attribute_id;
	u16 len;
	u8 service_hash[SERVICE_HASH_LEN];
	u8 instance_id; /*publish_id or subscribe_id */
	u8 requester_instance_id;
	u8 service_control_bitmap;
	/*   u8 binding_bitmap;*/
	u8 sda_var_attr[0];
} __ATTRIB_PACK__ nan_service_descriptor_attr;

typedef struct _nan_fa_availability_entry_attr {
	u8 ctrl;
	u8 op_class;
	u8 chan_num;
	u32 availability_btmp;
} __ATTRIB_PACK__ nan_fa_availability_entry_attr;

typedef struct _nan_fa_sd_attr {
	u8 attr_id;
	u16 len;
	u8 map_ctrl;
	u32 availability_btmp;
} __ATTRIB_PACK__ nan_fa_sd_attr;

typedef struct _nan_fa_attr {
	u8 attribute_id;
	u16 len;
	u8 map_id;
	nan_fa_availability_entry_attr e;
} __ATTRIB_PACK__ nan_fa_attr;

typedef struct _nan_fa_p2p_attr {
	u8 attribute_id;
	u16 len;
	u8 device_role;
	u8 mac[6];
	u8 map_ctrl;
	u32 availability_map;
} __ATTRIB_PACK__ nan_fa_p2p_attr;

typedef struct _nan_ranging_attr {
	u8 attribute_id;
	u16 len;
	u8 mac[6];
	u8 map_ctrl;
	u8 proto;
	u32 availability_btmp;
} __ATTRIB_PACK__ nan_ranging_attr;

typedef struct _nan_service_desc_ext_attr {
	u8 attribute_id;
	u16 len;
	u8 instance_id;
	u16 control;
	//    u32 range_limit;
} __ATTRIB_PACK__ nan_service_desc_ext_attr;

typedef struct _nan_device_capa_attr {
	u8 attribute_id;
	u16 len;
	u8 map_id;
	u16 committed_dw_info;
	u8 supported_bands;
	u8 operation_mode;
	u8 num_antenna;
	u16 max_chan_switch_time;
	struct {
		u8 dfs_master_device : 1;
		u8 ext_key_ID : 1;
		u8 simult_ndp_data_reception : 1;
		u8 ndpe_attr_supported : 1;
		u8 reserved : 4;
	} capabilities;
} __ATTRIB_PACK__ nan_device_capa_attr;

struct nan_ranging_frame_attr {
	u8 peer_mac[ETH_ALEN];
	nan_device_capability_attr device_capability_attr;
	nan_element_container_attr element_container_attr;
	nan_ranging_info_attr ranging_info_attr;
	nan_ranging_setup_attr ranging_setup_attr;
	nan_ranging_ftm_report_attr ranging_ftm_report_attr;
	u8 supported_rates[6];
	u8 ext_supported_rates[10];
	u8 htcap[28];
	u8 htinfo[24];
	u8 map_id_rsu;
	u8 time_bitmap_len_rsu;
	time_bitmap_control time_btmp_ctrl_rsu;
	u32 time_bitmap_rsu;
	u8 oui_subtype;
	u8 rejected;
	NAN_FTM_PARAMS nan_ftm_params;
};

struct nan_rx_sd_frame {
	u8 peer_mac[ETH_ALEN];
	u32 rssi;
	u8 service_hash[6];
	u8 instance_id;
	u8 requester_instance_id;
	u8 service_ctrl_btmap;
	u16 binding_btmap;
	u8 matching_filter_len;
	u8 matching_filter[255];
	u8 srf_len;
	struct nan_srf srf;
	u8 service_info_len;
	u8 service_info[255];
	u8 hash_buff[255];
	u8 bloom_filter[255];
	nan_fa_attr fa;
	nan_ranging_attr ranging;
	nan_fa_sd_attr further_nan_sd;
	nan_fa_p2p_attr further_nan_p2p;
	int fa_found;
	nan_device_capability_attr device_capa_attr;
	nan_service_desc_ext_attr desc_ext_attr;
};

enum nan_error
nan_start_unsolicited_publish(struct mwu_iface_info *cur_if,
			      struct nan_publish_service_conf *p_service);

void compute_sha256_service_hash(char *name, unsigned char *service_hash);

enum nan_error nan_tx_sdf(struct mwu_iface_info *cur_if, int type,
			  unsigned char *dest);

enum nan_error nan_tx_ranging_result_frame(struct mwu_iface_info *cur_if,
					   u8 oui_sub_type, u8 reason,
					   u32 range_rslt, unsigned char *dest);

enum nan_error nan_tx_ranging_request_frame(struct mwu_iface_info *cur_if,
					    char peer_mac[ETH_ALEN]);

enum nan_error nan_set_ndl_attr(struct module *mod, u8 *ndl, u8 len);
enum nan_error nan_set_ndc_attr(struct module *mod, u8 *ndc, u8 len);
enum nan_error nan_handle_rx_ranging(struct mwu_iface_info *cur_if,
				     unsigned char *buffer, int size);
int nan_match_filter_ext(char *mf1, char *mf2, int mf1_len, int mf2_len);
int is_mac_in_srf(unsigned char *mac, struct nan_srf *srf);
enum nan_error nan_handle_rx_subscribe(struct mwu_iface_info *cur_if,
				       unsigned char *buffer, int size);
enum nan_error nan_handle_rx_publish(struct mwu_iface_info *cur_if,
				     unsigned char *buffer, int size);
u32 crc32(u32 crc, const void *buff, int size);
void nan_tx_unsolicited_publish_sdf(void *context);
void nan_tx_unsolicited_subscribe_sdf(void *context);
enum nan_error
nan_start_unsolicited_subscribe(struct mwu_iface_info *cur_if,
				struct nan_subscribe_service *s_service);
enum nan_error nan_parse_rx_sdf(struct mwu_iface_info *cur_if, u8 *buffer,
				int size, struct nan_rx_sd_frame *rx_sdf,
				u8 sdf_type);
enum nan_error nan_start_ftm_session(unsigned char *mac);
enum nan_error nan_set_ranging_bitmap(struct mwu_iface_info *cur_if,
				      u32 ranging_bitmap);
#endif
