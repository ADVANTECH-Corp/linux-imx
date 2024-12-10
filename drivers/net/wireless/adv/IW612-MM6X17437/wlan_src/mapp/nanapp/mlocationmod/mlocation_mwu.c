/*
 *  Copyright 2012-2022 NXP
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
/* mlocation_mwu: mlocation module for mwu control interface */
#include <stdlib.h>
#include "mwu.h"

#ifndef ANDROID
#include <stdio.h>
#include <string.h>
#endif

#include "util.h"
#include "mwu_internal.h"

#include "mwu_if_manager.h"

#include "mwu_log.h"
#include "mwu_defs.h"
#include "module.h"
#include "mlocation_lib.h"
#include "mlocation_mwu.h"
#include "mlocation_api.h"
#include "range_kalman.h" //kalman filter for distance

static struct module mlocation_mod;
range_kalman_state range_state;
double range_measurement_var = 1.0f;
double range_rate_init = 0.2f;
double range_drive_var = 0.0001f;
static int enable_kalman_filter = 0;
u8 fqdn_support;

enum mwu_error mlocation_handle_mwu(struct mwu_msg *msg, struct mwu_msg **resp);

static struct mwu_module mlocation_mwu_mod = {
	.name = "mlocation",
	.msg_cb = mlocation_handle_mwu,
	.msg_free = NULL,
};

#ifdef UTIL_LOG_TAG
#undef UTIL_LOG_TAG
#endif

#define UTIL_LOG_TAG "MLOCATION"
#define MLOCATION_MOD_NAME "mlocation"
#define MLOCATION_MOD_KV MWU_KV("module", MLOCATION_MOD_NAME)
#define MLOCATION_MAX_CMD 50
#define MLOCATION_MAX_EVENT_NAME 32

extern void *os_zalloc(size_t size);

#define MLOCATION_CMD_HDR_SCAN                                                 \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_SCAN_STR("iface", IFNAMSIZ)                                     \
	MWU_KV_SCAN_STR("cmd", MLOCATION_MAX_CMD)

#define MLOCATION_SESSION_CMD                                                  \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_SCAN_STR("iface", IFNAMSIZ)                                     \
	MWU_KV_SCAN_STR("cmd", MLOCATION_MAX_CMD)                              \
	MWU_KV_FMT_DEC32("action")                                             \
	MWU_KV_FMT_MAC("mac")                                                  \
	MWU_KV_FMT_DEC32("channel")                                            \
	MWU_KV_FMT_DEC32("civic_req")                                          \
	MWU_KV_FMT_DEC32("lci_req")

#define MLOCATION_SEND_RADIO_REQUEST                                           \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_SCAN_STR("iface", IFNAMSIZ)                                     \
	MWU_KV_SCAN_STR("cmd", MLOCATION_MAX_CMD)                              \
	MWU_KV_FMT_MAC("mac")

#define MLOCATION_SEND_NEIGHBOR_REQUEST                                        \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_SCAN_STR("iface", IFNAMSIZ)                                     \
	MWU_KV_SCAN_STR("cmd", MLOCATION_MAX_CMD)                              \
	MWU_KV_FMT_DEC32("civic_req")                                          \
	MWU_KV_FMT_DEC32("lci_req")

#define MLOCATION_SEND_ANQP_REQUEST                                            \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_SCAN_STR("iface", IFNAMSIZ)                                     \
	MWU_KV_SCAN_STR("cmd", MLOCATION_MAX_CMD)                              \
	MWU_KV_FMT_MAC("mac")                                                  \
	MWU_KV_FMT_DEC32("civic_req")                                          \
	MWU_KV_FMT_DEC32("lci_req")

#define MLOCATION_MOD_KV_SZ MWU_KV_SZ("module", MLOCATION_MOD_NAME)
#define MLOCATION_EVENT_HDR_SZ                                                 \
	(MLOCATION_MOD_KV_SZ + MWU_KV_SZ_STR("iface", IFNAMSIZ) +              \
	 MWU_KV_SZ_STR("event", MLOCATION_MAX_EVENT_NAME))

