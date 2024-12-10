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

/* nan_mwu.c: implementation of mwu control interface to nan module */

#define UTIL_LOG_TAG "MWU NAN"
#include "util.h"
#include "nan_mwu.h"
#include "nan.h"
#include "module.h"
#include "mwu_log.h"
//#include "wps_msg.h"  // for a2hex
#include "mwu_if_manager.h"
#include "nan_lib.h"
#include <stdio.h>

#include "discovery_engine.h"
//#include "mlocation.h"
//#include "mlocation_lib.h"

/* We currently only support one module instance at a time.
 * Note that with one module we can still support multiple interfaces. The iface
 * in wifidir_mod means current interface for one command. Each interface will
 * maintain its own data.
 */
static struct module nan_mod;
#if 0
static struct module mlocation_mod;
#endif
#define MAX_CA_VALUES 256

char hexc2bin(char chr);
u32 a2hex(char *s);
int hex2num(s8 c);
/* The max event and command names are actually less than this.  But leave some
 * room to grow
 */
#define NAN_MAX_CMD 36
#define NAN_MAX_EVENT_NAME 24

#define NDP_MAX_SERVICE_INFO_LEN 1024

#define NAN_MOD_NAME "nan"
#define NAN_MOD_KV MWU_KV("module", NAN_MOD_NAME)
#define NAN_MOD_KV_SZ MWU_KV_SZ("module", NAN_MOD_NAME)

#define NAN_CMD_HDR_SCAN                                                       \
	NAN_MOD_KV                                                             \
	MWU_KV_SCAN_STR("iface", IFNAMSIZ)                                     \
	MWU_KV_SCAN_STR("cmd", NAN_MAX_CMD)

#define NAN_CMD_INIT_SCAN                                                      \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_HEX8("a_band")                                              \
	MWU_KV_FMT_DEC32("disc_bcn_period")                                    \
	MWU_KV_FMT_DEC32("scan_time")                                          \
	MWU_KV_FMT_HEX8("master_pref")

#define NAN_CMD_PUBLISH_SCAN                                                   \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_SCAN_STR("service_name", 255)                                   \
	MWU_KV_SCAN_STR("matching_filter_tx", 255)                             \
	MWU_KV_SCAN_STR("matching_filter_rx", 255)                             \
	MWU_KV_SCAN_STR("service_info", 255)                                   \
	MWU_KV_FMT_DEC32("publish_type")                                       \
	MWU_KV_FMT_HEX8("discovery_range")                                     \
	MWU_KV_FMT_DEC32("solicited_tx_type")                                  \
	MWU_KV_FMT_DEC32("announcement_period")                                \
	MWU_KV_FMT_DEC32("time_to_live")                                       \
	MWU_KV_FMT_DEC32("event_cond")                                         \
	MWU_KV_FMT_DEC32("matching_filter_flag")

#define NAN_CMD_FOLLOW_UP_SCAN                                                 \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_DEC32("local_instance_id")                                  \
	MWU_KV_FMT_DEC32("remote_instance_id")                                 \
	MWU_KV_FMT_MAC("mac")

#define NAN_CMD_RANGING_INIT_SCAN                                              \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_MAC("peer_mac")

#define NAN_CMD_SCHED_REQ_SCAN                                                 \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_MAC("responder_mac")

#define NAN_CMD_NDP_REQ_SCAN                                                   \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_DEC32("type")                                               \
	MWU_KV_FMT_DEC32("pub_id")                                             \
	MWU_KV_FMT_MAC("responder_mac")                                        \
	MWU_KV_FMT_DEC32("security") /*Add NAN_SECURITY flag*/                 \
	MWU_KV_FMT_DEC32("confirm_required")                                   \
	MWU_KV_SCAN_STR("service_info", 1024)

#define NAN_CMD_NDP_RESP_SCAN                                                  \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_DEC32("type")                                               \
	MWU_KV_FMT_DEC32("pub_id")                                             \
	MWU_KV_FMT_MAC("initiator_mac")                                        \
	MWU_KV_FMT_DEC32("security") /*Add NAN_SECURITY flag*/                 \
	MWU_KV_FMT_DEC32("confirm_required")                                   \
	MWU_KV_SCAN_STR("service_info", 1024)

#define NAN_CMD_ULW_UPDATE_SCAN                                                \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_DEC32("start_time")                                         \
	MWU_KV_FMT_DEC32("duration")                                           \
	MWU_KV_FMT_DEC32("period")                                             \
	MWU_KV_FMT_DEC32("countdown")                                          \
	MWU_KV_FMT_DEC32("op_class")                                           \
	MWU_KV_FMT_DEC32("channel")                                            \
	MWU_KV_FMT_HEX8("avail_type")

#define NAN_CMD_QOS_UPDATE_SCAN                                                \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_FMT_DEC32("min_slots")                                          \
	MWU_KV_FMT_DEC32("max_latency")

#define NAN_CMD_SET_USER_ATTR                                                  \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_SCAN_STR("data", 255)

#define NAN_CMD_SUBSCRIBE_SCAN                                                 \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_SCAN_STR("service_name", 255)                                   \
	MWU_KV_SCAN_STR("matching_filter_rx", 255)                             \
	MWU_KV_SCAN_STR("matching_filter_tx", 255)                             \
	MWU_KV_SCAN_STR("service_info", 255)                                   \
	MWU_KV_FMT_DEC32("subscribe_type")                                     \
	MWU_KV_FMT_HEX8("discovery_range")                                     \
	MWU_KV_FMT_DEC32("query_period")                                       \
	MWU_KV_FMT_DEC32("time_to_live")                                       \
	MWU_KV_FMT_DEC32("matching_filter_flag")
#define NAN_MAX_EVENT 50

#define NAN_CMD_SET_PMK                                                        \
	NAN_CMD_HDR_SCAN                                                       \
	MWU_KV_SCAN_STR("pmk", PMK_LEN)

#define NAN_EVENT_HDR_FMT                                                      \
	NAN_MOD_KV                                                             \
	MWU_KV_FMT_STR("iface", IFNAMSIZ)                                      \
	MWU_KV_FMT_STR("event", NAN_MAX_EVENT_NAME)

#define NAN_EVENT_HDR_SZ                                                       \
	(NAN_MOD_KV_SZ + MWU_KV_SZ_STR("iface", IFNAMSIZ) +                    \
	 MWU_KV_SZ_STR("event", NAN_MAX_EVENT_NAME))

#define NAN_EVENT_SZ                                                           \
	(NAN_EVENT_HDR_SZ + MWU_KV_SZ_DEC32("status") +                        \
	 MWU_KV_SZ_DEC32("remote_instance_id") +                               \
	 MWU_KV_SZ_DEC32("local_instance_id") + MWU_KV_SZ_MAC("mac"))

