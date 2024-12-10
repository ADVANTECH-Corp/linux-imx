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

/* nan.c: implementation of the nan module
 *
 * The core of this module is the state machine implemented in
 * nan_state_machine().  See nan_state_machine.jpg for a rough diagram of the
 * state machine.  Be sure to update nan_state_machine.jpg when you alter the
 * state machine.  And don't trust the jpg over the code!  It's probably out of
 * date!
 */

#include "nan.h"
#define UTIL_LOG_TAG "NAN"
#include "wps_msg.h" /* for ifreq.h */
#include <net/if_arp.h> /* For ARPHRD_ETHER */
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "util.h"
#include "wlan_hostcmd.h"
#include "util.h"
#include "nan_lib.h"
#include "mwu.h"
#include "wps_wlan.h"
#include "mwu_ioctl.h"
#include "mwu_log.h"
#include "wps_os.h" /* for timer functions */
#include "mwu_timer.h"
#include "mwu_if_manager.h"
#include "nan_sm.h"
#include "discovery_engine.h"

#include "data_engine.h"

struct module *nan_mod = NULL;
// static enum nan_state __state = NAN_STATE_INIT;
enum nan_error nan_send_sdf_event(struct mwu_iface_info *cur_if, u8 event_id,
				  struct nan_rx_sd_frame *sd_frame,
				  u8 local_instance_id);

enum nan_ui_cmd_type {
	NAN_UI_CMD_INIT,
	NAN_UI_CMD_DEINIT,
	NAN_UI_CMD_START,
	NAN_UI_CMD_STOP,
	NAN_UI_CMD_SET_STATE_INFO,
	NAN_UI_CMD_SET_CONFIG,
	NAN_UI_CMD_GET_STATE_INFO,
	NAN_UI_CMD_PUBLISH_SERVICE,
	NAN_UI_CMD_SUBSCRIBE_SERVICE,
	NAN_UI_CMD_FOLLOW_UP,
	NAN_UI_CMD_NDP_REQ,
	NAN_UI_CMD_NDP_RESP,
	NAN_UI_CMD_SCHEDULE_UPDATE,
	NAN_UI_CMD_ULW_UPDATE,
	NAN_UI_CMD_NDP_TERMINATE,
	NAN_UI_CMD_RANGING_INITIATE,
	NAN_UI_CMD_RANGING_TERMINATE,
	NAN_UI_CMD_FTM_INIT,
	NAN_UI_CMD_SCHED_REQ,
	NAN_UI_CMD_SCHED_RESP,
	NAN_UI_CMD_SCHED_UPDATE,
};

static inline const char *nan_ui_cmd_to_str(enum nan_ui_cmd_type cmd)
{
	switch (cmd) {
	case NAN_UI_CMD_INIT:
		return "INIT";
	case NAN_UI_CMD_DEINIT:
		return "DEINIT";
	case NAN_UI_CMD_START:
		return "NAN_UI_CMD_NAN_START";
	case NAN_UI_CMD_STOP:
		return "NAN_UI_CMD_NAN_STOP";
	case NAN_UI_CMD_SET_CONFIG:
		return "NAN_UI_CMD_SET_CONFIG";
	case NAN_UI_CMD_SET_STATE_INFO:
		return "NAN_UI_CMD_SET_STATE_INFO";
	case NAN_UI_CMD_GET_STATE_INFO:
		return "NAN_UI_CMD_GET_STATE_INFO";
	case NAN_UI_CMD_PUBLISH_SERVICE:
		return "NAN_UI_CMD_PUBLISH_SERVICE";
	case NAN_UI_CMD_SUBSCRIBE_SERVICE:
		return "NAN_UI_CMD_SUBSCRIBE_SERVICE";
	case NAN_UI_CMD_FOLLOW_UP:
		return "NAN_UI_CMD_FOLLOW_UP";
	case NAN_UI_CMD_NDP_REQ:
		return "NAN_UI_CMD_NDP_REQ";
	case NAN_UI_CMD_NDP_RESP:
		return "NAN_UI_CMD_NDP_RESP";
	case NAN_UI_CMD_ULW_UPDATE:
		return "NAN_UI_CMD_ULW_UPDATE";
	case NAN_UI_CMD_NDP_TERMINATE:
		return "NAN_UI_CMD_NDP_TERMINATE";
	case NAN_UI_CMD_RANGING_INITIATE:
		return "NAN_UI_CMD_RANGING_INITIATE";
	case NAN_UI_CMD_RANGING_TERMINATE:
		return "NAN_UI_CMD_RANGING_TERMINATE";
	case NAN_UI_CMD_FTM_INIT:
		return "NAN_UI_CMD_FTM_INIT";
	case NAN_UI_CMD_SCHED_REQ:
		return "NAN_UI_CMD_SCHED_REQ";
	case NAN_UI_CMD_SCHED_RESP:
		return "NAN_UI_CMD_SCHED_RESP";
	case NAN_UI_CMD_SCHED_UPDATE:
		return "NAN_UI_CMD_SCHED_UPDATE";
	default:
		return "UNKNOWN COMMAND";
	}
	return NAN_ERR_SUCCESS;
}

static inline const char *ndp_state_to_str(enum nan_state state)
{
	switch (state) {
	case NDP_IDLE:
		return "NDP_IDLE";
	case NDP_INIT:
		return "NDP_INIT";
	case NDP_CONFIRM_PENDING:
		return "NDP_CONFIRM_PENDING";
	case NDP_CONNECTED:
		return "NDP_CONNECTED";
	default:
		return "UNKNOWN";
	}
}

static inline const char *nan_state_to_str(enum nan_state state)
{
	switch (state) {
	case NAN_STATE_INIT:
		return "INIT";
	case NAN_STATE_IDLE:
		return "IDLE";
	case NAN_STATE_STARTED:
		return "NAN_STATE_STARTED";
	case NAN_STATE_PUBLISH:
		return "NAN_STATE_PUBLISH";
	case NAN_STATE_SUBSCRIBE:
		return "NAN_STATE_SUBSCRIBE";
	default:
		return "UNKNOWN";
	}
}
/* change the state */
static void change_state(struct mwu_iface_info *cur_if, enum nan_state new)
{
	INFO("Changing state from %s to %s\n",
	     nan_state_to_str(cur_if->pnan_info->cur_state),
	     nan_state_to_str(new));
	if (new == cur_if->pnan_info->cur_state)
		return;
	cur_if->pnan_info->cur_state = new;
}

/* change the ndp state */
void change_ndp_state(struct mwu_iface_info *cur_if, enum ndp_state new)
{
	INFO("Changing ndp state from %s to %s\n",
	     ndp_state_to_str(cur_if->pnan_info->cur_ndp_state),
	     ndp_state_to_str(new));

	// set the ndl_setup bit
	if (new == NDP_CONNECTED) {
		cur_if->pnan_info->ndc_info[0].ndc_setup = TRUE;
		cur_if->pnan_info->ndc_info[0].ndl_info[0].ndl_setup = TRUE;
	}

	if (new == cur_if->pnan_info->cur_ndp_state)
		return;
	cur_if->pnan_info->cur_ndp_state = new;
}

static enum nan_error nan_do_stop(struct mwu_iface_info *cur_if)
{
	int ret = NAN_ERR_SUCCESS;

	ret = nancmd_set_mode_config(cur_if, NAN_MODE_STOP);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to stop NAN");
	} else {
		INFO("NAN stop successful");
	}
	return ret;
}

static enum nan_error nan_do_start(struct mwu_iface_info *cur_if)
{
	int ret = NAN_ERR_SUCCESS;

	ret = nancmd_set_mode_config(cur_if, NAN_MODE_START);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to start NAN");
	} else {
		INFO("NAN start successful");
	}
	return ret;
}

static enum nan_error nan_do_subscribe(struct mwu_iface_info *cur_if,
				       struct nan_subscribe_service *sservc,
				       int *subscribe_id)
{
	int ret = NAN_ERR_SUCCESS;
	u8 service_hash[SERVICE_HASH_LEN];
	struct subscribed_service *internal_service_ptr;
	/* check if already suubscibing */
	if (strcmp(cur_if->pnan_info->s_service.service_name,
		   sservc->service_name) == 0) {
		ERR("Service %s already being subscribed..",
		    sservc->service_name);
		//        return NAN_ERR_BUSY;
	}

	/* compute service hash */
	compute_sha256_service_hash(sservc->service_name, service_hash);
#if 0
    /* add service hash to firmware */
    ret = nancmd_add_service_hash(cur_if, service_hash);
    if (ret != NAN_ERR_SUCCESS) {
        ERR("Failed to configure service hash to firmware");
        return ret;
    }
#endif
	/* set the service params to internal database */
	internal_service_ptr = &(cur_if->pnan_info->s_service);

	memset(internal_service_ptr, 0, sizeof(struct subscribed_service));

	internal_service_ptr->subscribe_id =
		SUBSCRIBE_ID_START + cur_if->pnan_info->last_subscribe_id++;
	cur_if->pnan_info->last_subscribe_id =
		internal_service_ptr->subscribe_id;
	memcpy(internal_service_ptr->service_hash, service_hash,
	       SERVICE_HASH_LEN);
	/* copy rest of the struct */
	memcpy(internal_service_ptr->service_name, sservc->service_name, 255);

	/* matching filter is a length-value pair. so store accordingly while
	 * storing */
	if (strcasecmp(sservc->matching_filter_tx, "null") != 0) {
		internal_service_ptr->tx_filter_len = sservc->tx_filter_len;
		ERR("==> Matching tx filter is len is %d",
		    internal_service_ptr->tx_filter_len);
		memcpy(internal_service_ptr->matching_filter_tx,
		       sservc->matching_filter_tx,
		       internal_service_ptr->tx_filter_len);
		mwu_hexdump(MSG_ERROR, "==> Matching filter tx is",
			    (u8 *)internal_service_ptr->matching_filter_tx,
			    internal_service_ptr->tx_filter_len);
	} else {
		strcpy(internal_service_ptr->matching_filter_tx, "null");
	}

	/* matching filter is a length-value pair. so store accordingly while
	 * storing */
	if (strcasecmp(sservc->matching_filter_rx, "null") != 0) {
		internal_service_ptr->rx_filter_len = sservc->rx_filter_len;
		ERR("==> Matchingn rx filter is len is %d",
		    internal_service_ptr->rx_filter_len);
		memcpy(internal_service_ptr->matching_filter_rx,
		       sservc->matching_filter_rx,
		       internal_service_ptr->rx_filter_len);
		ERR("==> Matching filter rx is %s",
		    internal_service_ptr->matching_filter_rx);
		mwu_hexdump(MSG_ERROR, "==> Matching filter tx is",
			    (u8 *)internal_service_ptr->matching_filter_tx,
			    internal_service_ptr->tx_filter_len);
	} else {
		strcpy(internal_service_ptr->matching_filter_rx, "null");
	}

	memcpy(internal_service_ptr->matching_filter_tx,
	       sservc->matching_filter_tx, 255);
	memcpy(internal_service_ptr->matching_filter_rx,
	       sservc->matching_filter_rx, 255);
	memcpy(internal_service_ptr->service_info, sservc->service_info, 255);
	internal_service_ptr->subscribe_type = sservc->subscribe_type;
	internal_service_ptr->discovery_range = sservc->discovery_range;
	internal_service_ptr->query_period = sservc->query_period;
	ERR("Query period is %d", internal_service_ptr->query_period);
	internal_service_ptr->time_to_live = sservc->ttl;
	internal_service_ptr->matching_filter_flag =
		sservc->matching_filter_flag;

	*subscribe_id = internal_service_ptr->subscribe_id;

	/* check if publish type is solicited or unsolicited */
	if (sservc->subscribe_type & SUBSCRIBE_TYPE_ACTIVE) {
		ERR("Doing active subscribe");
		ret = nan_start_unsolicited_subscribe(cur_if, sservc);
		if (ret != NAN_ERR_SUCCESS) {
			ERR("Could not start unsolicited subscribe");
			goto fail;
		}
	} else {
		/*
		ERR("Sending solicited action frame");
		//u8 peer_mac[6] = {0x00, 0x90, 0x4c, 0x19, 0x70, 0x21}; For
		Broadcom u8 peer_mac[6] = {0x02, 0x08, 0x22, 0x11, 0x11, 0x11};
		// For Mediatek ret = nan_tx_sdf(cur_if, SUBSCRIBE, peer_mac);
		if (ret == NAN_ERR_SUCCESS)
		    ERR("Tx subscribe frame success");
		else
		    ERR("Failde to tx sd frame");
	*/
	}
	/* solicited publish is handled in event path */

	return ret;

fail:
	nancmd_remove_service_hash(cur_if, service_hash);
	/* remove internal service */
	memset(internal_service_ptr, 0, sizeof(struct published_service));
	return NAN_ERR_INVAL;
}