#define MLOCATION_EVENT_DEV_SZ                                                 \
	(MLOCATION_EVENT_HDR_SZ + MWU_KV_SZ_DEC32("bssNum") +                  \
	 MWU_KV_SZ_DEC32("bssType") + MWU_KV_SZ_MAC("mac_address") +           \
	 MWU_KV_SZ_DEC32("AverageRTT") +                                       \
	 MWU_KV_SZ_DEC32("AverageClockOffset") +                               \
	 MWU_KV_SZ_FLOAT01("distance") +                                       \
	 MWU_KV_SZ_DEC32("next_update_timeout"))

#define MLOCATION_EVENT_RADIO_REPORT_RECVD_SZ                                  \
	(MLOCATION_EVENT_HDR_SZ + MWU_KV_SZ_DEC32("rpt_distance") +            \
	 MWU_KV_SZ_MAC("rpt_mac"))

#define MLOCATION_EVENT_HDR_FMT                                                \
	MLOCATION_MOD_KV                                                       \
	MWU_KV_FMT_STR("iface", IFNAMSIZ)                                      \
	MWU_KV_FMT_STR("event", MLOCATION_MAX_EVENT_NAME)

#define MLOCATION_EVENT_DEV_FMT                                                \
	MLOCATION_EVENT_HDR_FMT                                                \
	MWU_KV_FMT_DEC32("bssNum")                                             \
	MWU_KV_FMT_DEC32("bssType")                                            \
	MWU_KV_FMT_MAC("mac_address")                                          \
	MWU_KV_FMT_DEC32("AverageRTT")                                         \
	MWU_KV_FMT_DEC32("AverageClockOffset")                                 \
	MWU_KV_FMT_FLOAT01("distance")                                         \
	MWU_KV_FMT_DEC32("next_update_timeout")

#define MLOCATION_EVENT_DEV_RADIO_REPORT_FMT                                   \
	MLOCATION_EVENT_HDR_FMT                                                \
	MWU_KV_FMT_DEC32("rpt_distance")                                       \
	MWU_KV_FMT_MAC("rpt_mac")

/* status=4294967295\n\0 is our largest cmd response. */
#define MLOCATION_MAX_RESP 20

/* module=mlocation\nevent=pong\n\0 is our largest event at this time of
 * writing. But leave some room to grow.
 */
#define MLOCATION_MAX_EVENT 64

float ftm_distance;
int ftm_required = 0;
mlocation_radio_receive_event radio_frame_recv;

void *os_zalloc(size_t size)
{
	void *buf = malloc((size_t)size);
	if (buf)
		memset(buf, 0, size);
	return buf;
}