#define NAN_EVENT_P2P_FA_SZ                                                    \
	(NAN_EVENT_SZ + MWU_KV_SZ_DEC32("fa_p2p_role") +                       \
	 MWU_KV_SZ_DEC32("fa_p2p_chan") + MWU_KV_SZ_MAC("fa_p2p_mac"))

#define NAN_SDF_EVENT_FMT                                                      \
	NAN_EVENT_HDR_FMT                                                      \
	MWU_KV_FMT_DEC32("status")                                             \
	MWU_KV_FMT_DEC32("remote_instance_id")                                 \
	MWU_KV_FMT_DEC32("local_instance_id")                                  \
	MWU_KV_FMT_MAC("mac")

#define NAN_DATA_IND_SZ(len)                                                   \
	(NAN_EVENT_HDR_SZ + MWU_KV_SZ_HEX8("type") +                           \
	 MWU_KV_SZ_HEX8("publish_id") + MWU_KV_SZ_HEX8("ndp_id") +             \
	 MWU_KV_SZ_MAC("initiator_addr") + MWU_KV_SZ_MAC("responder_addr") +   \
	 MWU_KV_SZ_STR("service_info_data", len))

#define NAN_DATA_IND_FMT(len)                                                  \
	NAN_EVENT_HDR_FMT                                                      \
	MWU_KV_FMT_HEX8("type")                                                \
	MWU_KV_FMT_HEX8("publish_id")                                          \
	MWU_KV_FMT_HEX8("ndp_id")                                              \
	MWU_KV_FMT_MAC("initiator_addr")                                       \
	MWU_KV_FMT_MAC("responder_addr")                                       \
	MWU_KV_FMT_STR("service_info_data", len)

#define NAN_DATA_CONF_SZ(len)                                                  \
	(NAN_EVENT_HDR_SZ + MWU_KV_SZ_HEX8("type") +                           \
	 MWU_KV_SZ_HEX8("status") + MWU_KV_SZ_HEX8("reason") +                 \
	 MWU_KV_SZ_HEX8("publish_id") + MWU_KV_SZ_HEX8("ndp_id") +             \
	 MWU_KV_SZ_DEC32("ndp_chan") + MWU_KV_SZ_DEC32("ndp_chan2") +          \
	 MWU_KV_SZ_MAC("initiator_addr") + MWU_KV_SZ_MAC("responder_addr") +   \
	 MWU_KV_SZ_MAC("peer_ndi") + MWU_KV_SZ_STR("service_info_data", len))

#define NAN_DATA_CONF_FMT(len)                                                 \
	NAN_EVENT_HDR_FMT                                                      \
	MWU_KV_FMT_HEX8("type")                                                \
	MWU_KV_FMT_HEX8("status")                                              \
	MWU_KV_FMT_HEX8("reason")                                              \
	MWU_KV_FMT_HEX8("publish_id")                                          \
	MWU_KV_FMT_HEX8("ndp_id")                                              \
	MWU_KV_FMT_DEC32("ndp_chan")                                           \
	MWU_KV_FMT_DEC32("ndp_chan2")                                          \
	MWU_KV_FMT_MAC("initiator_addr")                                       \
	MWU_KV_FMT_MAC("responder_addr")                                       \
	MWU_KV_FMT_MAC("peer_ndi")                                             \
	MWU_KV_FMT_STR("service_info_data", len)

#define NAN_NDPE_DATA_SZ(len)                                                  \
	NAN_EVENT_HDR_SZ + MWU_KV_SZ_HEX8("type") +                            \
		MWU_KV_SZ_MAC("peer_mac") +                                    \
		MWU_KV_SZ_IFACE_ID("iface_identifier") +                       \
		MWU_KV_SZ_DEC32("transport_port") +                            \
		MWU_KV_SZ_DEC32("transport_protocol")

#define NAN_NDPE_DATA_FMT(len)                                                 \
	NAN_EVENT_HDR_FMT                                                      \
	MWU_KV_FMT_HEX8("type")                                                \
	MWU_KV_FMT_MAC("peer_mac")                                             \
	MWU_KV_FMT_IFACE_ID("iface_identifier")                                \
	MWU_KV_FMT_DEC32("transport_port")                                     \
	MWU_KV_FMT_DEC32("transport_protocol")

#define MLOCATION_MOD_NAME "mlocation"
#define MLOCATION_MOD_KV MWU_KV("module", MLOCATION_MOD_NAME)
#define MLOCATION_MOD_KV_SZ MWU_KV_SZ("module", MLOCATION_MOD_NAME)
#define MLOCATION_MAX_EVENT_NAME 32

#define MLOCATION_EVENT_HDR_SZ                                                 \
	(MLOCATION_MOD_KV_SZ + MWU_KV_SZ_STR("iface", IFNAMSIZ) +              \
	 MWU_KV_SZ_STR("event", MLOCATION_MAX_EVENT_NAME))

#define NAN_EVENT_P2P_FA_FMT                                                   \
	NAN_SDF_EVENT_FMT                                                      \
	MWU_KV_FMT_DEC32("fa_p2p_role")                                        \
	MWU_KV_FMT_DEC32("fa_p2p_chan")                                        \
	MWU_KV_FMT_MAC("fa_p2p_mac")

#define MLOCATION_EVENT_HDR_FMT                                                \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_FMT_STR("iface", IFNAMSIZ)                                      \
	MWU_KV_FMT_STR("event", MLOCATION_MAX_EVENT_NAME)

#define MLOCATION_EVENT_DEV_SZ                                                 \
	(MLOCATION_EVENT_HDR_SZ + MWU_KV_SZ_DEC32("bssNum") +                  \
	 MWU_KV_SZ_DEC32("bssType") + MWU_KV_SZ_MAC("mac_address") +           \
	 MWU_KV_SZ_DEC32("AverageRTT") +                                       \
	 MWU_KV_SZ_DEC32("AverageClockOffset") +                               \
	 MWU_KV_SZ_FLOAT01("distance") +                                       \
	 MWU_KV_SZ_DEC32("next_update_timeout"))

#define MLOCATION_EVENT_DEV_FMT                                                \
	MLOCATION_EVENT_HDR_FMT                                                \
	MWU_KV_FMT_DEC32("bssNum")                                             \
	MWU_KV_FMT_DEC32("bssType")                                            \
	MWU_KV_FMT_MAC("mac_address")                                          \
	MWU_KV_FMT_DEC32("AverageRTT")                                         \
	MWU_KV_FMT_DEC32("AverageClockOffset")                                 \
	MWU_KV_FMT_FLOAT01("distance")                                         \
	MWU_KV_FMT_DEC32("next_update_timeout")

/* we may receive multiple filters in the format
 * filter=aaa:*:bbb:cc:defg from higher layer.
 * This filter needs to be parsed and created in a <l,v>
 * pair form.
 *
 * This function takes the raw filter string as argument and creates
 * the <l,v> filter string */