static enum nan_error nan_do_publish(struct mwu_iface_info *cur_if,
				     struct nan_publish_service_conf *pservc,
				     int *publish_id)
{
	int ret = NAN_ERR_SUCCESS;
	u8 service_hash[SERVICE_HASH_LEN];
	struct published_service *internal_service_ptr;
	/* check if already publishing */
	if (strcmp(cur_if->pnan_info->p_service.service_name,
		   pservc->service_name) == 0) {
		ERR("Service %s already being published..",
		    pservc->service_name);
		//      return NAN_ERR_BUSY;
	}

	if (pservc->ttl == 1) {
		return NAN_ERR_SUCCESS;
	}

	/* compute service hash */
	compute_sha256_service_hash(pservc->service_name, service_hash);

	/* add service hash to firmware */
	ret = nancmd_add_service_hash(cur_if, service_hash);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to configure service hash to firmware");
		return ret;
	}

	/* set the service params to internal database */
	internal_service_ptr = &(cur_if->pnan_info->p_service);

	memset(internal_service_ptr, 0, sizeof(struct published_service));

	internal_service_ptr->publish_id =
		PUBLISH_ID_START + cur_if->pnan_info->last_publish_id++;
	cur_if->pnan_info->last_publish_id = internal_service_ptr->publish_id;
	memcpy(internal_service_ptr->service_hash, service_hash,
	       SERVICE_HASH_LEN);
	/* copy rest of the struct */
	memcpy(internal_service_ptr->service_name, pservc->service_name, 255);

	/* matching filter is a length-value pair. so store accordingly while
	 * storing */
	if (strcasecmp(pservc->matching_filter_tx, "null") != 0) {
		internal_service_ptr->tx_filter_len = pservc->tx_filter_len;
		ERR("==> Matching tx filter is len is %d",
		    internal_service_ptr->tx_filter_len);
		memcpy(internal_service_ptr->matching_filter_tx,
		       pservc->matching_filter_tx,
		       internal_service_ptr->tx_filter_len);
		mwu_hexdump(MSG_ERROR, "==> Matching filter tx is",
			    (u8 *)internal_service_ptr->matching_filter_tx,
			    internal_service_ptr->tx_filter_len);
	} else {
		strcpy(internal_service_ptr->matching_filter_tx, "null");
	}

	/* matching filter is a length-value pair. so store accordingly while
	 * storing */
	if (strcasecmp(pservc->matching_filter_rx, "null") != 0) {
		internal_service_ptr->rx_filter_len = pservc->rx_filter_len;
		ERR("==> Matchingn rx filter is len is %d",
		    internal_service_ptr->rx_filter_len);
		memcpy(internal_service_ptr->matching_filter_rx,
		       pservc->matching_filter_rx,
		       internal_service_ptr->rx_filter_len);
		ERR("==> Matching filter rx is %s",
		    internal_service_ptr->matching_filter_rx);
		mwu_hexdump(MSG_ERROR, "==> Matching filter tx is",
			    (u8 *)internal_service_ptr->matching_filter_tx,
			    internal_service_ptr->tx_filter_len);
	} else {
		strcpy(internal_service_ptr->matching_filter_rx, "null");
	}
	memcpy(internal_service_ptr->service_info, pservc->service_info, 255);
	internal_service_ptr->publish_type = pservc->publish_type;
	internal_service_ptr->discovery_range =
		pservc->discovery_range; /* discovery_range = 0 - limited, 255 -
					    all */
	internal_service_ptr->solicited_tx_type = pservc->solicited_tx_type;
	internal_service_ptr->announcement_period = pservc->announcement_period;
	internal_service_ptr->time_to_live = pservc->ttl;
	ERR("Announcement period is %d",
	    internal_service_ptr->announcement_period);
	internal_service_ptr->event_cond = pservc->event_cond;
	internal_service_ptr->matching_filter_flag =
		pservc->matching_filter_flag;
	internal_service_ptr->bloom_filter_index = pservc->bloom_filter_index;
	memcpy(internal_service_ptr->bloom_filter_tx, pservc->bloom_filter_tx,
	       255);
	*publish_id = internal_service_ptr->publish_id;

	/* check if publish type is solicited or unsolicited */
	if (pservc->publish_type == 1) {
		INFO("MFi here is %s",
		     internal_service_ptr->matching_filter_tx);
		INFO("MFr here is %s",
		     internal_service_ptr->matching_filter_rx);
		cur_if->pnan_info->instance_id = 0x00; /* instance ID should be
							  0 for unsolicited
							  publish */
		ret = nan_start_unsolicited_publish(cur_if, pservc);
		if (ret != NAN_ERR_SUCCESS) {
			ERR("Could not start unsolicited publish");
			goto fail;
		}
	} else {
		/* solicited publish is handled in event path
		    ERR("==================> Sending solicited action frame
		   <=========================");
		    //u8 peer_mac[6] = {0x00, 0x90, 0x4c, 0x19, 0x70, 0x21}; For
		   Broadcom
		    //u8 peer_mac[6] = {0x02, 0x0a, 0x00, 0x0a, 0x01, 0x06}; //
		   For Mediatek u8 peer_mac[6] = {0x00, 0xe0, 0x4c, 0x02, 0x52,
		   0x61}; // For realtek cur_if->pnan_info->instance_id = 0x0a;
		    ret = nan_tx_sdf(cur_if, PUBLISH, peer_mac);
		    if (ret == NAN_ERR_SUCCESS)
			ERR("Tx publish frame success");
		    else
			ERR("Failde to tx sd frame"); */
	}

	return ret;

fail:
	nancmd_remove_service_hash(cur_if, service_hash);
	/* remove internal service */
	memset(internal_service_ptr, 0, sizeof(struct published_service));
	return NAN_ERR_INVAL;
}

static enum nan_error nan_do_follow_up(struct mwu_iface_info *cur_if,
				       struct nan_follow_up *f)
{
	int ret = NAN_ERR_SUCCESS;

	cur_if->pnan_info->instance_id = f->remote_instance_id;

	/* firmware has a bug, when a_band=1 and sd_frame_periodicity=256
	 * firmware alternatively sends action_frames on 2.4G and 5G bands
	 * This causes 5.2.15 to fail intermittently since some unfortunate
	 * times the queued follow up gets sent on 5G causing the 2.4G sniffer
	 * check to fail To work this around, send 3 follow up frames, so
	 * atleast one goes on 2.4G */

	ret = nan_tx_sdf(cur_if, FOLLOW_UP, f->mac);
	ret = nan_tx_sdf(cur_if, FOLLOW_UP, f->mac);
	ret = nan_tx_sdf(cur_if, FOLLOW_UP, f->mac);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to send follow up");
	} else {
		INFO("Follow up tx successful");
	}

	return ret;
}

static enum nan_error nan_do_set_config(struct mwu_iface_info *cur_if,
					struct nan_params_cfg *cfg)
{
	int ret = NAN_ERR_SUCCESS;

	ret = nancmd_set_config(cur_if, cfg);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to set NAN config");
	} else {
		INFO("NAN config set successful");
	}
	return ret;
}

static enum nan_error nan_do_set_state_info(struct mwu_iface_info *cur_if,
					    struct nan_state_info *state)
{
	int ret = NAN_ERR_SUCCESS;

	ret = nancmd_set_state_info(cur_if, state);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to set NAN state info");
	} else {
		INFO("NAN state info set successful");
	}
	return ret;
}

enum nan_error nan_get_state_info(struct module *mod,
				  struct nan_state_info *state)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;
	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}
	ret = nancmd_get_state_info(cur_if, state);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to set NAN state info");
	} else {
		INFO("NAN state info set successful");
	}
	return ret;
}
static enum nan_error nan_do_deinit(struct mwu_iface_info *cur_if)
{
	char ifname[IFNAMSIZ] = {0};