void mlocation_mwu_event_cb(struct event *event, void *priv)
{
	int type;
	struct mwu_msg *msg_event = NULL;
	int ret = MWU_ERR_SUCCESS;
	/* Increase update_timeout to a really large value so that
	 * GUI does not report zero distance if MWU does not send
	 * any event within timeout. This may happen when FTM fails
	 * and MWU inhibits sending the failed distance to GUI/App.
	 * Why FTM fails frequently needs to be debugged and fixed
	 * before reverting timeout to 15*/
	int update_timeout = 3600;

	ENTER();
	if (!event) {
		ERR("event pointer is NULL\n");
		return;
	}

	type = event->type;
	switch (type) {
	case MLOCATION_EVENT_SESSION_COMPLETE: {
		mlocation_event *dev;
		float distance;

		dev = (mlocation_event *)event->val;

		distance = (dev->AverageClockOffset / 2) * 0.3 /
			   1000; /*clock offset is in pico seconds, dist shoudl
				    be in m */
#if 0
                if(ftm_required) {
                    if(cur_if != NULL) {
                        ftm_distance = distance;
                        ftm_required = 0;
                        INFO("DISTANCE_FTM = %f ~=  %d\n",ftm_distance*100, (int) ftm_distance*100);
                        mlocation_response_frame(cur_if, &radio_frame_recv.bssid, &radio_frame_recv.tsf_low, (int)ftm_distance*100);
                    }
                    else
                        ERR("FTM_RES: Interface information is NULL\n");

                }
#endif

		if (distance <= 0) {
			ERR("FTM exchange yielded invalid distance. Make it -1 to mark an error at application");
			distance = -1;
		} else if (enable_kalman_filter) {
			struct timeval te;
			long long milliseconds;
			gettimeofday(&te, NULL);
			milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;

			range_state.range_measurement = distance;
			range_state.time = milliseconds;
			range_kalman(&range_state);
			INFO("Calculated Dist = %.01f, Overwrite with kalman Dist = %.01f",
			     distance, range_state.last_range);
			// Overwrite distance with filtered distance from kalman
			// filter
			distance = range_state.last_range;
		} else {
			INFO("Calculated Dist = %.01f", distance);
		}

		ALLOC_MSG_OR_FAIL(msg_event, MLOCATION_EVENT_DEV_SZ + 1,
				  MLOCATION_EVENT_DEV_FMT, event->iface,
				  "ftm_complete", dev->bssNum, dev->bssType,
				  UTIL_MAC2STR(dev->mac_address),
				  dev->AverageRTT, dev->AverageClockOffset,
				  distance, update_timeout);

		if (msg_event == NULL)
			ERR("failed to prepare sd request event\n");
	} break;

	case MLOCATION_RADIO_REQUEST_RECEIVED: {
#if 0
           mlocation_radio_receive_event *radio_recv;
           mlocation_session_ctrl ctrl;
           struct mwu_iface_info *cur_if = NULL;
           INFO("MLOCATION_RADIO_REQUEST_RECEIVED:\n");
           cur_if = mwu_get_interface(event->iface, MLOCATION_ID);
           INFO("MLOCATION_RADIO_REQUEST_RECEIVED:1\n");
            if(cur_if ==NULL)
                INFO("curr info null %s, event->iface");
           INFO("MLOCATION_RADIO_REQUEST_RECEIVED:2\n");

           radio_recv = (struct mlocation_radio_receive_event *) event->val;
           ftm_required = 1;
           INFO("MLOCATION_RADIO_REQUEST_RECEIVED: 3\n");

           memcpy(&radio_frame_recv, radio_recv, sizeof(mlocation_radio_receive_event));
           INFO("MLOCATION_RADIO_REQUEST_RECEIVED: 4\n");

           ctrl.action = 1;
           ctrl.ftm_for_nan_ranging = 1;
           memcpy(&ctrl.peer_mac, radio_recv->bssid, ETH_ALEN);
           INFO("sending mlocaiton session ctrl:\n");
           mwu_hexdump(MSG_ERROR, "mlocation_session_ctrl mac", &ctrl.peer_mac, ETH_ALEN);
           cmd_mlocation_session_ctrl(cur_if, &ctrl);
           INFO("mlocation session ctrl sent\n");
#endif
	} break;

	case MLOCATION_RADIO_REPORT_RECEIVED: {
		struct mlocation_radio_report_event *rm_rpt;

		INFO("MLOCATION_RADIO_REPORT_RECEIVED: 4\n");
		rm_rpt = (struct mlocation_radio_report_event *)event->val;

		ALLOC_MSG_OR_FAIL(msg_event,
				  MLOCATION_EVENT_RADIO_REPORT_RECVD_SZ + 1,
				  MLOCATION_EVENT_DEV_RADIO_REPORT_FMT,
				  event->iface, "radio_rpt_rcvd",
				  rm_rpt->distance, UTIL_MAC2STR(rm_rpt->mac));

		if (msg_event == NULL)
			ERR("failed to prepare radio report event\n");
	} break;
	case MLOCATION_ANQP_RESP_RECEIVED: {
	} break;

	default:
		ERR("Event type %d is not supported.\n", type);
		break;
	}

	if (msg_event) {
		mwu_internal_send(msg_event);
	}

	ret = ret;

fail:
	FREE(msg_event);

	LEAVE();
	return;
}

enum nan_error nan_set_ftm_params(struct module *mod, struct mlocation_cfg *cfg)
{
	int ret = NAN_ERR_SUCCESS;
	// struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	ENTER();