int parse_filter_str(char *str, char *filter_str)
{
	char *value = NULL;
	char *tmp = filter_str;
	int total_len = 0;
	for (;;) {
		value = strtok_r(NULL, ":", &str);
		if (value == NULL || value[0] == '\0')
			break; /* No more key-value pairs */

		if (strcmp(value, "*") == 0) { /* its a wildcard */
			ERR("Wildcard filter found");
			*tmp = 0;
			tmp++;
			total_len++;
		} else {
			ERR("Filer %s found", value);
			*tmp = strlen(value);
			tmp++;
			total_len++;
			strncpy(tmp, value, strlen(value));
			tmp = tmp + strlen(value);
			total_len = total_len + strlen(value);
		}
		mwu_hexdump(MSG_ERROR, "Complete filter so far is",
			    (u8 *)filter_str, total_len);
	}

	INFO("Parsed filter str is %s", filter_str);
	INFO("Total filter length is %d", (int)strlen(filter_str));

	return total_len;
}

enum mwu_error nan_handle_mwu(struct mwu_msg *msg, struct mwu_msg **resp)
{
	int ret, status;
	char iface[IFNAMSIZ + 1];
	char cmd[NAN_MAX_CMD];

	*resp = NULL;

	INFO("*********************************\n");
	INFO("NAN module receives message:\n");
	INFO("\n%s", msg->data);
	INFO("*********************************\n\n");
	/* parse the message */
	ret = sscanf(msg->data, NAN_CMD_HDR_SCAN, iface, cmd);
	if (ret != 2) {
		ERR("Failed to parse cmd message: ret = %d.\n", ret);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
		return MWU_ERR_SUCCESS;
	}