	wps_cancel_timer(nan_tx_unsolicited_publish_sdf, cur_if);
	wps_cancel_timer(nan_tx_unsolicited_subscribe_sdf, cur_if);
	nan_do_stop(cur_if);
	/** Last step: de-init current interface */
	strncpy(ifname, cur_if->ifname, IFNAMSIZ);
	if (mwu_iface_deinit(cur_if->ifname, NAN_ID) != MWU_ERR_SUCCESS)
		ERR("Failed to de-initialize interface %s.\n", ifname);

	return NAN_ERR_SUCCESS;
}

extern u8 a_band_flag;

static enum nan_error nan_do_init(struct mwu_iface_info *cur_if,
				  struct nan_cfg *cfg)
{
	int ret = NAN_ERR_SUCCESS;

	/** Store interface's device MAC address */
	if (mwu_get_mac_addr(cur_if->ifname, cur_if->device_mac_addr) !=
	    MWU_ERR_SUCCESS) {
		ERR("Failed to get MAC address.\n");
		return NAN_ERR_COM;
	}
#if 0
    /* Make sure 1st byte of NAN device address OR-ed with 0x04 */
    cur_if->device_mac_addr[0] |= NAN_DEVICE_ADDR_FLIP_BIT;
    /* Start off with device address */
    mwu_set_intended_mac_addr(cur_if->ifname, cur_if->device_mac_addr);
#endif

	if (cfg) {
		a_band_flag = cfg->a_band ? TRUE : FALSE;
		ret = nancmd_config_download(cur_if, cfg);
		if (ret != NAN_ERR_SUCCESS) {
			ERR("Failed to set NAN config to firmware");
			return ret;
		}
	}

	/** Initialize NAN_INFO */
	if (mwu_nan_info_init(cur_if) != MWU_ERR_SUCCESS) {
		ERR("Failed to initialize NAN_INFO.");
		return NAN_ERR_INVAL;
	}

	return ret;
}

enum nan_error nan_deinit(struct module *mod)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_DEINIT;
	ui_cmd->len = 0;

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_init(struct module *mod, struct nan_cfg *cfg)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_iface_init(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Failed to allocate memory for new interface.");
		return NAN_ERR_NOMEM;
	}

	/** Start with an empty neighbour list */
	if (!SLIST_EMPTY(&cur_if->pnan_info->neighbours)) {
		ERR("Negihbours list is not empty.  Apparently deinit was not called.\n");
		return NAN_ERR_BUSY;
	}
	SLIST_INIT(&cur_if->pnan_info->neighbours);
	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(struct nan_cfg) + 1);
	if (!ui_cmd)
		return NAN_ERR_NOMEM;

	ui_cmd->type = NAN_UI_CMD_INIT;
	ui_cmd->len = sizeof(struct nan_cfg);
	memcpy(ui_cmd->val, cfg, ui_cmd->len);

	/*Copy status of confirm required flag*/
	cur_if->pnan_info->a_band = cfg->a_band;

	u8 default_pmk[32] = {0xF0, 0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x4e,
			      0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e,
			      0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x4e, 0x41,
			      0x4e, 0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x0F};

	cur_if->pnan_info->nan_security.pmk_len = PMK_LEN;
	memcpy(cur_if->pnan_info->nan_security.pmk, default_pmk,
	       cur_if->pnan_info->nan_security.pmk_len);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);
	return ret;
}

enum nan_error nan_start(struct module *mod)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_START;
	ui_cmd->len = 0;

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_publish(struct module *mod,
			   struct nan_publish_service_conf *service)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}
	INFO("33Cur_if is %p", cur_if);
	INFO("33Pnan_info is %p", cur_if->pnan_info);

	ui_cmd = (struct event *)malloc(
		sizeof(struct event) + sizeof(struct nan_publish_service_conf) +
		1);
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	memset(ui_cmd, 0,
	       sizeof(struct event) + sizeof(struct nan_publish_service_conf) +
		       1);

	ui_cmd->type = NAN_UI_CMD_PUBLISH_SERVICE;
	ui_cmd->len = sizeof(struct nan_publish_service_conf);
	memcpy(ui_cmd->val, service, ui_cmd->len);

	INFO("44Cur_if is %p", cur_if);
	INFO("44Pnan_info is %p", cur_if->pnan_info);
	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_subscribe(struct module *mod,
			     struct nan_subscribe_service *service)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(struct nan_subscribe_service) +
					1);
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_SUBSCRIBE_SERVICE;
	ui_cmd->len = sizeof(struct nan_subscribe_service);
	memcpy(ui_cmd->val, service, ui_cmd->len);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ndp_request(struct module *mod, nan_ndp_req *ndp)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(nan_ndp_req) + 1);
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_NDP_REQ;
	ui_cmd->len = sizeof(nan_ndp_req);
	memcpy(ui_cmd->val, ndp, ui_cmd->len);

	/*Copy status of confirm required flag*/
	cur_if->pnan_info->confirm_required = ndp->confirm_required;

	/*Validate status of security flag*/
	if (cur_if->pnan_info->security_required ^ ndp->security) {
		ERR("NAN2: Mismatch detected. Security type %d cannot be used",
		    ndp->security);
	}

	/*Copy peer MAC addr*/
	memcpy(cur_if->pnan_info->peer_mac, ndp->responder_mac, ETH_ALEN);
	memcpy(cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac,
	       ndp->responder_mac, ETH_ALEN);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ndp_response(struct module *mod, nan_ndp_resp *ndp)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(nan_ndp_resp) + 1);
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_NDP_RESP;
	ui_cmd->len = sizeof(nan_ndp_resp);
	memcpy(ui_cmd->val, ndp, ui_cmd->len);

	/*Validate status of security flag*/
	if (cur_if->pnan_info->security_required ^ ndp->security) {
		ERR("NAN2: Mismatch detected. Security type %d cannot be used",
		    ndp->security);
	}

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ulw_update(struct module *mod, nan_ulw_param *ulw)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(nan_ndp_resp) + 1);
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}
	ui_cmd->type = NAN_UI_CMD_ULW_UPDATE;
	ui_cmd->len = sizeof(nan_ulw_param);
	memcpy(ui_cmd->val, ulw, ui_cmd->len);

	INFO("Countdown %d", (u8)ulw->count_down);
	INFO("op_class %d", (u8)ulw->op_class);
	INFO("channel %d", (u8)ulw->channel);
	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_set_avail_attr(struct module *mod, u8 *avail, u8 len)
{
	// struct nan_schedule *ndl_sched;
	peer_availability_info *self_info;
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;
	int i, j;
	u8 temp;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	if (len % 2)
		return NAN_ERR_INVAL;

	cur_if->pnan_info->avail_attr.len = (len / 2) - 1;

	for (i = 2, j = 0; i < len; i += 2, j += 1) {
		sscanf((char *)(avail + i), "%2hhx", &temp);
		cur_if->pnan_info->avail_attr.data[j] = temp;
	}

	mwu_hexdump(MSG_MSGDUMP, "avail_attr",
		    cur_if->pnan_info->avail_attr.data,
		    cur_if->pnan_info->avail_attr.len);

	// ndl_sched = &cur_if->pnan_info->ndl_sched;
	self_info = &cur_if->pnan_info->self_avail_info;
	/*Clear potential entries before populating availability entries.
	  As of now only potential entries are updated with this command */
	self_info->potential_valid = 0;
	nan_parse_availability(
		(nan_availability_attr *)cur_if->pnan_info->avail_attr.data,
		self_info);
	self_info->entry_potential[0].time_bitmap[0] &= NAN_POTENTIAL_BITMAP;
	return ret;
}

enum nan_error nan_set_ndc_attr(struct module *mod, u8 *ndc, u8 len)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;
	int i, j;
	u8 temp;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	cur_if->pnan_info->ndc_attr.len = (len / 2) - 1;

	for (i = 2, j = 0; i < len; i += 2, j += 1) {
		sscanf((char *)(ndc + i), "%2hhx", &temp);
		cur_if->pnan_info->ndc_attr.data[j] = temp;
	}

	mwu_hexdump(MSG_MSGDUMP, "ndc_attr", cur_if->pnan_info->ndc_attr.data,
		    cur_if->pnan_info->ndc_attr.len);

	return ret;
}

enum nan_error nan_set_ndl_attr(struct module *mod, u8 *ndl, u8 len)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;
	int i, j;
	u8 temp;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	cur_if->pnan_info->ndl_attr.len = (len + 1) / 2;

	for (i = 0, j = 0; i < len; i += 2, j += 1) {
		sscanf((char *)(ndl + i), "%2hhx", &temp);
		cur_if->pnan_info->ndl_attr.data[j] = temp;
	}

	mwu_hexdump(MSG_MSGDUMP, "ndl_attr", cur_if->pnan_info->ndl_attr.data,
		    cur_if->pnan_info->ndl_attr.len);

	return ret;
}

enum nan_error nan_follow_up(struct module *mod,
			     struct nan_follow_up *follow_up)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(struct nan_follow_up) + 1);
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_FOLLOW_UP;
	ui_cmd->len = sizeof(struct nan_follow_up);
	memcpy(ui_cmd->val, follow_up, ui_cmd->len);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_stop(struct module *mod)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_STOP;
	ui_cmd->len = 0;

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ndp_terminate(struct module *mod)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_NDP_TERMINATE;
	ui_cmd->len = 0;

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ranging_initiate(struct module *mod, char peer_mac[ETH_ALEN])
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd =
		(struct event *)malloc((sizeof(struct event)) + (ETH_ALEN + 1));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_RANGING_INITIATE;
	ui_cmd->len = ETH_ALEN;
	memcpy(ui_cmd->val, peer_mac, ETH_ALEN);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_send_sched_update(struct module *mod,
				     char responder_mac[ETH_ALEN])
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd =
		(struct event *)malloc((sizeof(struct event)) + (ETH_ALEN + 1));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_SCHED_UPDATE;
	ui_cmd->len = ETH_ALEN;
	memcpy(ui_cmd->val, responder_mac, ETH_ALEN);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_sched_request(struct module *mod,
				 char responder_mac[ETH_ALEN])
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd =
		(struct event *)malloc((sizeof(struct event)) + (ETH_ALEN + 1));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_SCHED_REQ;
	ui_cmd->len = ETH_ALEN;
	memcpy(ui_cmd->val, responder_mac, ETH_ALEN);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_sched_response(struct module *mod,
				  char responder_mac[ETH_ALEN])
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd =
		(struct event *)malloc((sizeof(struct event)) + (ETH_ALEN + 1));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_SCHED_RESP;
	ui_cmd->len = ETH_ALEN;
	memcpy(ui_cmd->val, responder_mac, ETH_ALEN);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ftm_init(struct module *mod)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_FTM_INIT;
	ui_cmd->len = 0;

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_ranging_terminate(struct module *mod)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event));
	if (!ui_cmd) {
		ERR("Not enough memory.");
		return NAN_ERR_NOMEM;
	}

	ui_cmd->type = NAN_UI_CMD_RANGING_TERMINATE;
	ui_cmd->len = 0;

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);

	return ret;
}