	if (!mod)
		return NAN_ERR_INVAL;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	cur_if->pnan_info->nan_ftm_params =
		(NAN_FTM_PARAMS *)(uintptr_t)os_zalloc(sizeof(NAN_FTM_PARAMS));

	cur_if->pnan_info->nan_ftm_params->max_burst_dur =
		cfg->burst_duration & 0x0F;
	cur_if->pnan_info->nan_ftm_params->min_delta_ftm =
		cfg->min_delta & 0x3F;
	cur_if->pnan_info->nan_ftm_params->max_ftms_per_burst =
		cfg->mlocation_per_burst & 0x1F;
	cur_if->pnan_info->nan_ftm_params->ftm_fmt_bw = cfg->bw & 0x3F;

	ERR("max_burst_dur= %d\n",
	    cur_if->pnan_info->nan_ftm_params->max_burst_dur);
	ERR("min_delta_ftm = %d\n",
	    cur_if->pnan_info->nan_ftm_params->min_delta_ftm);
	ERR("max_ftms_per_burst = %d\n",
	    cur_if->pnan_info->nan_ftm_params->max_ftms_per_burst);
	ERR("ftm_fmt_bw = %d\n", cur_if->pnan_info->nan_ftm_params->ftm_fmt_bw);

	LEAVE();

	return ret;
}

void nan_send_ftm_complete_event(mlocation_event *dev, char iface[],
				 float distance)
{
	int ret;
	struct mwu_msg *msg_event = NULL;
	int update_timeout = 3600;

	ALLOC_MSG_OR_FAIL(msg_event, MLOCATION_EVENT_DEV_SZ + 1,
			  MLOCATION_EVENT_DEV_FMT, iface, "ftm_complete",
			  dev->bssNum, dev->bssType,
			  UTIL_MAC2STR(dev->mac_address), dev->AverageRTT,
			  dev->AverageClockOffset, distance, update_timeout);

	if (msg_event) {
		ret = mwu_internal_send(msg_event);
		if (ret != MWU_ERR_SUCCESS)
			goto fail;
	}
	return;

fail:
	FREE(msg_event);
	return;
}

enum mwu_error mlocation_handle_mwu(struct mwu_msg *msg, struct mwu_msg **resp)
{
	int ret;
	char iface[IFNAMSIZ + 1];
	char cmd[MLOCATION_MAX_CMD];
	struct timeval te;

	*resp = NULL;

	INFO("*********************************\n");
	INFO("MLOCATION module receives message:\n");
	INFO("\n%s", msg->data);
	INFO("*********************************\n\n");

	/* parse the message */
	ret = sscanf(msg->data, MLOCATION_CMD_HDR_SCAN, iface, cmd);
	if (ret != 2) {
		ERR("Failed to parse cmd message: ret = %d.\n", ret);
		ALLOC_MSG_OR_FAIL(*resp, MLOCATION_MAX_RESP, "status=%d\n",
				  MLOCATION_ERR_INVALID);
		return MWU_ERR_SUCCESS;
	}