	/* process command */
	if (strcmp(cmd, "init") == 0) {
		struct nan_cfg cfg;
		ret = sscanf(msg->data, NAN_CMD_INIT_SCAN, iface, cmd,
			     (short unsigned int *)&cfg.a_band,
			     (int *)&cfg.disc_bcn_period, (int *)&cfg.scan_time,
			     (short unsigned int *)&cfg.master_pref);

		if (ret != 6) {
			ERR("init command was missing some key-value pairs. (ret=%d)\n",
			    ret);
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
			return MWU_ERR_SUCCESS;
		}

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		nan_mod.cb = nan_mwu_event_cb;
		nan_mod.cbpriv = &nan_mod;

		status = nan_init(&nan_mod, &cfg);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "deinit") == 0) {
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		nan_deinit(&nan_mod);

	} else if (strcmp(cmd, "nan_start") == 0) {
		INFO("Got nan_start command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_start(&nan_mod);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_stop") == 0) {
		INFO("Got nan_stop command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_stop(&nan_mod);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ndp_terminate") == 0) {
		INFO("Got nan_ndp_terminate command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_ndp_terminate(&nan_mod);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ranging_terminate") == 0) {
		INFO("Got nan_ranging_terminate command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_ranging_terminate(&nan_mod);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ndp_set_immutable_sched") == 0) {
		u32 bitmap = 0;
		char *kv = NULL;
		char *current = msg->data;
		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0') {
				break; /* No more key-value pairs */
			} else if (sscanf(kv, "bitmap=0x%x\n", &bitmap) == 1) {
				INFO("Schedule Bitmap=0x%x", bitmap);
			}
		}
		INFO("Got nan_ndp_set_immutable_sched command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_ndp_set_immutable_sched(&nan_mod, bitmap);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ranging_initiate") == 0) {
		INFO("Got nan_ranging_initiate command");
		char peer_mac[ETH_ALEN];
		ret = sscanf(msg->data, NAN_CMD_RANGING_INIT_SCAN, iface, cmd,
			     UTIL_MAC2SCAN(peer_mac));
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_ranging_initiate(&nan_mod, peer_mac);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ftm_init") == 0) {
		INFO("Got nan_ftm_init command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_ftm_init(&nan_mod);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_set_schedule_update") == 0) {
		struct nan_schedule ndl_sched;
		char *kv = NULL;
		char *current = msg->data;

		memset(&ndl_sched, 0, sizeof(ndl_sched));

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0') {
				break; /* No more key-value pairs */
			} else if (sscanf(kv, "op_class=%d\n",
					  (int *)&ndl_sched.op_class) == 1) {
				INFO("Operating Class=%d", ndl_sched.op_class);
			} else if (sscanf(kv, "op_chan=%d\n",
					  (int *)&ndl_sched.op_chan) == 1) {
				INFO("Operating Channel: 0x%x",
				     ndl_sched.op_chan);
			} else if (sscanf(kv, "bitmap=%x\n",
					  &ndl_sched.availability_map[0]) ==
				   1) {
				INFO("Schedule bitmap: 0x%x",
				     ndl_sched.availability_map[0]);
			}
		}

		INFO("Got nan_set_schedule_update command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_set_schedule_update(&nan_mod, &ndl_sched);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ndp_set_counter_proposal") == 0) {
		struct nan_schedule ndl_sched;
		char *kv = NULL;
		char *current = msg->data;

		memset(&ndl_sched, 0, sizeof(ndl_sched));

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0') {
				break; /* No more key-value pairs */
			} else if (sscanf(kv, "op_class=%d\n",
					  (int *)&ndl_sched.op_class) == 1) {
				INFO("Operating Class=%d", ndl_sched.op_class);
			} else if (sscanf(kv, "op_chan=%d\n",
					  (int *)&ndl_sched.op_chan) == 1) {
				INFO("Operating Channel: 0x%x",
				     ndl_sched.op_chan);
			} else if (sscanf(kv, "bitmap=%x\n",
					  &ndl_sched.availability_map[0]) ==
				   1) {
				INFO("Schedule bitmap: 0x%x",
				     ndl_sched.availability_map[0]);
			}
		}

		INFO("Got nan_ndp_set_counter_proposal command");
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		status = nan_ndp_set_counter_proposal(&nan_mod, &ndl_sched);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "publish") == 0) {
		struct nan_publish_service_conf servc;
		char tx_filter[255] = {0}, rx_filter[255] = {0};
		int total_filter_len = 0;
		INFO("Received publish command");
		memset(&servc, 0, sizeof(struct nan_publish_service_conf));
		ret = sscanf(msg->data, NAN_CMD_PUBLISH_SCAN, iface, cmd,
			     servc.service_name, servc.matching_filter_tx,
			     servc.matching_filter_rx, servc.service_info,
			     &servc.publish_type, (u16 *)&servc.discovery_range,
			     &servc.solicited_tx_type,
			     &servc.announcement_period, &servc.ttl,
			     &servc.event_cond, &servc.matching_filter_flag);

		if (ret != 13) {
			ERR("Publish command was missing some key-value pairs. (ret=%d)\n",
			    ret);
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
			return MWU_ERR_SUCCESS;
		}

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		if (strcasecmp(servc.matching_filter_tx, "null") != 0) {
			total_filter_len = parse_filter_str(
				servc.matching_filter_tx, tx_filter);
			/* copy parsed filter back */
			servc.tx_filter_len = total_filter_len;
			memcpy(servc.matching_filter_tx, tx_filter,
			       total_filter_len);
		}

		if (strcasecmp(servc.matching_filter_rx, "null") != 0) {
			total_filter_len = parse_filter_str(
				servc.matching_filter_rx, rx_filter);
			/* copy parsed filter back */
			servc.rx_filter_len = total_filter_len;
			memcpy(servc.matching_filter_rx, rx_filter,
			       total_filter_len);
		}

		INFO("MF is %s", servc.matching_filter_tx);

		status = nan_publish(&nan_mod, &servc);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "subscribe") == 0) {
		INFO("Received subscribe command");
		struct nan_subscribe_service servc;
		char tx_filter[255] = {0}, rx_filter[255] = {0};
		int total_filter_len = 0;
		ret = sscanf(msg->data, NAN_CMD_SUBSCRIBE_SCAN, iface, cmd,
			     servc.service_name, servc.matching_filter_rx,
			     servc.matching_filter_tx, servc.service_info,
			     &servc.subscribe_type,
			     (u16 *)&servc.discovery_range, &servc.query_period,
			     &servc.ttl, &servc.matching_filter_flag);

		if (ret != 11) {
			ERR("Subscribe command was missing some key-value pairs. (ret=%d)\n",
			    ret);
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
			return MWU_ERR_SUCCESS;
		}

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		/* there can be multiple filters coming from higher layers in
		 * the format filter=aaa:bb:cc:*:d - '*' is wildcard */

		if (strcasecmp(servc.matching_filter_tx, "null") != 0) {
			total_filter_len = parse_filter_str(
				servc.matching_filter_tx, tx_filter);
			/* copy parsed filter back */
			servc.tx_filter_len = total_filter_len;
			memcpy(servc.matching_filter_tx, tx_filter,
			       total_filter_len);
		}

		if (strcasecmp(servc.matching_filter_rx, "null") != 0) {
			total_filter_len = parse_filter_str(
				servc.matching_filter_rx, rx_filter);
			/* copy parsed filter back */
			servc.rx_filter_len = total_filter_len;
			memcpy(servc.matching_filter_rx, rx_filter,
			       total_filter_len);
		}

		status = nan_subscribe(&nan_mod, &servc);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "follow_up") == 0) {
		struct nan_follow_up follow_up;
		memset(&follow_up, 0, sizeof(struct nan_follow_up));

		ERR("before scanning follow up");
		ret = sscanf(msg->data, NAN_CMD_FOLLOW_UP_SCAN, iface, cmd,
			     &follow_up.local_instance_id,
			     &follow_up.remote_instance_id,
			     UTIL_MAC2SCAN(follow_up.mac));

		if (ret != 10) {
			ERR("Follow up command was missing some key-value pairs. (ret=%d)\n",
			    ret);
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
			return MWU_ERR_SUCCESS;
		}

		ERR("Got follow up command");

		status = nan_follow_up(&nan_mod, &follow_up);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_set_cfg") == 0) {
		struct nan_params_cfg nan_cfg;
		char *kv = NULL;
		char *current = msg->data;
		int update = 0;

		memset(&nan_cfg, 0, sizeof(nan_cfg));
		nan_cfg.a_band = -1;
		nan_cfg.disc_bcn_period = -1;
		nan_cfg.scan_time = -1;
		nan_cfg.master_pref = -1;
		nan_cfg.sdf_delay = -1;
		nan_cfg.op_chan_a = -1;
		nan_cfg.hightsf = -1;
		nan_cfg.warm_up_period = -1;
		nan_cfg.include_fa_attr = -1;
		nan_cfg.process_fa_attr = -1;
		nan_cfg.awake_dw_interval = -1;
		nan_cfg.data_path_needed = -1;
		nan_cfg.ranging_required = -1;
		nan_cfg.security = -1;
		nan_cfg.pmf_required = -1;
		nan_cfg.dual_map = -1;
		nan_cfg.qos_enabled = -1;
		nan_cfg.invalid_sched_required = -1;
		nan_cfg.set_apple_dut_test = -1;
		nan_cfg.ndpe_attr_supported = -1;
		nan_cfg.ndpe_attr_protocol = -1; /** 0: none, 1: TCP, 2: UDP */
		nan_cfg.ndpe_attr_iface_identifier = -1;
		nan_cfg.ndpe_attr_trans_port = -1;
		nan_cfg.ndpe_attr_negative = -1;
		nan_cfg.ndp_attr_present = -1;

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0')
				break; /* No more key-value pairs */

			if (sscanf(kv, "sdf_delay=%d\n", &nan_cfg.sdf_delay) ==
			    1) {
				INFO("SDF delay: %d uSec", nan_cfg.sdf_delay);
				update = 1;
			} else if (sscanf(kv, "op_chan_a=%d\n",
					  &nan_cfg.op_chan_a) == 1) {
				INFO("OP chan a: %d", nan_cfg.op_chan_a);
				update = 1;
			} else if (sscanf(kv, "hightsf=%d\n",
					  &nan_cfg.hightsf) == 1) {
#ifdef NAN1_TESTBED
				ERR("TESTBED release: HighTSF");
				INFO("HighTSF: %d", nan_cfg.hightsf);
				update = 1;
#else
				ERR("ALERT: HighTSF configuration is not supported with this release");
				INFO("Ignore HighTSF Flag: %d",
				     nan_cfg.hightsf);
				nan_cfg.hightsf = -1;
				update = 0;
#endif
			} else if (sscanf(kv, "warm_up_period=%d\n",
					  &nan_cfg.warm_up_period) == 1) {
				INFO("WarmUp Period: %ds",
				     nan_cfg.warm_up_period);
				update = 1;
			} else if (sscanf(kv, "include_fa_attr=%d\n",
					  &nan_cfg.include_fa_attr) == 1) {
				INFO("Include Further Availability Attribute: %d",
				     nan_cfg.include_fa_attr);
				update = 1;
			} else if (sscanf(kv, "process_fa_attr=%d\n",
					  &nan_cfg.process_fa_attr) == 1) {
				INFO("process Further Availability Attribute: %d",
				     nan_cfg.process_fa_attr);
				update = 1;
			} else if (sscanf(kv, "awake_dw_interval=%d\n",
					  &nan_cfg.awake_dw_interval) == 1) {
				INFO("Awake DW interval: %d",
				     nan_cfg.awake_dw_interval);
				update = 1;
			} else if (sscanf(kv, "pmf_required=%d\n",
					  &nan_cfg.pmf_required) == 1) {
				INFO("PMF required: %d", nan_cfg.pmf_required);
				update = 1;
			} else if (sscanf(kv, "data_path_needed=%d\n",
					  &nan_cfg.data_path_needed) == 1) {
				INFO("Data patch needed= %d",
				     nan_cfg.data_path_needed);
				update = 1;
			} else if (sscanf(kv, "ranging_required=%d\n",
					  &nan_cfg.ranging_required) == 1) {
				INFO("ranging_required= %d",
				     nan_cfg.ranging_required);
				update = 1;
			} else if (sscanf(kv, "security=%d\n",
					  &nan_cfg.security) == 1) {
				INFO("security= %d", nan_cfg.security);
				update = 1;
			} else if (sscanf(kv, "set_apple_dut_test=%d\n",
					  &nan_cfg.set_apple_dut_test) == 1) {
				INFO("set_apple_dut_test= %d",
				     nan_cfg.set_apple_dut_test);
				update = 1;
			} else if (sscanf(kv, "dual_map=%d\n",
					  &nan_cfg.dual_map) == 1) {
				INFO("dual_map= %d", nan_cfg.dual_map);
				update = 1;
			} else if (sscanf(kv, "qos_enabled=%d\n",
					  &nan_cfg.qos_enabled) == 1) {
				INFO("qos_enabled= %d", nan_cfg.qos_enabled);
				update = 1;
			} else if (sscanf(kv, "invalid_sched_required=%d\n",
					  &nan_cfg.invalid_sched_required) ==
				   1) {
				INFO("invalid_sched_required= %d",
				     nan_cfg.invalid_sched_required);
				update = 1;
			} else if (sscanf(kv, "ndpe_supported=%d\n",
					  &nan_cfg.ndpe_attr_supported) == 1) {
				INFO("ndpe_supported= %d",
				     nan_cfg.ndpe_attr_supported);
				update = 1;
			} else if (sscanf(kv, "ndpe_protocol=%d\n",
					  &nan_cfg.ndpe_attr_protocol) == 1) {
				INFO("ndpe_protocol= %d",
				     nan_cfg.ndpe_attr_protocol);
				update = 1;
			} else if (sscanf(kv, "ndpe_iface_identifier=%d\n",
					  &nan_cfg.ndpe_attr_iface_identifier) ==
				   1) {
				INFO("ndpe_iface_identifier= %d",
				     nan_cfg.ndpe_attr_iface_identifier);
				update = 1;
			} else if (sscanf(kv, "ndpe_trans_port=%d\n",
					  &nan_cfg.ndpe_attr_trans_port) == 1) {
				INFO("ndpe_trans_port= %d",
				     nan_cfg.ndpe_attr_trans_port);
				update = 1;
			} else if (sscanf(kv, "ndpe_negative=%d\n",
					  &nan_cfg.ndpe_attr_negative) == 1) {
				INFO("ndpe_negative= %d",
				     nan_cfg.ndpe_attr_negative);
				update = 1;
			} else if (sscanf(kv, "ndp_present=%d\n",
					  &nan_cfg.ndp_attr_present) == 1) {
				INFO("ndp_present= %d",
				     nan_cfg.ndp_attr_present);
				update = 1;
			} else {
				INFO("Ignoring key-value: %s", kv);
			}
		}
		if (!update) {
			/* Configuration is unchanged */
			WARN("NAN Config is not changed");
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_SUCCESS);
			return NAN_ERR_SUCCESS;
		} else {
			strncpy(nan_mod.iface, iface, IFNAMSIZ);
			status = nan_set_config(&nan_mod, &nan_cfg);
			ALLOC_STATUS_MSG_OR_FAIL(*resp, status);
		}
	} else if (strcmp(cmd, "nan_get_cfg") == 0) {
#if 0
        struct nan_params_cfg nan_cfg;
        char resp_data[100] = {0};

        strncpy(nan_mod.iface, iface, IFNAMSIZ);
        memset(&nan_cfg, 0 , sizeof(nan_cfg));

        status = nan_get_config(&nan_mod, &nan_cfg);

        sprintf(resp_data,"group_cap=%x\ndevice_cap=%x\n",
                p2p_cfg.group_capability, p2p_cfg.dev_capability);

        /* Prepare the response buffer */
        *resp = malloc(sizeof(struct mwu_msg) + strlen(resp_data) + 1);
        if (*resp == NULL) {
            ERR("Failed to allocate p2p_get_cfg response message");
            ret = MWU_ERR_NOMEM;
            goto fail;
        }
        strcpy((*resp)->data, resp_data);
        (*resp)->len = strlen(resp_data) + 1;
#endif
		/* not supported at the moment */
		ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_UNSUPPORTED);
	} else if (strcmp(cmd, "get_nan_state_info") == 0) {
		struct nan_state_info nan_state;
		char resp_data[500] = {0};
		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		memset(&nan_state, 0, sizeof(nan_state_info));

		status = nan_get_state_info(&nan_mod, &nan_state);

		sprintf(resp_data,
			"hold_role_flag=%x\ncur_role=%x\n"
			"hold_master_pref_flag=%x\ncur_master_pref=%x\n"
			"hold_rfactor_flag=%x\ncur_rfactor=%x\n"
			"hold_hop_cnt_flag=%x\ncur_hop_cnt=%x\n"
			"disable_2g=%x\n",
			nan_state.hold_role_flag, nan_state.cur_role,
			nan_state.hold_master_pref_flag,
			nan_state.cur_master_pref, nan_state.hold_rfactor_flag,
			nan_state.cur_rfactor, nan_state.hold_hop_cnt_flag,
			nan_state.cur_hop_cnt, nan_state.disable_2g);

		/* Prepare the response buffer */
		*resp = malloc(sizeof(struct mwu_msg) + strlen(resp_data) + 1);
		if (*resp == NULL) {
			ERR("Failed to allocate p2p_get_cfg response message");
			ret = MWU_ERR_NOMEM;
			goto fail;
		}
		strcpy((*resp)->data, resp_data);
		(*resp)->len = strlen(resp_data) + 1;
	} else if (strcmp(cmd, "set_nan_state_info") == 0) {
		struct nan_state_info nan_state;
		char *kv = NULL;
		char *current = msg->data;
		int update = 0;

		memset(&nan_state, 0, sizeof(nan_state));

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0')
				break; /* No more key-value pairs */
			if (sscanf(kv, "cur_role=%x\n", &nan_state.cur_role) ==
			    1) {
				INFO("Cur Role: 0x%x", nan_state.cur_role);
				nan_state.hold_role_flag = 1;
				update = 1;
			} else if (sscanf(kv, "cur_master_pref=%x\n",
					  &nan_state.cur_master_pref) == 1) {
				INFO("Current master pref: 0x%x",
				     nan_state.cur_master_pref);
				nan_state.hold_master_pref_flag = 1;
				update = 1;
			} else if (sscanf(kv, "cur_rfactor=%x\n",
					  &nan_state.cur_rfactor) == 1) {
				INFO("Current Random Factror: %x",
				     nan_state.cur_rfactor);
				nan_state.hold_rfactor_flag = 1;
				update = 1;
			} else if (sscanf(kv, "cur_hop_cnt=%x\n",
					  &nan_state.cur_hop_cnt) == 1) {
				INFO("Current Hop count: %x",
				     nan_state.cur_hop_cnt);
				nan_state.hold_hop_cnt_flag = 1;
				update = 1;
			} else if (sscanf(kv, "disable_2g=%x\n",
					  &nan_state.disable_2g) == 1) {
				INFO("Disable_2g: %x", nan_state.disable_2g);
				nan_state.disable_2g_flag = 1;
				update = 1;

			} else {
				INFO("Ignoring key-value: %s", kv);
			}
		}
		if (!update) {
			/* Configuration is unchanged */
			WARN("NAN state info is not changed");
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_SUCCESS);
			return NAN_ERR_SUCCESS;
		} else {
			strncpy(nan_mod.iface, iface, IFNAMSIZ);
			status = nan_set_state_info(&nan_mod, &nan_state);
			ALLOC_STATUS_MSG_OR_FAIL(*resp, status);
		}
	} else if (strcmp(cmd, "srf") == 0) {
		struct nan_srf srf;
		char *kv = NULL;
		char *current = msg->data;
		int index = 0;
		char *value = NULL;
		u32 ctrl = 0;

		memset(&srf, 0, sizeof(struct nan_srf));

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0')
				break; /* No more key-value pairs */
			if (sscanf(kv, "srf_ctrl=%x\n", &ctrl) == 1) {
				INFO("SRF Ctrl: 0x%x", ctrl);
				srf.srf_ctrl = (u8)ctrl;
			}

			else if (strncmp(kv, "mac_list", strlen("mac_list")) ==
				 0) {
				strtok_r(NULL, "=", &kv);

				/* Parse mac list */
				for (;;) {
					value = strtok_r(NULL, ",\n", &kv);
					if (value == NULL || value[0] == '\0')
						break; /* No more key-value
							  pairs */

					sscanf(value, UTIL_MACSTR,
					       UTIL_MAC2SCAN(
						       srf.mac_list[index]));

					INFO("MAC LIST: " UTIL_MACSTR,
					     UTIL_MAC2STR(srf.mac_list[index]));
					index++;

					if (index == 10)
						break;
				}
			} else {
				INFO("Ignoring key-value: %s", kv);
			}
		}

		srf.num_mac = (u8)index;

		strncpy(nan_mod.iface, iface, IFNAMSIZ);
		mwu_hexdump(MSG_INFO, "srf", (u8 *)&srf, sizeof(srf));
		status = configure_nan_srf(&nan_mod, &srf);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_set_pmk") == 0) {
		struct nan_set_pmk_attr nan_pmk_set;

		memset(&nan_pmk_set, 0, sizeof(nan_pmk_set));

		INFO("Rcvd nan_set_pmk");
		ret = sscanf(msg->data, NAN_CMD_SET_PMK, iface, cmd,
			     nan_pmk_set.pmk);

		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		if (ret != 3) {
			ERR("Insufficient parameters , nan_set_pmk command is missing PMK");
			ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
			return MWU_ERR_INVAL;
		}

		status = nan_set_user_pmk(&nan_mod, &nan_pmk_set);

		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							  * API which returns
							  * publish_id on
							  */
	} else if (strcmp(cmd, "ndp_sec_test") == 0) {
		char *kv = NULL;
		char *current = msg->data;
		u8 wrong_mic = 0, m4_reject = 0;

		INFO("Rcvd ndp_sec_test");
		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0') {
				break; /* No more key-value pairs */
			} else if (sscanf(kv, "m4_wrong_mic=%d\n",
					  (int *)&wrong_mic) == 1) {
				INFO("m4_wrong_mic=%d", wrong_mic);
			} else if (sscanf(kv, "m4_reject=%d\n",
					  (int *)&m4_reject) == 1) {
				INFO("m4_reject=%d", m4_reject);
			}
		}
		status = nan_ndp_sec_test(&nan_mod, wrong_mic, m4_reject);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);
	} else if (strcmp(cmd, "nan_set_ndl_schedule") == 0) {
		peer_availability_info avail_info;
		char *kv = NULL;
		char *current = msg->data;
		u8 clean = 0;

		memset(&avail_info, 0, sizeof(peer_availability_info));

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0') {
				break; /* No more key-value pairs */
			} else if (sscanf(kv, "clean=%d\n", (int *)&clean) ==
				   1) {
				INFO("Cleanup availability=%d", clean);
			} else if (sscanf(kv, "op_class=%d\n",
					  (int *)&avail_info
						  .entry_conditional[0]
						  .op_class) == 1) {
				INFO("Operating Class=%d",
				     avail_info.entry_conditional[0].op_class);
			} else if (sscanf(kv, "op_chan=%d\n",
					  (int *)&avail_info
						  .entry_conditional[0]
						  .channels[0]) == 1) {
				INFO("Operating Channel: 0x%x",
				     avail_info.entry_conditional[0]
					     .channels[0]);
			} else if (sscanf(kv, "bitmap=%x\n",
					  &avail_info.entry_conditional[0]
						   .time_bitmap[0]) == 1) {
				INFO("availability bitmap: 0x%x",
				     avail_info.entry_conditional[0]
					     .time_bitmap[0]);
			}
		}
		status = nan_set_ndl_schedule(&nan_mod, &avail_info, clean);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);
	} else if (strcmp(cmd, "nan_set_availability") == 0) {
		peer_availability_info avail_info;
		char *kv = NULL;
		char *current = msg->data;

		memset(&avail_info, 0, sizeof(peer_availability_info));

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0') {
				break; /* No more key-value pairs */
			} else if (sscanf(kv, "op_class=%d\n",
					  (int *)&avail_info.entry_potential[0]
						  .op_class) == 1) {
				INFO("Operating Class=%d",
				     avail_info.entry_potential[0].op_class);
			} else if (sscanf(kv, "op_chan=%d\n",
					  (int *)&avail_info.entry_potential[0]
						  .channels[0]) == 1) {
				INFO("Operating Channel: 0x%x",
				     avail_info.entry_potential[0].channels[0]);
			} else if (sscanf(kv, "bitmap=%x\n",
					  (int *)&avail_info.entry_potential[0]
						  .time_bitmap[0]) == 1) {
				INFO("availability bitmap: 0x%x",
				     avail_info.entry_potential[0]
					     .time_bitmap[0]);
			}
		}
		status = nan_set_availability(&nan_mod, &avail_info);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);
	} else if (strcmp(cmd, "nan_set_fa_p2p") == 0) {
		struct nan_params_p2p_fa p2p_fa;
		char *kv = NULL;
		char *current = msg->data;

		memset(&p2p_fa, 0, sizeof(p2p_fa));

		p2p_fa.map_id = 0;
		p2p_fa.ctrl = 0;
		p2p_fa.availability_map = 0x00fcff00;

		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0')
				break; /* No more key-value pairs */
			if (sscanf(kv, "dev_role=%x\n",
				   (u32 *)&p2p_fa.dev_role) == 1) {
				INFO("Device Role: 0x%x", p2p_fa.dev_role);
			} else if (sscanf(kv, "op_class=%x\n",
					  (u32 *)&p2p_fa.op_class) == 1) {
				INFO("Operating Class: 0x%x", p2p_fa.op_class);
			} else if (sscanf(kv, "op_chan=%x\n",
					  (u32 *)&p2p_fa.op_chan) == 1) {
				INFO("Operating Channel: 0x%x", p2p_fa.op_chan);
			} else if (sscanf(kv, "bitmap=%x\n",
					  &p2p_fa.availability_map) == 1) {
				INFO("FAV bitmap: 0x%x",
				     p2p_fa.availability_map);
			} else if (strncmp(kv, "mac", strlen("mac")) == 0) {
				strtok_r(NULL, "=", &kv);
				INFO("MAC: %s", kv);
				sscanf(kv, UTIL_MACSTR,
				       UTIL_MAC2SCAN(p2p_fa.mac));
				INFO("MAC: " UTIL_MACSTR,
				     UTIL_MAC2STR(p2p_fa.mac));
			}
		}

		status = nan_set_p2p_fa(&nan_mod, &p2p_fa);