enum nan_error nan_set_schedule_update(struct module *mod,
				       struct nan_schedule *sched)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	cur_if->pnan_info->schedule_update_needed = 1;
	/* copy sched attr */
	memcpy(&cur_if->pnan_info->ndl_sched, sched,
	       sizeof(struct nan_schedule));

	return ret;
}

enum nan_error nan_ndp_set_counter_proposal(struct module *mod,
					    struct nan_schedule *ndl_sched)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	cur_if->pnan_info->counter_proposal_needed = 1;
	/* copy sched attr */
	memcpy(&cur_if->pnan_info->counter_proposal, ndl_sched,
	       sizeof(struct nan_schedule));

	return ret;
}

enum nan_error nan_ndp_set_immutable_sched(struct module *mod, u32 bitmap)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}
	/*Set immutable schedule needed flag*/
	cur_if->pnan_info->immutable_sched_required = 1;
	cur_if->pnan_info->immutable_bitmap = bitmap;
	return ret;
}

enum nan_error nan_qos_update(struct module *mod, ndl_qos_t *qos)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}
	/*Set qos params in self info*/
	cur_if->pnan_info->self_avail_info.qos.min_slots = qos->min_slots;
	cur_if->pnan_info->self_avail_info.qos.max_latency = qos->max_latency;
	ERR("NAN2: QOS params set to Min slots:%d Max latency:%d",
	    qos->min_slots, qos->max_latency);
	return ret;
}

enum nan_error configure_nan_srf(struct module *mod, struct nan_srf *srf)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	/* copy this struct to nan_srf */
	memcpy((u8 *)&cur_if->pnan_info->saved_srf, (u8 *)srf,
	       sizeof(struct nan_srf));
	mwu_hexdump(MSG_INFO, "saved_srf", (u8 *)&cur_if->pnan_info->saved_srf,
		    sizeof(struct nan_srf));
	cur_if->pnan_info->include_srf = 1;

	return ret;
}

enum nan_error nan_set_config(struct module *mod, struct nan_params_cfg *cfg)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(struct nan_params_cfg) + 1);
	if (!ui_cmd)
		return NAN_ERR_NOMEM;

	ui_cmd->type = NAN_UI_CMD_SET_CONFIG;
	ui_cmd->len = sizeof(struct nan_params_cfg);
	memcpy(ui_cmd->val, cfg, ui_cmd->len);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);
	return ret;
}

enum nan_error nan_set_state_info(struct module *mod,
				  struct nan_state_info *cfg)
{
	int ret = NAN_ERR_SUCCESS;
	struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	ui_cmd = (struct event *)malloc(sizeof(struct event) +
					sizeof(struct nan_state_info) + 1);
	if (!ui_cmd)
		return NAN_ERR_NOMEM;

	ui_cmd->type = NAN_UI_CMD_SET_STATE_INFO;
	ui_cmd->len = sizeof(struct nan_state_info);
	memcpy(ui_cmd->val, cfg, ui_cmd->len);

	ret = nan_state_machine(cur_if, (u8 *)ui_cmd, ui_cmd->len,
				NAN_EVENTS_UI);
	FREE(ui_cmd);
	return ret;
}
/* In some cases, we need to know the peer that we are currently working with
 * in order to ensure that incoming events pertain to the peer we are working
 * with.  This variably should NOT be used by anybody but the state machine!
 * Ever!
 */
/*static struct nan_neighbour *current_peer;*/

/* deinit is unique among UI commands because it can be invoked from any state,
 * and it always results in transitioning to the INIT state.  So we can have a
 * common handler for this particular event that alters our state.  This saves
 * a bit of copy-paste.  return 1 (true) if we handled the event.  Otherwise,
 * return false (0) so that some other handler can be invoked.  This is a
 * convenience function for handing the very common deinit event.
 */
static int handle_nan_deinit_event(struct mwu_iface_info *cur_if, int es,
				   struct event *cmd)
{
	if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_DEINIT) {
		nan_do_deinit(cur_if);
		return 1;
	}
	return 0;
}

enum nan_error nan_handle_ndp_state(int event, struct mwu_iface_info *cur_if,
				    void *buffer, u16 size, short type)
{
	enum ndp_state cur_state = NAN_STATE_IDLE;
	enum nan_error ret = NAN_ERR_SUCCESS;
	u8 *buff = NULL;
	nan_ndp_req *ndp_req = NULL;

	/** Get current state */
	cur_state = cur_if->pnan_info->cur_ndp_state;

	/* ndp state machine */
	switch (cur_state) {
	case NDP_IDLE:
		if (event == NAN_EVENTS_DRIVER && type == NDP_REQ) {
			buff = (u8 *)buffer;
			ERR("Handling NDP request");
			ret = nan_handle_ndp_req(cur_if, buff, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed NDP request");
			} else {
				WARN("Invalid NDP request received");
			}
		} else if (event == NAN_EVENTS_UI &&
			   type == NAN_UI_CMD_NDP_REQ) {
			int ndp_req_id = -1;
			ndp_req = (nan_ndp_req *)buffer;
			ret = nan_send_ndp_req(cur_if, ndp_req, &ndp_req_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP request");
				return ret;
			}
			change_ndp_state(cur_if, NDP_INIT);
		}
		break;

	case NDP_INIT:
		if (event == NAN_EVENTS_DRIVER && type == NDP_RESP) {
			buff = (u8 *)buffer;
			ERR("Handling NDP response");
			ret = nan_handle_ndp_resp(cur_if, buff, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed NDP response");
			} else {
				WARN("Ignored NDP response");
			}
		} else if (event == NAN_EVENTS_UI &&
			   type == NAN_UI_CMD_NDP_RESP) {
			u8 ndp_status, ndl_status, reason;
			ndp_req = (nan_ndp_req *)buffer;
			// parse NDP Request
			INFO("parse NDP request");
			ret = nan_parse_ndp_req(cur_if, &ndl_status,
						&ndp_status, &reason);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to parse NDP request");
				return ret;
			}
			if (cur_if->pnan_info->security_required) {
				// Generate  Nonce value and PTK required for M2
				nan_gen_ptk_nonce_m2(
					cur_if->pnan_info->nan_security.pmk,
					(size_t)cur_if->pnan_info->nan_security
						.pmk_len,
					(cur_if->pnan_info->ndc_info[0]
						 .ndl_info[0]
						 .ndp_info[0]
						 .initiator_ndi),
					(cur_if->pnan_info->ndc_info[0]
						 .ndl_info[0]
						 .ndp_info[0]
						 .responder_ndi),
					cur_if->pnan_info->nan_security
						.peer_key_nonce,
					(u8 *)(&(cur_if->pnan_info->nan_security
							 .local_key_nonce)),
					(u8 *)(&(cur_if->pnan_info->nan_security
							 .ptk_buf)));

				mwu_hexdump(
					MSG_INFO,
					"M2 Nonce cur_if->pnan_info->nan_security.local_key_nonce",
					(u8 *)cur_if->pnan_info->nan_security
						.local_key_nonce,
					NONCE_LEN);

				ret = nan_generate_validate_pmkid(
					cur_if->pnan_info->nan_security.pmk,
					(size_t)cur_if->pnan_info->nan_security
						.pmk_len,
					(u8 *)(&(cur_if->pnan_info->peer_mac)),
					cur_if->device_mac_addr,
					cur_if->pnan_info->nan_security
						.nan_pmkid,
					cur_if->pnan_info->p_service
						.service_hash,
					(u8 *)(&(cur_if->pnan_info->nan_security
							 .m1->buf)),
					cur_if->pnan_info->nan_security.m1
						->size);

				if (ret != NAN_ERR_SUCCESS) {
					ndp_status = NDP_NDL_STATUS_REJECTED;
					ndl_status = NDP_NDL_STATUS_REJECTED;
					reason =
						NAN_REASON_CODE_SECURITY_POLICY;
					ERR("PMKID mismatch, M2 key descriptor not updated\n");
				}
			}

			// Send NDP Response or Counter or Terminate
			INFO("Send NDP response");
			ret = nan_send_ndp_resp(cur_if, ndp_req, ndl_status,
						ndp_status, reason);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP response");
				change_ndp_state(cur_if, NDP_IDLE);
				return ret;
			}
			if (NDP_NDL_STATUS_ACCEPTED == ndl_status &&
			    NDP_NDL_STATUS_ACCEPTED == ndp_status) {
				change_ndp_state(cur_if, NDP_CONNECTED);
				nan_send_data_confirm_event(
					cur_if,
					cur_if->pnan_info->rx_ndp_req->buf,
					cur_if->pnan_info->rx_ndp_req->size);
				nancmd_set_final_bitmap(cur_if, NAN_NDL);
				/*Open nan interface*/
				nan_iface_open();
				/** Send NDPE event to app */
				ipv6_link_local_tlv *ipv6_tlv = NULL;
				nan_get_ipv6_link_local_tlv(cur_if, ipv6_tlv);
				nan_send_ndpe_data_event(cur_if, ipv6_tlv, NULL,
							 NULL);
				/*Clear peers published entries*/
				nan_clear_peer_avail_published_entries(cur_if);
			} else if (NDP_NDL_STATUS_REJECTED == ndl_status ||
				   NDP_NDL_STATUS_REJECTED == ndl_status) {
				memset(&cur_if->pnan_info->peer_avail_info, 0,
				       sizeof(peer_availability_info));
				memset(&cur_if->pnan_info->ndc_info, 0,
				       sizeof(NDC_INFO));
				change_ndp_state(cur_if, NDP_IDLE);
			} else {
				change_ndp_state(cur_if, NDP_CONFIRM_PENDING);
			}
			FREE(cur_if->pnan_info->rx_ndp_req);
		}
		break;

	case NDP_CONFIRM_PENDING:
		if (event == NAN_EVENTS_DRIVER && type == NDP_CONFIRM) {
			buff = (u8 *)buffer;
			ERR("Received NDP confirm");
			ret = nan_handle_ndp_confirm(cur_if, buff, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed NDP confirm");
			} else {
				WARN("Ignored NDP confirm");
			}
			break;
		}
		if (event == NAN_EVENTS_DRIVER && type == NDP_KEY_INSTALL) {
			buff = (u8 *)buffer;
			ERR("Received NDP install");
			ret = nan_handle_security_install(cur_if, buff, size);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to do security install");
				return ret;
			}
			break;
		}

	case NDP_CONNECTED:
		if (event == NAN_EVENTS_DRIVER && type == NDP_REQ) {
			buff = (u8 *)buffer;
			ERR("Handling NDP request for second peer");
			nan_clear_peer_avail_entries(cur_if);
			ret = nan_handle_ndp_req(cur_if, buff, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed NDP request");
			} else {
				WARN("Invalid NDP request received");
			}
			break;
		}

		if (event == NAN_EVENTS_UI &&
		    type == NAN_UI_CMD_NDP_TERMINATE) {
			ret = nan_send_ndp_terminate(cur_if);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP terminate");
				return ret;
			}
			/* TODO :: Better way to handle terminate.
			 * This has been added only for security test case,
			 * as behaviour is different for one particular security
			 * test case*/
			if (cur_if->pnan_info->security_required &&
			    !cur_if->pnan_info->pmf_required) {
				break;
			} else {
				change_ndp_state(cur_if, NDP_IDLE);
				nan_clear_avail_entries(cur_if);
				nan_iface_close();
			}
			break;
		}

		if (event == NAN_EVENTS_DRIVER && type == NDP_TERMINATE) {
			ERR("Received NDP terminate from peer");
			change_ndp_state(cur_if, NDP_IDLE);
			nan_clear_avail_entries(cur_if);
			nan_iface_close();
			// ret = nan_do_stop(cur_if);
			break;
		}
		break;

	default:
		break;
	}
	LEAVE();
	return ret;
}