	/* process command */
	if (strcmp(cmd, "init") == 0) {
		char *kv = NULL;
		char *current = msg->data;
		int param_count = 0;
		enum mlocation_error status;
		struct mlocation_cfg cfg;
		memset(&cfg, 0, sizeof(cfg));
		for (;;) {
			kv = strtok_r(NULL, "\n", &current);
			if (kv == NULL || kv[0] == '\0')
				break;

			if (sscanf(kv, MWU_KV_FMT_U8("burst_exp"),
				   &cfg.burst_exp) == 1) {
				param_count++;
				ERR("burst_exp: %d", cfg.burst_exp);
			} else if (sscanf(kv, MWU_KV_FMT_U8("burst_duration"),
					  &cfg.burst_duration) == 1) {
				param_count++;
				ERR("burst_duration: %d", cfg.burst_duration);
			} else if (sscanf(kv, MWU_KV_FMT_U8("min_delta"),
					  &cfg.min_delta) == 1) {
				param_count++;
				ERR("min_delta: %d", cfg.min_delta);
			} else if (sscanf(kv, MWU_KV_FMT_U8("asap"),
					  &cfg.asap) == 1) {
				param_count++;
				ERR("asap: %d", cfg.asap);
			} else if (sscanf(kv,
					  MWU_KV_FMT_U8("mlocation_per_burst"),
					  &cfg.mlocation_per_burst) == 1) {
				param_count++;
				ERR("mlocation_per_burst: %d",
				    cfg.mlocation_per_burst);
			} else if (sscanf(kv, MWU_KV_FMT_U8("bw"), &cfg.bw) ==
				   1) {
				param_count++;
				ERR("bw: %d", cfg.bw);
			} else if (sscanf(kv, MWU_KV_FMT_DEC32("burst_period"),
					  (int *)&cfg.burst_period) == 1) {
				param_count++;
				ERR("burst_period: %d", cfg.burst_period);
			} else if (sscanf(kv, MWU_KV_FMT_U8("civic_location"),
					  &cfg.civic_location) == 1) {
				param_count++;
				ERR("civic_location: %d", cfg.civic_location);
			} else if (sscanf(kv,
					  MWU_KV_FMT_U8("civic_location_type"),
					  &cfg.civic_location_type) == 1) {
				param_count++;
				ERR("civic_location_type,: %d",
				    cfg.civic_location_type);
			} else if (sscanf(kv, MWU_KV_FMT_U8("ca_type"),
					  &cfg.ca_type) == 1) {
				param_count++;
				ERR("ca_type: %d", cfg.ca_type);
			} else if (sscanf(kv, MWU_KV_FMT_U8("ca_length"),
					  &cfg.ca_length) == 1) {
				param_count++;
				ERR("ca_length: %d", cfg.ca_length);
			} else if (sscanf(kv,
					  MWU_KV_ONE_LINE_STR("ca_value",
							      MAX_CA_VALUES),
					  cfg.ca_value) == 1) {
				param_count++;
				ERR("address: %s", cfg.ca_value);
			} else if (sscanf(kv, MWU_KV_FMT_U8("country_code"),
					  &cfg.country_code) == 1) {
				param_count++;
				ERR("country_code: %d", cfg.country_code);
			} else if (sscanf(kv, MWU_KV_FMT_U8("lci_request"),
					  &cfg.lci_request) == 1) {
				param_count++;
				ERR("lci_request: %d", cfg.lci_request);
			} else if (sscanf(kv, MWU_KV_FMT_DOUBLE("latitude"),
					  &cfg.latitude) == 1) {
				param_count++;
				ERR("latitude: %lf", cfg.latitude);
			} else if (sscanf(kv, MWU_KV_FMT_DOUBLE("longitude"),
					  &cfg.longitude) == 1) {
				param_count++;
				ERR("longitude: %lf", cfg.longitude);
			} else if (sscanf(kv, MWU_KV_FMT_DOUBLE("altitude"),
					  &cfg.altitude) == 1) {
				param_count++;
				ERR("altitude: %lf", cfg.altitude);
			} else if (sscanf(kv, MWU_KV_FMT_U8("lat_unc"),
					  &cfg.lat_unc) == 1) {
				param_count++;
				ERR("lat_unc: %d", cfg.lat_unc);
			} else if (sscanf(kv, MWU_KV_FMT_U8("long_unc"),
					  &cfg.long_unc) == 1) {
				param_count++;
				ERR("long_unc: %d", cfg.long_unc);
			} else if (sscanf(kv, MWU_KV_FMT_U8("alt_unc"),
					  &cfg.alt_unc) == 1) {
				param_count++;
				ERR("lat_unc: %d", cfg.alt_unc);
			} else if (sscanf(kv,
					  MWU_KV_FMT_DOUBLE("range_drive_var"),
					  &range_drive_var) == 1) {
				param_count++;
				enable_kalman_filter = 1;
				ERR("range_drive_var: %lf", range_drive_var);
			} else if (sscanf(kv,
					  MWU_KV_FMT_DOUBLE(
						  "range_measurement_var"),
					  &range_measurement_var) == 1) {
				param_count++;
				enable_kalman_filter = 1;
				ERR("range_measurement_var: %lf",
				    range_measurement_var);
			} else if (sscanf(kv,
					  MWU_KV_FMT_DOUBLE("range_rate_init"),
					  &range_rate_init) == 1) {
				param_count++;
				enable_kalman_filter = 1;
				ERR("range_rate_init: %lf", range_rate_init);
			} else if (sscanf(kv, MWU_KV_FMT_U8("fqdn_support"),
					  &fqdn_support) == 1) {
				param_count++;
				ERR("fqdn support = %d", fqdn_support);
			} else if (sscanf(kv, MWU_KV_FMT_U8("format_bw"),
					  &cfg.format_bw) == 1) {
				param_count++;
				ERR("format_bw: %d", cfg.format_bw);
			} else if (sscanf(kv,
					  MWU_KV_FMT_U8("max_i2r_sts_upto80"),
					  &cfg.max_i2r_sts_upto80) == 1) {
				param_count++;
				ERR("max_i2r_sts_upto80: %d",
				    cfg.max_i2r_sts_upto80);
			} else if (sscanf(kv,
					  MWU_KV_FMT_U8("max_r2i_sts_upto80"),
					  &cfg.max_r2i_sts_upto80) == 1) {
				param_count++;
				ERR("max_r2i_sts_upto80: %d",
				    cfg.max_r2i_sts_upto80);
			} else if (sscanf(kv,
					  MWU_KV_FMT_U8("az_measurement_freq"),
					  &cfg.az_measurement_freq) == 1) {
				param_count++;
				ERR("az_measurement_freq: %d",
				    cfg.az_measurement_freq);
			} else if (sscanf(kv,
					  MWU_KV_FMT_U8(
						  "az_number_of_measurements"),
					  &cfg.az_number_of_measurements) ==
				   1) {
				param_count++;
				ERR("az_number_of_measurements: %d",
				    cfg.az_number_of_measurements);
			} else if (sscanf(kv, MWU_KV_FMT_U8("protocol_type"),
					  &cfg.protocol_type) == 1) {
				param_count++;
				ERR("protocol_type: %d", cfg.protocol_type);
			} else {
				if (strcmp(kv, "module=mlocation") == 0)
					ERR("Module: mlocation");
				else if (strcmp(kv, "iface=nan0") == 0)
					ERR("Interface: nan0");
				else if (strcmp(kv, "cmd=init") == 0)
					ERR("Command: init");
				else
					ERR("Ignoring unknown parameter : %s",
					    kv);
			}
		}

		/* prepare the module struct */
		strncpy(mlocation_mod.iface, iface, IFNAMSIZ);
		mlocation_mod.cb = mlocation_mwu_event_cb;
		mlocation_mod.cbpriv = &mlocation_mod;
		// Initialize kalman filter
		if (enable_kalman_filter) {
			long long milliseconds;
			gettimeofday(&te, NULL);
			milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
			range_kalman_init(&range_state, 0.0f, milliseconds,
					  range_drive_var,
					  range_measurement_var,
					  range_rate_init);
		}

		status = mlocation_init(&mlocation_mod, &cfg);

		if (status == MLOCATION_ERR_SUCCESS) {
			ERR("mlocation_init SUCCESS\n");
			nan_set_ftm_params(&mlocation_mod, &cfg);
		} else {
			ERR("mlocation_init FAIL, status = %d\n", status);
		}

		ALLOC_STATUS_MSG_OR_FAIL(*resp, status);

	} else if (strcmp(cmd, "deinit") == 0) {
		strncpy(mlocation_mod.iface, iface, IFNAMSIZ);
		mlocation_deinit(&mlocation_mod, &mlocation_mwu_mod);
	} else if (strcmp(cmd, "mlocation_session") == 0) {
		struct mlocation_session mlocations;
		enum mlocation_error status;

		memset(&mlocations, 0, sizeof(mlocations));

		ret = sscanf(msg->data, MLOCATION_SESSION_CMD, iface, cmd,
			     (int *)&mlocations.action,
			     UTIL_MAC2SCAN(mlocations.mac),
			     (int *)&mlocations.channel,
			     (int *)&mlocations.civic_req,
			     (int *)&mlocations.lci_req);

		if (!(ret >= 7))
			ERR("Insufficient parameters\n");

#if 0
        INFO("mlocations.action : %d", mlocations.action);
        INFO("mlocations.mac: UTIL_MACSTR ", UTIL_MAC2STR(mlocations.mac));
        INFO("mlocation.channel : %d\n", mlocations.channel);
#endif

		/* prepare the module struct */
		strncpy(mlocation_mod.iface, iface, IFNAMSIZ);
		mlocation_mod.cb = mlocation_mwu_event_cb;
		mlocation_mod.cbpriv = &mlocation_mod;

		status = do_mlocation_session_ctrl(&mlocation_mod, &mlocations,
						   FALSE);
		ALLOC_STATUS_MSG_OR_FAIL(*resp, status); /* This is a special
							    API which returns
							    publish_id on
							    successful publish
							  */

	} else if (strcmp(cmd, "mlocation_send_radio_request") == 0) {
		struct mwu_iface_info *cur_if = NULL;
		unsigned char mac[ETH_ALEN];

		ret = sscanf(msg->data, MLOCATION_SEND_RADIO_REQUEST, iface,
			     cmd, UTIL_MAC2SCAN(mac));
		INFO("iface = %s\n", iface);
		cur_if = mwu_get_interface(iface, MLOCATION_ID);
		if (cur_if != NULL)
			mlocation_request_frame(cur_if, mac); // for measurement
							      // request frame
		else
			INFO("Interface information is NULL\n");
	} else if (strcmp(cmd, "mlocation_send_neighbor_request") == 0) {
		struct mwu_iface_info *cur_if = NULL;
		int civic_req, lci_req;
		mlocation_neighbor_req req;

		memset(&req, 0, sizeof(req));

		ret = sscanf(msg->data, MLOCATION_SEND_NEIGHBOR_REQUEST, iface,
			     cmd, &civic_req, &lci_req);
		INFO("iface = %s\n", iface);
		cur_if = mwu_get_interface(iface, MLOCATION_ID);

		req.dialog_token = 1;
		req.lci_req = lci_req;
		req.loc_civic_req = civic_req;
		if (cur_if != NULL)
			mlocation_send_neighbor_req(cur_if, &req); // for
								   // measurement
								   // request
								   // frame
		else
			INFO("Interface information is NULL\n");
	} else if (strcmp(cmd, "mlocation_send_anqp_request") == 0) {
		struct mwu_iface_info *cur_if = NULL;
		mlocation_anqp_cfg anqp;

		memset(&anqp, 0, sizeof(anqp));
		ret = sscanf(msg->data, MLOCATION_SEND_ANQP_REQUEST, iface, cmd,
			     UTIL_MAC2SCAN(anqp.mac), &anqp.civic, &anqp.lci);

		INFO("Send ANQP req: " UTIL_MACSTR, UTIL_MAC2STR(anqp.mac));

		INFO("iface = %s\n", iface);
		cur_if = mwu_get_interface(iface, MLOCATION_ID);

		if (cur_if != NULL)
			mlocation_send_anqp_req(cur_if, &anqp);
		else
			INFO("Interface information is NULL\n");
	} else {
		ERR("Got unknown command: %s\n", cmd);
		ALLOC_MSG_OR_FAIL(*resp, MLOCATION_MAX_RESP, "status=%d\n",
				  MLOCATION_ERR_UNSUPPORTED);
	}
	return MWU_ERR_SUCCESS;

fail:
	FREE(*resp);
	return ret;
}

enum mwu_error mlocation_mwu_launch(void)
{
	if (mlocation_mod.iface[0] != 0)
		return MWU_ERR_BUSY;

	return mwu_internal_register_module(&mlocation_mwu_mod);
}

void mlocation_mwu_halt(void)
{
	mwu_internal_unregister_module(&mlocation_mwu_mod);
	mlocation_mod.iface[0] = 0;
}