#if 0
        struct fa_attr fa;
        ret = sscanf(msg->data, NAN_CMD_FA_ATTR_SCAN, iface, cmd,
                fa.hex_data)

        if (ret != 3) {
            ERR("fa_attr_hex_str command was missing some key-value pairs. (ret=%d)\n", ret);
            ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_INVAL);
            return MWU_ERR_SUCCESS;
        }

        /* prepare the module struct */
        strncpy(nan_mod.iface, iface, IFNAMSIZ);
        nan_mod.cb = nan_mwu_event_cb;
        nan_mod.cbpriv = &nan_mod;

        status = nan_set_further_availability(&nan_mod, &fa);
        ALLOC_STATUS_MSG_OR_FAIL(*resp, status);
#endif
		/* not supported at the moment - uses hardcoded Further
		 * availability map*/
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_sched_req") == 0) {
		INFO("Received Schedule request command");
		char responder_mac[ETH_ALEN];
		ret = sscanf(msg->data, NAN_CMD_SCHED_REQ_SCAN, iface, cmd,
			     UTIL_MAC2SCAN(responder_mac));

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_sched_request(&nan_mod, responder_mac);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_sched_resp") == 0) {
		INFO("Received Schedule response command");
		char responder_mac[ETH_ALEN];
		ret = sscanf(msg->data, NAN_CMD_SCHED_REQ_SCAN, iface, cmd,
			     UTIL_MAC2SCAN(responder_mac));

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_sched_response(&nan_mod, responder_mac);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_send_sched_update") == 0) {
		INFO("Received send schedule_update command");
		char responder_mac[ETH_ALEN];
		ret = sscanf(msg->data, NAN_CMD_SCHED_REQ_SCAN, iface, cmd,
			     UTIL_MAC2SCAN(responder_mac));

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_send_sched_update(&nan_mod, responder_mac);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "nan_ndp_req") == 0) {
		INFO("Received NDP request command");
		nan_ndp_req ndp;
		ret = sscanf(msg->data, NAN_CMD_NDP_REQ_SCAN, iface, cmd,
			     &ndp.type, &ndp.pub_id,
			     UTIL_MAC2SCAN(ndp.responder_mac), &ndp.security,
			     &ndp.confirm_required, ndp.service_info);

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_ndp_request(&nan_mod, &ndp);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "nan_ndp_resp") == 0) {
		INFO("Received NDP response command");
		nan_ndp_resp ndp;
		ret = sscanf(msg->data, NAN_CMD_NDP_RESP_SCAN, iface, cmd,
			     &ndp.type, &ndp.pub_id,
			     UTIL_MAC2SCAN(ndp.initiator_mac), &ndp.security,
			     &ndp.confirm_required, ndp.service_info);

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_ndp_response(&nan_mod, &ndp);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "nan_ulw_update") == 0) {
		nan_ulw_param ulw;
		INFO("Received nan ulw update command");
		memset(&ulw, 0, sizeof(nan_ulw_param));
		ret = sscanf(msg->data, NAN_CMD_ULW_UPDATE_SCAN, iface, cmd,
			     &ulw.start_time, &ulw.duration, &ulw.period,
			     (int *)&ulw.count_down, (int *)&ulw.op_class,
			     (int *)&ulw.channel, (u16 *)&ulw.avail_type);

		/* prepare the module struct */
		strncpy(nan_mod.iface, "nan0", IFNAMSIZ);

		status = nan_ulw_update(&nan_mod, &ulw);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "nan_qos_update") == 0) {
		ndl_qos_t qos;
		INFO("Received nan qos update command");
		memset(&qos, 0, sizeof(ndl_qos_t));
		ret = sscanf(msg->data, NAN_CMD_QOS_UPDATE_SCAN, iface, cmd,
			     (int *)&qos.min_slots, (int *)&qos.max_latency);

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_qos_update(&nan_mod, &qos);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "nan_set_avail_attr") == 0) {
		INFO("Received set_nan_avial_attr command");
		u8 avail[256];
		ret = sscanf(msg->data, NAN_CMD_SET_USER_ATTR, iface, cmd,
			     avail);

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_set_avail_attr(&nan_mod, avail,
					    strlen((char *)avail));
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "nan_set_ndc_attr") == 0) {
		INFO("Received set_nan_ndc_attr command");
		u8 ndc[256];
		ret = sscanf(msg->data, NAN_CMD_SET_USER_ATTR, iface, cmd, ndc);

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_set_ndc_attr(&nan_mod, ndc, strlen((char *)ndc));
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else if (strcmp(cmd, "nan_set_ndl_attr") == 0) {
		INFO("Received set_nan_ndl_attr command");
		u8 ndl[256];
		ret = sscanf(msg->data, NAN_CMD_SET_USER_ATTR, iface, cmd, ndl);

		/* prepare the module struct */
		strncpy(nan_mod.iface, iface, IFNAMSIZ);

		status = nan_set_ndl_attr(&nan_mod, ndl, strlen((char *)ndl));
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */
	} else {
		ERR("Got unknown command: %s\n", cmd);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, NAN_ERR_UNSUPPORTED);
	}
	return MWU_ERR_SUCCESS;