int nan_state_machine(struct mwu_iface_info *cur_if, u8 *buffer, u16 size,
		      int es)
{
	int ret = NAN_ERR_SUCCESS;
	//    apeventbuf_nan_generic *nan_event = NULL;
	int e = 0;
	// int sub = 0;
	struct event *cmd = NULL;
	// struct nan_neighbour *neighbour = NULL;
	enum nan_state cur_state = NAN_STATE_INIT;

	// nan_generic_event *nan_event = NULL;
	nan_service_descriptor_attr *rx_sda;
	u8 peer_mac[6];
	int rx_frame_type = -1;
	int rx_frame_sub_type = -1;
	u8 *sda_buffer;
	u8 *tmp;
	int rx_rssi;

	ENTER();

	if (!cur_if || !buffer)
		return NAN_ERR_INVAL;

	/** Get current state */
	cur_state = cur_if->pnan_info->cur_state;

	if (es == NAN_EVENTS_DRIVER) {
		INFO("==> Need to handle driver envet for NAN");
		mwu_hexdump(MSG_INFO,
			    "NAN Generic event buffer received:", buffer, size);

#if 0
        nan_event = (nan_generic_event *) buffer;
        /* do the parsing and update the table if needed.*/
        if (nan_neighbour_parse(nan_event, &temp_neighbour) != 0) {
            WARN("Failed to parse driver event.  Proceeding anyway.\n");
        }
#endif

		/* quick and dirty cast for knowing the common elements early */
		/* 16 = sizeof (peer_mac(6) + rssi (4)
			+ cat_code(0x04) + public_act (0x09) + OUI (50,6f,9a)
			+ subtype (19)) */
		sda_buffer = buffer + 16;
		tmp = buffer + 6;
		memcpy(&rx_rssi, tmp, sizeof(u32));
		rx_rssi = wlan_le32_to_cpu(*((u32 *)tmp));
		memcpy(peer_mac, buffer, ETH_ALEN);
		INFO("Rx RSSI is %d", rx_rssi);
		rx_frame_sub_type = *sda_buffer;

		rx_frame_type = *(sda_buffer - 1);

		if (rx_frame_sub_type >= NDP_REQ &&
		    rx_frame_sub_type <= NDP_TERMINATE) {
			rx_frame_type = NDP_FRAME;
			INFO("NDP frame subtype 0x%0x", rx_frame_sub_type);
		} else if (rx_frame_type == NAN_ACTION_FRAME &&
			   (rx_frame_sub_type >= RANGING_REQUEST &&
			    rx_frame_sub_type <= RANGING_REPORT)) {
			INFO("Ranging frame");
		} else if (rx_frame_type == NAN_ACTION_FRAME &&
			   (rx_frame_sub_type >= SCHEDULE_REQUEST &&
			    rx_frame_sub_type <= SCHEDULE_UPDATE)) {
			INFO("Schedule frame");
		} else {
			rx_sda = (nan_service_descriptor_attr *)sda_buffer;
			rx_frame_type = SD_FRAME;
			rx_frame_sub_type = rx_sda->service_control_bitmap &
					    3; /* last 2 bits of control btmap
						  is frame type */
			INFO("Service control bitmap is 0x%02x",
			     rx_sda->service_control_bitmap);
			INFO("Rx frame type is %d", rx_frame_sub_type);
		}

	} else if (es == NAN_EVENTS_UI) {
		cmd = (struct event *)buffer;
		e = cmd->type;
		INFO("Got UI event %s in state %s\n", nan_ui_cmd_to_str(e),
		     nan_state_to_str(cur_state));
	} else if (es == NAN_EVENTS_DRIVER_SDF_TX_DONE) {
		INFO("==> SDF Tx Done");
	} else {
		ERR("Unexpected event space: %d\n", es);
		return NAN_ERR_INVAL;
	}

	/* Handle the events that do not affect our state early */
#if 0
    if (cur_state == NAN_STATE_PUBLISH && rx_frame_sub_type == SERVICE_CTRL_BITMAP_PUBLISH) {
        ERR("Not hadling publish rx frame in state Publish");
        return NAN_ERR_NOTREADY;
    }

    if (cur_state == NAN_STATE_SUBSCRIBE && rx_frame_sub_type == SERVICE_CTRL_BITMAP_SUBSCRIBE) {
        ERR("Not hadling subscribe rx frame in state Subscribe");
        return NAN_ERR_NOTREADY;
    }
#endif
	if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SET_STATE_INFO) {
		ret = nan_do_set_state_info(cur_if,
					    (struct nan_state_info *)cmd->val);
		if (ret != NAN_ERR_SUCCESS) {
			ERR("NAN set state no successful");
		} else {
			INFO("NAN set state successful");
		}
		return ret;
	}

	if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SET_CONFIG) {
		ret = nan_do_set_config(cur_if,
					(struct nan_params_cfg *)cmd->val);
		if (ret != NAN_ERR_SUCCESS) {
			ERR("NAN set config not successful");
		} else {
			INFO("NAN set config successful");
		}
		return ret;
	}

	if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_FOLLOW_UP) {
		if (cur_state == NAN_STATE_INIT ||
		    cur_state == NAN_STATE_IDLE) {
			ERR("NAN is not started. Can not subscribe!");
			return NAN_ERR_NOTREADY;
		} else {
			ret = nan_do_follow_up(
				cur_if, (struct nan_follow_up *)cmd->val);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("==> Could not send follow up");
			} else {
				ERR("==> Sent follow up message");
			}
		}
	}

#if 0
    if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_PUBLISH_SERVICE) {
        if (cur_state == NAN_STATE_INIT || cur_state == NAN_STATE_IDLE || cur_state == NAN_STATE_PUBLISH) {
            ERR("NAN is not started. Can not publish!");
            return NAN_ERR_NOTREADY;
        }
    }

    if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SUBSCRIBE_SERVICE) {
        if (cur_state == NAN_STATE_INIT || cur_state == NAN_STATE_IDLE || cur_state == NAN_STATE_SUBSCRIBE) {
            ERR("NAN is not started. Can not subscribe!");
            return NAN_ERR_NOTREADY;
        }
    }
#endif

	/* Handeling received follow up does not need any state change */

	if (es == NAN_EVENTS_DRIVER && rx_frame_type == SD_FRAME &&
	    rx_frame_sub_type == SERVICE_CTRL_BITMAP_FOLLOW_UP) {
		struct nan_rx_sd_frame rx_sd_frame;
		memset(&rx_sd_frame, 0, sizeof(struct nan_rx_sd_frame));
		nan_parse_rx_sdf(cur_if, buffer, size, &rx_sd_frame,
				 SERVICE_CTRL_BITMAP_FOLLOW_UP);

		/* report an event up */
		INFO("==> Follow up received. Report to upper layer..");
		nan_send_sdf_event(cur_if, NAN_EVENT_FOLLOW_UP_RECVD,
				   &rx_sd_frame, 0x5);
	}

	/* actual state machine */
	switch (cur_state) {
	case NAN_STATE_INIT:
		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_INIT) {
			ret = nan_do_init(cur_if, (struct nan_cfg *)cmd->val);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("nan_do_init error: %d\n", ret);
				return ret;
			}
			change_state(cur_if, NAN_STATE_IDLE);
			break;
		}

		if (handle_nan_deinit_event(cur_if, es, cmd)) {
			cur_if = NULL; /* reset cur_if after successful deinit
					  i.e FREE(cur_if); */
			break;
		}

		break;

	case NAN_STATE_IDLE:
		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_START) {
			ret = nan_do_start(cur_if);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("nan_do_start error: %d\n", ret);
				return ret;
			}
			change_state(cur_if, NAN_STATE_STARTED);
			break;
		}

		if (handle_nan_deinit_event(cur_if, es, cmd)) {
			cur_if = NULL; /* reset cur_if after successful deinit
					  i.e FREE(cur_if); */
			break;
		}

		break;

	case NAN_STATE_STARTED:
		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_STOP) {
			ret = nan_do_stop(cur_if);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("nan_do_stop error: %d\n", ret);
				return ret;
			}
			change_state(cur_if, NAN_STATE_IDLE);
			change_ndp_state(cur_if, NDP_IDLE);
			break;
		}

		if (handle_nan_deinit_event(cur_if, es, cmd)) {
			cur_if = NULL; /* reset cur_if after successful deinit
					  i.e FREE(cur_if); */
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_PUBLISH_SERVICE) {
			int publish_id = -1;
			ret = nan_do_publish(
				cur_if,
				(struct nan_publish_service_conf *)cmd->val,
				&publish_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to publish a NAN service");
				return -1;
			}
			ret = publish_id; /* @TODO: fix it. uncommon status
					     return as publish_id */
			change_state(cur_if, NAN_STATE_PUBLISH);
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_SUBSCRIBE_SERVICE) {
			int subscribe_id = -1;
			ret = nan_do_subscribe(
				cur_if,
				(struct nan_subscribe_service *)cmd->val,
				&subscribe_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to publish a NAN service");
				return -1;
			}
			ret = subscribe_id; /* @TODO: fix it. uncommon status
					       return as publish_id */
			change_state(cur_if, NAN_STATE_SUBSCRIBE);
			break;
		}
		break;

	case NAN_STATE_PUBLISH:
		/* handle cancel_publish here */
		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_STOP) {
			ret = nan_do_stop(cur_if);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("nan_do_stop error: %d\n", ret);
				return ret;
			}
			change_state(cur_if, NAN_STATE_IDLE);
			change_ndp_state(cur_if, NDP_IDLE);
			break;
		}
		if (handle_nan_deinit_event(cur_if, es, cmd)) {
			cur_if = NULL; /* reset cur_if after successful deinit
					  i.e FREE(cur_if); */
			break;
		}

		if (es == NAN_EVENTS_DRIVER_SDF_TX_DONE &&
		    cur_if->pnan_info->p_service.publish_type &
			    PUBLISH_TYPE_UNSOLICITED) {
			nan_tx_unsolicited_publish_sdf((void *)cur_if);
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_PUBLISH_SERVICE) {
			int publish_id = -1;
			ret = nan_do_publish(
				cur_if,
				(struct nan_publish_service_conf *)cmd->val,
				&publish_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to publish a NAN service");
				return -1;
			}
			ret = publish_id; /* @TODO: fix it. uncommon status
					     return as publish_id */
			change_state(cur_if, NAN_STATE_PUBLISH);
			break;
		}

		if (es == NAN_EVENTS_DRIVER && rx_frame_type == SD_FRAME &&
		    rx_frame_sub_type == SERVICE_CTRL_BITMAP_SUBSCRIBE) {
			ERR("Handling received subscribe");
			ret = nan_handle_rx_subscribe(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully sent publish message");
				/* @TODO: fix it. Do we send event "replied"
				 * here?? or not?? */
			} else {
				WARN("Ignored rx subscribe frame");
			}
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_SUBSCRIBE_SERVICE) {
			int subscribe_id = -1;
			ret = nan_do_subscribe(
				cur_if,
				(struct nan_subscribe_service *)cmd->val,
				&subscribe_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to publish a NAN service");
				return -1;
			}
			ret = subscribe_id; /* @TODO: fix it. uncommon status
					       return as publish_id */
			change_state(cur_if, NAN_STATE_SUBSCRIBE);
			break;
		}

		if (es == NAN_EVENTS_DRIVER && rx_frame_type == SD_FRAME &&
		    rx_frame_sub_type == SERVICE_CTRL_BITMAP_PUBLISH) {
			ERR("Handling received publish");
			ret = nan_handle_rx_publish(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed publish message");
			} else {
				WARN("Ignored rx publish frame");
			}

			// if configured to do solicited publish and in case SDF
			// has 2 SDAs.
			if (cur_if->pnan_info->p_service.publish_type ==
			    PUBLISH_TYPE_SOLICITED) {
				ret = nan_handle_rx_subscribe(cur_if, buffer,
							      size);
				if (ret == NAN_ERR_SUCCESS) {
					INFO("Successfully sent publish message");
				} else {
					WARN("No SDA for subscribe found in the publish frame");
				}
			}

			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     (rx_frame_sub_type >= RANGING_REQUEST &&
		      rx_frame_sub_type <= RANGING_REPORT))) {
			ERR("Handling received ranging frame");
			ret = nan_handle_rx_ranging(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed ranging frame");
			} else {
				WARN("Ignored rx ranging frame");
			}
			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     rx_frame_sub_type == SCHEDULE_REQUEST)) {
			u8 ndp_status, ndl_status, reason;
			nan_ndp_req *ndp_req = NULL;
			ndp_req = (nan_ndp_req *)buffer;
			ERR("Handling SCHEDULE_REQUEST frame");
			ret = nan_handle_schedule_req(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				// parse Schedule Request
				INFO("parse Schedule request");
				ret = nan_parse_schedule_req(cur_if,
							     &ndl_status,
							     &ndp_status,
							     &reason);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to parse NDP request");
					return ret;
				}
				// Send NDP Response or Counter or Terminate
				INFO("Send Schedule response");
				ret = nan_send_schedule_resp(cur_if, ndp_req,
							     ndl_status,
							     ndp_status,
							     reason);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to send Schedule response");
					return ret;
				}
				if (NDP_NDL_STATUS_ACCEPTED == ndl_status &&
				    NDP_NDL_STATUS_ACCEPTED == ndp_status) {
					nancmd_set_final_bitmap(cur_if,
								NAN_NDL);
					/*Open nan interface*/
					nan_iface_open();
				}
				FREE(cur_if->pnan_info->rx_ndp_req);
				INFO("Successfully processed SCHEDULE_REQUEST frame");
			} else {
				WARN("Ignored rx SCHEDULE_REQUEST frame");
			}
			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     rx_frame_sub_type == SCHEDULE_RESPONSE)) {
			ERR("Handling received SCHEDULE_RESPONSE frame");
			ret = nan_handle_schedule_resp(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed SCHEDULE_RESPONSE frame");
			} else {
				WARN("Ignored rx SCHEDULE_RESPONSE frame");
			}
			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     rx_frame_sub_type == SCHEDULE_CONFIRM)) {
			ERR("Handling received SCHEDULE_CONFIRM frame");
			ret = nan_handle_schedule_confirm(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed SCHEDULE_CONFIRM frame");
			} else {
				WARN("Ignored rx SCHEDULE_CONFIRM frame");
			}
			break;
		}

		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_FTM_INIT) {
			INFO("Handling received FTM init");
			ret = nan_start_ftm_session(
				cur_if->pnan_info->peer_avail_info.peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to start FTM session");
			} else {
				ERR("FTM session start success");
			}
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_RANGING_TERMINATE) {
			ret = nan_tx_ranging_result_frame(
				cur_if, RANGING_TERMINATION,
				NAN_REASON_CODE_UNSPECIFIED, 0,
				cur_if->pnan_info->peer_avail_info.peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send RANGING terminate");
				return ret;
			}
			nan_set_ranging_bitmap(cur_if, BITMAP_RESET);
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_RANGING_INITIATE) {
			char peer_mac[ETH_ALEN];
			memcpy(peer_mac, cmd->val, ETH_ALEN);
			if (cur_if->pnan_info->peer_avail_info_published
				    .peer_ranging_required) {
				ret = nan_tx_ranging_request_frame(cur_if,
								   peer_mac);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to send Ranging initiate");
					return ret;
				}
			}
			break;
		}

		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SCHED_REQ) {
			char peer_mac[ETH_ALEN];
			memcpy(peer_mac, cmd->val, ETH_ALEN);
			ret = nan_send_schedule_req(cur_if, peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send schedule request");
				return ret;
			}
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_SCHED_UPDATE) {
			char peer_mac[ETH_ALEN];
			memcpy(peer_mac, cmd->val, ETH_ALEN);
			ret = nan_send_schedule_update(cur_if, NULL, peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send schedule request");
				return ret;
			}
			break;
		}

		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SCHED_RESP) {
			u8 ndp_status, ndl_status, reason;
			nan_ndp_req *ndp_req = NULL;
			ndp_req = (nan_ndp_req *)buffer;
			// parse Schedule Request
			INFO("parse Schedule request");
			ret = nan_parse_schedule_req(cur_if, &ndl_status,
						     &ndp_status, &reason);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to parse NDP request");
				return ret;
			}

			// Send NDP Response or Counter or Terminate
			INFO("Send Schedule response");
			ret = nan_send_schedule_resp(cur_if, ndp_req,
						     ndl_status, ndp_status,
						     reason);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send Schedule response");
				return ret;
			}
			if (NDP_NDL_STATUS_ACCEPTED == ndl_status &&
			    NDP_NDL_STATUS_ACCEPTED == ndp_status) {
				nancmd_set_final_bitmap(cur_if, NAN_NDL);
				/*Open nan interface*/
				nan_iface_open();
			}
			FREE(cur_if->pnan_info->rx_ndp_req);
			break;
		}

		if (es == NAN_EVENTS_DRIVER && rx_frame_type == NDP_FRAME &&
		    ((rx_frame_sub_type == NDP_REQ) ||
		     (rx_frame_sub_type == NDP_CONFIRM) ||
		     (rx_frame_sub_type == NDP_TERMINATE) ||
		     (rx_frame_sub_type == NDP_KEY_INSTALL))) {
			nan_handle_ndp_state(es, cur_if, (void *)buffer, size,
					     rx_frame_sub_type);
			break;
		}
		if (es == NAN_EVENTS_UI &&
		    ((cmd->type == NAN_UI_CMD_NDP_TERMINATE) ||
		     (cmd->type == NAN_UI_CMD_NDP_RESP))) {
			nan_handle_ndp_state(es, cur_if, (void *)(cmd->val), 0,
					     cmd->type);
			break;
		}
		if (es == NAN_EVENTS_DRIVER &&
		    rx_frame_type == NAN_ACTION_FRAME &&
		    (rx_frame_sub_type == SCHEDULE_UPDATE)) {
			ERR("NAN2: Received schedule update");
			nan_handle_schedule_update(cur_if, (void *)buffer,
						   size);
			break;
		}
		if (es == NAN_EVENTS_UI &&
		    (cmd->type == NAN_UI_CMD_ULW_UPDATE)) {
			char mcast_mac[6] = {0x51, 0x6f, 0x9a,
					     0x01, 0x00, 0x00};
			ERR("Sending ULW update");
			nan_send_schedule_update(
				cur_if, (nan_ulw_param *)(cmd->val), mcast_mac);
			break;
		}
		break;

	case NAN_STATE_SUBSCRIBE:
		/* handle cancel_subscribe here */
		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_STOP) {
			ret = nan_do_stop(cur_if);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("nan_do_stop error: %d\n", ret);
				return ret;
			}
			change_state(cur_if, NAN_STATE_IDLE);
			change_ndp_state(cur_if, NDP_IDLE);
			break;
		}
		if (handle_nan_deinit_event(cur_if, es, cmd)) {
			cur_if = NULL; /* reset cur_if after successful deinit
					  i.e FREE(cur_if); */
			break;
		}

		if (es == NAN_EVENTS_DRIVER_SDF_TX_DONE &&
		    cur_if->pnan_info->s_service.subscribe_type &
			    SUBSCRIBE_TYPE_ACTIVE) {
			nan_tx_unsolicited_subscribe_sdf((void *)cur_if);
		}

		if (es == NAN_EVENTS_DRIVER && rx_frame_type == SD_FRAME &&
		    rx_frame_sub_type == SERVICE_CTRL_BITMAP_PUBLISH) {
			ERR("Handling received publish");
			ret = nan_handle_rx_publish(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed publish message");
			} else {
				WARN("Ignored rx publish frame");
			}

			// if configured to do solicited publish and in case SDF
			// has 2 SDAs.
			if (cur_if->pnan_info->p_service.publish_type ==
			    PUBLISH_TYPE_SOLICITED) {
				ret = nan_handle_rx_subscribe(cur_if, buffer,
							      size);
				if (ret == NAN_ERR_SUCCESS) {
					INFO("Successfully sent publish message");
				} else {
					WARN("No SDA for subscribe found in the publish frame");
				}
			}

			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     (rx_frame_sub_type >= RANGING_REQUEST &&
		      rx_frame_sub_type <= RANGING_REPORT))) {
			ERR("Handling received ranging frame");
			ret = nan_handle_rx_ranging(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed ranging frame");
			} else {
				WARN("Ignored rx ranging frame");
			}
			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     rx_frame_sub_type == SCHEDULE_REQUEST)) {
			u8 ndp_status, ndl_status, reason;
			nan_ndp_req *ndp_req = NULL;
			ndp_req = (nan_ndp_req *)buffer;
			ERR("Handling SCHEDULE_REQUEST frame");
			ret = nan_handle_schedule_req(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				// parse Schedule Request
				INFO("parse Schedule request");
				ret = nan_parse_schedule_req(cur_if,
							     &ndl_status,
							     &ndp_status,
							     &reason);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to parse NDP request");
					return ret;
				}
				// Send NDP Response or Counter or Terminate
				INFO("Send Schedule response");
				ret = nan_send_schedule_resp(cur_if, ndp_req,
							     ndl_status,
							     ndp_status,
							     reason);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to send Schedule response");
					return ret;
				}
				if (NDP_NDL_STATUS_ACCEPTED == ndl_status &&
				    NDP_NDL_STATUS_ACCEPTED == ndp_status) {
					nancmd_set_final_bitmap(cur_if,
								NAN_NDL);
					/*Open nan interface*/
					nan_iface_open();
				}
				FREE(cur_if->pnan_info->rx_ndp_req);
				INFO("Successfully processed SCHEDULE_REQUEST frame");
			} else {
				WARN("Ignored rx SCHEDULE_REQUEST frame");
			}
			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     rx_frame_sub_type == SCHEDULE_RESPONSE)) {
			ERR("Handling received SCHEDULE_RESPONSE frame");
			ret = nan_handle_schedule_resp(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed SCHEDULE_RESPONSE frame");
			} else {
				WARN("Ignored rx SCHEDULE_RESPONSE frame");
			}
			break;
		}

		if (es == NAN_EVENTS_DRIVER &&
		    (rx_frame_type == NAN_ACTION_FRAME &&
		     rx_frame_sub_type == SCHEDULE_CONFIRM)) {
			ERR("Handling received SCHEDULE_CONFIRM frame");
			ret = nan_handle_schedule_confirm(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully processed SCHEDULE_CONFIRM frame");
			} else {
				WARN("Ignored rx SCHEDULE_CONFIRM frame");
			}
			break;
		}

		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_FTM_INIT) {
			INFO("Handling received FTM init");
			ret = nan_start_ftm_session(
				cur_if->pnan_info->peer_avail_info.peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to start FTM session");
			} else {
				ERR("FTM session start success");
			}
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_RANGING_TERMINATE) {
			ret = nan_tx_ranging_result_frame(
				cur_if, RANGING_TERMINATION,
				NAN_REASON_CODE_UNSPECIFIED, 0,
				cur_if->pnan_info->peer_avail_info.peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send Ranging terminate");
				return ret;
			}
			nan_set_ranging_bitmap(cur_if, BITMAP_RESET);
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_RANGING_INITIATE) {
			char peer_mac[ETH_ALEN];
			memcpy(peer_mac, cmd->val, ETH_ALEN);
			if (cur_if->pnan_info->peer_avail_info_published
				    .peer_ranging_required) {
				ret = nan_tx_ranging_request_frame(cur_if,
								   peer_mac);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to send Ranging initiate");
					return ret;
				}
			}
			break;
		}

		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SCHED_REQ) {
			char peer_mac[ETH_ALEN];
			memcpy(peer_mac, cmd->val, ETH_ALEN);
			ret = nan_send_schedule_req(cur_if, peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send schedule request");
				return ret;
			}
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_SCHED_UPDATE) {
			char peer_mac[ETH_ALEN];
			memcpy(peer_mac, cmd->val, ETH_ALEN);
			ret = nan_send_schedule_update(cur_if, NULL, peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send schedule request");
				return ret;
			}
			break;
		}

		if (es == NAN_EVENTS_UI && cmd->type == NAN_UI_CMD_SCHED_RESP) {
			u8 ndp_status, ndl_status, reason;
			nan_ndp_req *ndp_req = NULL;
			ndp_req = (nan_ndp_req *)buffer;
			// parse Schedule Request
			INFO("parse Schedule request");
			ret = nan_parse_schedule_req(cur_if, &ndl_status,
						     &ndp_status, &reason);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to parse NDP request");
				return ret;
			}

			// Send NDP Response or Counter or Terminate
			INFO("Send Schedule response");
			ret = nan_send_schedule_resp(cur_if, ndp_req,
						     ndl_status, ndp_status,
						     reason);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send Schedule response");
				return ret;
			}
			if (NDP_NDL_STATUS_ACCEPTED == ndl_status &&
			    NDP_NDL_STATUS_ACCEPTED == ndp_status) {
				nancmd_set_final_bitmap(cur_if, NAN_NDL);
				/*Open nan interface*/
				nan_iface_open();
			}
			FREE(cur_if->pnan_info->rx_ndp_req);
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_PUBLISH_SERVICE) {
			int publish_id = -1;
			ret = nan_do_publish(
				cur_if,
				(struct nan_publish_service_conf *)cmd->val,
				&publish_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to publish a NAN service");
				return -1;
			}
			ret = publish_id; /* @TODO: fix it. uncommon status
					     return as publish_id */
			change_state(cur_if, NAN_STATE_PUBLISH);
			break;
		}

		if (es == NAN_EVENTS_DRIVER && rx_frame_type == SD_FRAME &&
		    rx_frame_sub_type == SERVICE_CTRL_BITMAP_SUBSCRIBE) {
			ERR("Handling received subscribe");
			ret = nan_handle_rx_subscribe(cur_if, buffer, size);
			if (ret == NAN_ERR_SUCCESS) {
				INFO("Successfully sent publish message");
				/* @TODO: fix it. Do we send event "replied"
				 * here?? or not?? */
			} else {
				WARN("Ignored rx subscribe frame");
			}
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    cmd->type == NAN_UI_CMD_SUBSCRIBE_SERVICE) {
			int subscribe_id = -1;
			ret = nan_do_subscribe(
				cur_if,
				(struct nan_subscribe_service *)cmd->val,
				&subscribe_id);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to subscribe a NAN service");
				return -1;
			}
			ret = subscribe_id; /* @TODO: fix it. uncommon status
					       return as publish_id */
			change_state(cur_if, NAN_STATE_SUBSCRIBE);
			break;
		}

		if (es == NAN_EVENTS_UI &&
		    ((cmd->type == NAN_UI_CMD_NDP_REQ) ||
		     (cmd->type == NAN_UI_CMD_NDP_TERMINATE))) {
			nan_handle_ndp_state(es, cur_if, (void *)(cmd->val), 0,
					     cmd->type);
			break;
		}

		if (es == NAN_EVENTS_DRIVER && rx_frame_type == NDP_FRAME &&
		    ((rx_frame_sub_type == NDP_RESP) ||
		     (rx_frame_sub_type == NDP_CONFIRM) ||
		     (rx_frame_sub_type == NDP_TERMINATE) ||
		     (rx_frame_sub_type == NDP_KEY_INSTALL))) {
			nan_handle_ndp_state(es, cur_if, (void *)buffer, size,
					     rx_frame_sub_type);
			break;
		}
		if (es == NAN_EVENTS_DRIVER &&
		    rx_frame_type == NAN_ACTION_FRAME &&
		    (rx_frame_sub_type == SCHEDULE_UPDATE)) {
			ERR("NAN2: Received schedule update");
			nan_handle_schedule_update(cur_if, (void *)buffer,
						   size);
			break;
		}
		if (es == NAN_EVENTS_UI &&
		    (cmd->type == NAN_UI_CMD_ULW_UPDATE)) {
			char mcast_mac[6] = {0x51, 0x6f, 0x9a,
					     0x01, 0x00, 0x00};
			ERR("Sending ULW update");
			nan_send_schedule_update(
				cur_if, (nan_ulw_param *)(cmd->val), mcast_mac);
			break;
		}
	}

	LEAVE();
	return ret;
}