fail:
	FREE(*resp);
	return ret;
}

static struct mwu_module nan_mwu_mod = {
	.name = NAN_MOD_NAME,
	.msg_cb = nan_handle_mwu,
	.msg_free = NULL,
};

enum mwu_error nan_mwu_launch(void)
{
	if (nan_mod.iface[0] != 0)
		return MWU_ERR_BUSY;

	return mwu_internal_register_module(&nan_mwu_mod);
}

void nan_mwu_halt(void)
{
	mwu_internal_unregister_module(&nan_mwu_mod);
	nan_mod.iface[0] = 0;
}

void nan_mwu_event_cb(struct event *event, void *priv)
{
	int type, ret = 0;
	struct mwu_msg *msg_event = NULL;
	struct discover_result *r;
	struct ndp_data_indication *di;
	struct ndp_data_confirm *dc;
	ndp_ndpe_data *ndpe = NULL;

	ENTER();
	if (!event) {
		ERR("event pointer is NULL\n");
		return;
	}

	type = event->type;
	switch (type) {
	case NAN_EVENT_DISCOVERY_RESULT:
		r = (struct discover_result *)event->val;

		if (r->is_fa_p2p) {
			ALLOC_MSG_OR_FAIL(msg_event, NAN_EVENT_P2P_FA_SZ + 1,
					  NAN_EVENT_P2P_FA_FMT, event->iface,
					  "DiscoveryResult", event->status,
					  r->remote_instance_id,
					  r->local_instance_id,
					  UTIL_MAC2STR(r->mac), r->fa_dev_role,
					  r->fa_chan, UTIL_MAC2STR(r->fa_mac));
		} else {
			ALLOC_MSG_OR_FAIL(msg_event, NAN_EVENT_SZ + 1,
					  NAN_SDF_EVENT_FMT, event->iface,
					  "DiscoveryResult", event->status,
					  r->remote_instance_id,
					  r->local_instance_id,
					  UTIL_MAC2STR(r->mac));
		}
		ERR("==> event: \n %s", msg_event->data);
		break;

	case NAN_EVENT_REPLIED:
		r = (struct discover_result *)event->val;

		if (r->is_fa_p2p) {
			ALLOC_MSG_OR_FAIL(msg_event, NAN_EVENT_P2P_FA_SZ + 1,
					  NAN_EVENT_P2P_FA_FMT, event->iface,
					  "Replied", event->status,
					  r->remote_instance_id,
					  r->local_instance_id,
					  UTIL_MAC2STR(r->mac), r->fa_dev_role,
					  r->fa_chan, UTIL_MAC2STR(r->fa_mac));
		} else {
			ALLOC_MSG_OR_FAIL(msg_event, NAN_EVENT_SZ + 1,
					  NAN_SDF_EVENT_FMT, event->iface,
					  "Replied", event->status,
					  r->remote_instance_id,
					  r->local_instance_id,
					  UTIL_MAC2STR(r->mac));
		}
		ERR("==> event: \n %s", msg_event->data);
		break;

	case NAN_EVENT_FOLLOW_UP_RECVD:
		r = (struct discover_result *)event->val;
		ALLOC_MSG_OR_FAIL(msg_event, NAN_EVENT_SZ + 1,
				  NAN_SDF_EVENT_FMT, event->iface,
				  "FollowUpReceive", event->status,
				  r->remote_instance_id, r->local_instance_id,
				  UTIL_MAC2STR(r->mac));
		ERR("==> event: \n %s", msg_event->data);
		break;

	case NAN_EVENT_DATA_INDICATION:
		di = (struct ndp_data_indication *)(event->val);
		ALLOC_MSG_OR_FAIL(msg_event,
				  (NAN_DATA_IND_SZ(di->service_info.len) + 1),
				  NAN_DATA_IND_FMT(di->service_info.len),
				  event->iface, "DataIndication", di->type,
				  di->publish_id, di->ndp_id,
				  UTIL_MAC2STR(di->initiator_addr),
				  UTIL_MAC2STR(di->responder_addr),
				  di->service_info.data[0]);
		ERR("==> event: \n %s", msg_event->data);
		break;

	case NAN_EVENT_DATA_CONFIRM:
		dc = (struct ndp_data_confirm *)(event->val);
		ALLOC_MSG_OR_FAIL(
			msg_event, (NAN_DATA_CONF_SZ(dc->service_info.len) + 1),
			NAN_DATA_CONF_FMT(dc->service_info.len), event->iface,
			"DataConfirm", dc->type, dc->status_reason.status,
			dc->status_reason.reason, dc->publish_id, dc->ndp_id,
			dc->ndp_chan, dc->ndp_chan2,
			UTIL_MAC2STR(dc->initiator_addr),
			UTIL_MAC2STR(dc->responder_addr),
			UTIL_MAC2STR(dc->peer_ndi), dc->service_info.data[0]);
		ERR("==> event: \n %s", msg_event->data);
		break;
	case NAN_EVENT_NDPE_DATA:
		ndpe = (ndp_ndpe_data *)(event->val);
		ALLOC_MSG_OR_FAIL(msg_event,
				  (NAN_NDPE_DATA_SZ(sizeof(ndp_ndpe_data)) + 1),
				  NAN_NDPE_DATA_FMT(sizeof(ndp_ndpe_data)),
				  event->iface, "NDPEData", ndpe->type,
				  UTIL_MAC2STR(ndpe->peer_mac),
				  UTIL_IFACEID2STR(ndpe->iface_identifier),
				  ndpe->transport_port,
				  ndpe->transport_protocol);
		ERR("==> event: \n %s", msg_event->data);
		break;
	}

	if (msg_event) {
		mwu_internal_send(msg_event);
		FREE(msg_event);
	}
	LEAVE();
	ret = ret;
fail:
	return;
}

void nan_mwu_event_ftm_cb(struct event *event, void *priv)
{
	int type;
	mlocation_event *dev;
	float distance;

	ENTER();
	if (!event) {
		ERR("event pointer is NULL\n");
		return;
	}

	type = event->type;
	switch (type) {
	case MLOCATION_EVENT_SESSION_COMPLETE:
		dev = (mlocation_event *)event->val;
		distance = (dev->AverageClockOffset / 2) * 0.3 / 1000;
		ERR("==> Got MLOCATION_EVENT_SESSION_COMPLETE, distane = %f\n",
		    distance);
		nan_send_ftm_report(&nan_mod, distance,
				    (char *)dev->mac_address);
		nan_send_ftm_complete_event(dev, event->iface, distance);

		break;
	}

	LEAVE();
	return;
}