/* recevive and event from the driver and send it to the state machine */
void nan_driver_event(char *ifname, u8 *buffer, u16 size)
{
	struct mwu_iface_info *cur_if = NULL;
	u16 nan_event_sub_type = -1;

	ERR("NAN driver event");
	if (!ifname || !buffer) {
		ERR("Invalid input.");
		return;
	}

	cur_if = mwu_get_interface(ifname, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", ifname);
		return;
	}

	nan_event_sub_type = *((u16 *)buffer);
	buffer += 2; // Event subtype takes 2 bytes
	size -= 2;

	if (nan_event_sub_type == NAN_EVENT_SUB_TYPE_SD_EVENT)
		nan_state_machine(cur_if, buffer, size, NAN_EVENTS_DRIVER);

	if (nan_event_sub_type == NAN_EVENT_SUB_TYPE_SDF_TX_DONE)
		nan_state_machine(cur_if, buffer, size,
				  NAN_EVENTS_DRIVER_SDF_TX_DONE);
}

enum nan_error nan_send_sdf_event(struct mwu_iface_info *cur_if, u8 event_id,
				  struct nan_rx_sd_frame *sd_frame,
				  u8 local_instance_id)
{
	struct event *event = NULL;
	struct discover_result r;

	memset(&r, 0, sizeof(r));
	event = malloc(sizeof(struct event) + sizeof(struct discover_result));
	if (!event) {
		ERR("No memory.  Can't send discovery result event.\n");
		return NAN_ERR_NOMEM;
	}
	memset(event, 0, sizeof(struct event) + sizeof(struct discover_result));
	event->type = event_id;
	event->status = NAN_ERR_SUCCESS;
	strncpy(event->iface, cur_if->ifname, IFNAMSIZ);

	event->len = sizeof(struct discover_result);

	r.remote_instance_id = sd_frame->instance_id;
	r.local_instance_id = local_instance_id;
	memcpy(r.mac, sd_frame->peer_mac, ETH_ALEN);

	if (sd_frame->further_nan_p2p.attribute_id == NAN_FURTHER_P2P_ATTR) {
		/* Add P2P FA information */
		r.is_fa_p2p = TRUE;
		r.fa_dev_role = sd_frame->further_nan_p2p.device_role;
		r.fa_chan = sd_frame->fa.e.chan_num;
		memcpy(r.fa_mac, sd_frame->further_nan_p2p.mac, ETH_ALEN);
	}

	memcpy(event->val, &r, sizeof(struct discover_result));
	mwu_hexdump(MSG_INFO, "event->val", (u8 *)event->val, event->len);
	MODULE_DO_CALLBACK(nan_mod, event);
	FREE(event);

	return NAN_ERR_SUCCESS;
}

void nan_send_ndp_event(struct mwu_iface_info *cur_if, u8 event_id,
			u8 *event_buffer, unsigned int buffer_len)
{
	struct event *event = NULL;
	event = malloc(sizeof(struct event) + buffer_len);
	if (!event) {
		ERR("No memory.  Can't send discovery result event.\n");
		return;
	}
	memset(event, 0, sizeof(struct event) + buffer_len);
	event->type = event_id;
	event->status = NAN_ERR_SUCCESS;
	strncpy(event->iface, cur_if->ifname, IFNAMSIZ);

	event->len = buffer_len;

	memcpy(event->val, event_buffer, buffer_len);
	mwu_hexdump(MSG_INFO, "event->val", (u8 *)event->val, event->len);
	MODULE_DO_CALLBACK(nan_mod, event);
	FREE(event);
}

enum nan_error nan_set_p2p_fa2(struct mwu_iface_info *cur_if)
{
	struct nan_params_p2p_fa *p2p_fa;
	struct nan_params_fa fap;
	int ret = NAN_ERR_SUCCESS;

	p2p_fa = &cur_if->pnan_info->fa_p2p_attr;

	/* fill up nan_params values */
	ERR("e.ctrl is %d", p2p_fa->ctrl);
	fap.interval = 16 * (1 << p2p_fa->ctrl);
	fap.repeat_entry = 0; /* valid only for current DW */
	fap.op_class = p2p_fa->op_class;
	fap.op_chan = p2p_fa->op_chan;
	fap.availability_map = p2p_fa->availability_map;
	INFO("Connfiguring FW for further availability parameters");
	ret = nancmd_set_fa(cur_if, &fap);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to set FA parameters in FW, err = %d", ret);
	}

	return ret;
}

/* @TODO: Implement common API for setting further availability across other
 * modules like wifidirect/ap-sta etc. */
enum nan_error nan_set_p2p_fa(struct module *mod,
			      struct nan_params_p2p_fa *p2p_fa)
{
	int ret = NAN_ERR_SUCCESS;
	// struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod || !p2p_fa)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	/* copy fa attr */
	memcpy(&cur_if->pnan_info->fa_p2p_attr, p2p_fa,
	       sizeof(struct nan_params_p2p_fa));

	ret = nan_set_p2p_fa2(cur_if);
	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to set p2p FA map in FW");
	}

	return ret;
}
enum nan_error nan_set_user_pmk(struct module *mod,
				struct nan_set_pmk_attr *nan_pmk_set)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod || !nan_pmk_set)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	/* copy pmk attr */

	/*
	    if (strlen(nan_pmk_set->pmk) > PMK_LEN)
		    cur_if->pnan_info->nan_security.pmk_len = PMK_LEN;
	    else
		    cur_if->pnan_info->nan_security.pmk_len =
	strlen(nan_pmk_set->pmk);


	memcpy(cur_if->pnan_info->nan_security.pmk , nan_pmk_set->pmk,
	cur_if->pnan_info->nan_security.pmk_len);
       */

	u8 default_pmk[32] = {0xF0, 0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x4e,
			      0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e,
			      0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x4e, 0x41,
			      0x4e, 0x4e, 0x41, 0x4e, 0x4e, 0x41, 0x4e, 0x0F};

	cur_if->pnan_info->nan_security.pmk_len = PMK_LEN;
	memcpy(cur_if->pnan_info->nan_security.pmk, default_pmk,
	       cur_if->pnan_info->nan_security.pmk_len);

	INFO("PMK set by user is : **%s**",
	     cur_if->pnan_info->nan_security.pmk);

	return ret;
}

enum nan_error nan_set_ndl_schedule(struct module *mod,
				    peer_availability_info *avail_info,
				    u8 clean)
{
	int ret = NAN_ERR_SUCCESS, i = 0;
	static u8 sched_set = 0;
	// struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (clean)
		sched_set = 0;

	if (!mod || !avail_info)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	/*Clear default entries, for the first time.Subsequent calls will have
	 * sched_set flag true*/
	if (!sched_set)
		cur_if->pnan_info->self_avail_info.conditional_valid = 0;

	/* Put the NDL schedule in the self conditional entry */
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (!(cur_if->pnan_info->self_avail_info.conditional_valid &
		      (1 << i))) {
			memcpy(&cur_if->pnan_info->self_avail_info
					.entry_conditional[i],
			       &avail_info->entry_conditional,
			       sizeof(avail_entry_t));
			cur_if->pnan_info->self_avail_info.conditional_valid |=
				(1 << i);
			sched_set = TRUE;
			ERR("NAN2: Conditional entry#%u populated", i);
			break;
		}
	}
	return ret;
}
enum nan_error nan_ndp_sec_test(struct module *mod, u8 wrong_mic, u8 m4_reject)
{
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return NAN_ERR_INVAL;

	nan_mod = mod;
	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	cur_if->pnan_info->nan_security.wrong_mic_test = wrong_mic;
	cur_if->pnan_info->nan_security.m4_reject_test = m4_reject;
	INFO("NAN2: Test options set : wrong mic %d m4 reject %d",
	     cur_if->pnan_info->nan_security.wrong_mic_test,
	     cur_if->pnan_info->nan_security.m4_reject_test);

	return ret;
}
enum nan_error nan_set_availability(struct module *mod,
				    peer_availability_info *avail_info)
{
	int ret = NAN_ERR_SUCCESS, i = 0;
	static u8 sched_set = 0;
	// struct event *ui_cmd;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod || !avail_info)
		return NAN_ERR_INVAL;

	nan_mod = mod;

	ERR("Module id is: %d\n\n", NAN_ID);
	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	/*Clear default entries, for the first time.Subsequent calls will have
	 * sched_set flag true*/
	if (!sched_set)
		cur_if->pnan_info->self_avail_info.potential_valid = 0;

	/* Populate the self potential entry */
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (!(cur_if->pnan_info->self_avail_info.potential_valid &
		      (1 << i))) {
			memcpy(&cur_if->pnan_info->self_avail_info
					.entry_potential[i],
			       &avail_info->entry_potential,
			       sizeof(avail_entry_t));
			cur_if->pnan_info->self_avail_info.band_entry_potential =
				FALSE;
			cur_if->pnan_info->self_avail_info.potential_valid |=
				(1 << i);
			sched_set = TRUE;
			ERR("NAN2: Potential entry#%u populated", i);
			break;
		}
	}

	return ret;
}

enum nan_error nan_send_ftm_report(struct module *mod, u32 distance,
				   char *mac_addr)
{
	int ret = NAN_ERR_SUCCESS;
	// construct FTM report and send the distance
	struct mwu_iface_info *cur_if = NULL;

	cur_if = mwu_get_interface(mod->iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return NAN_ERR_INVAL;
	}

	nan_tx_ranging_result_frame(cur_if, RANGING_REPORT,
				    NAN_REASON_CODE_UNSPECIFIED, distance,
				    (u8 *)mac_addr);

	return ret;
}
/*Open nan interface*/
void nan_iface_open(void)
{
	char cmd[100] = {0};
	sprintf(cmd, "ifconfig nan0 up 2>/dev/null");
	if (system(cmd) != NAN_ERR_SUCCESS)
		ERR("Error opening the interface");
}
/*close nan interface*/
void nan_iface_close(void)
{
	char cmd[100] = {0};
	sprintf(cmd, "ifconfig nan0 down 2>/dev/null");
	if (system(cmd) != NAN_ERR_SUCCESS)
		ERR("Error closing the interface");
}

int nan_is_nan_ranging_responder(void)
{
	struct mwu_iface_info *cur_if = NULL;
	char iface[IFNAMSIZ + 1];
	strncpy(iface, "nan0", IFNAMSIZ);
	cur_if = mwu_get_interface(iface, NAN_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", iface);
		return FALSE;
	}

	return cur_if->pnan_info->is_nan_ranging_responder;
}
