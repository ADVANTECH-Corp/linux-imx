#include "mlocation_api.h"
#include "mlocation_lib.h"
#include "mwu_internal.h"
#include "mwu_log.h"
#include "wps_wlan.h"

#include <unistd.h>
#define BUF_SIZE 200
#define MAX_RANGE_REPORT 5

#define DOT11MC_FTM 0
#define DOT11AZ_NTB_RANGING 2

struct module *mlocation_mod = NULL;

ftm_range_request range_req[MAX_RANGE_REPORT];
u8 range_req_count;
u8 range_req_index;
u8 ftm_range_request_in_progress;
extern u8 fqdn_support;

enum mlocation_ui_cmd_type {
	MLOCATION_UI_CMD_INIT,
	MLOCATION_UI_CMD_DEINIT,
	MLOCATION_UI_CMD_PDREQ,
};

static void process_anqp_comeback(struct mwu_iface_info *cur_if, u16 delay,
				  u8 *mac);
static enum mlocation_error send_anqp_req(struct mwu_iface_info *cur_if,
					  mlocation_anqp_cfg *req);
static enum mlocation_error send_action_frame(struct mwu_iface_info *cur_info,
					      u8 *frame_buf,
					      u16 frame_buf_size);
enum mlocation_error mlocation_response_frame(struct mwu_iface_info *cur_info,
					      u8 *mac, int dist);

u8 radio_req_bitmap;
u8 range_request_tokenid;
u8 dest_bssid[6];

typedef struct _lci_data {
	location_request_element ele;
	ftm_lci_request lci_req;
	u8 data[40];
} __ATTRIB_PACK__ lci_data;

lci_data lcidata;

void mlocation_deinit(struct module *mod, struct mwu_module *mwu_mod)
{
	struct mwu_iface_info *cur_if = NULL;
	char ifname[IFNAMSIZ];
	memset(ifname, 0, IFNAMSIZ);
	mlocation_session_ctrl mlocation_ctrl;

	memset(&mlocation_ctrl, 0, sizeof(mlocation_session_ctrl));

	if (!mod)
		return;

	mlocation_mod = mod;

	cur_if = mwu_get_interface(mod->iface, MLOCATION_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return;
	}
	strncpy(ifname, cur_if->ifname, IFNAMSIZ);

	mlocation_ctrl.action = 2;

	/* Abort all ongoing FTM Requests */
	INFO("INSIDE DEINIT : sending mlocation session ctrl\n");
	cmd_mlocation_session_ctrl(cur_if, &mlocation_ctrl);

	if (mwu_iface_deinit(cur_if->ifname, MLOCATION_ID) != MWU_ERR_SUCCESS)
		ERR("Failed to de-initialize interface %s.\n", ifname);
}

enum mlocation_error
mlocation_init_config(struct mwu_iface_info *cur_if,
		      struct mlocation_cfg *mlocation_config)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	mlocation_cmd_session_config *config = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	mlocation_init_tlv *init_tlv = NULL;
	mlocation_location_civic_tlv *civic_tlv = NULL;
	u8 civic_address_length = 0;
	mlocation_session_cfg_lci_tlv *lci_tlv = NULL;
	int civic_len = 0, total_tlv_size = 0;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(MLOCATION_HOST_CMD);
	int init_tlv_len = 0;
	// printf("\n%s:%d\n", __func__, __LINE__);
	mlocation_Ranging_NTB_Params_tlv *init_ntb_tlv = NULL;

	if (mlocation_config->protocol_type == DOT11AZ_NTB_RANGING) {
		total_tlv_size = sizeof(mlocation_Ranging_NTB_Params_tlv);
	} else if (mlocation_config->protocol_type == DOT11MC_FTM) {
		total_tlv_size = sizeof(mlocation_init_tlv);
	}
	INFO("total_tlv_size = %d", total_tlv_size);

	if (mlocation_config->lci_request)
		total_tlv_size += sizeof(mlocation_session_cfg_lci_tlv);

	if (mlocation_config->civic_location) {
		civic_address_length = strlen(mlocation_config->ca_value);
		total_tlv_size += sizeof(mlocation_location_civic_tlv) +
				  civic_address_length;
		civic_len = sizeof(mlocation_location_civic_tlv) +
			    civic_address_length;
	}

	INFO("total_tlv_size = %d", total_tlv_size);
	mrvl_cmd = mlocation_cmdbuf_alloc(
		sizeof(mlocation_cmd_session_config) + total_tlv_size,
		MLOCATION_HOST_CMD, HostCmd_MLOCATION_SESSION_CONFIG);

	if (!mrvl_cmd) {
		return MLOCATION_ERR_NOMEM;
	}
	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	config = (mlocation_cmd_session_config *)cmd->cmd_data;
	config->action = wlan_cpu_to_le16(ACTION_SET);

	if (mlocation_config->protocol_type == DOT11AZ_NTB_RANGING) {
		init_ntb_tlv =
			(mlocation_Ranging_NTB_Params_tlv *)(config->tlvs);
		init_ntb_tlv->tag = MLOCATION_NTB_RANGING_TLV_ID;
		init_ntb_tlv->len = sizeof(mlocation_Ranging_NTB_Params_tlv) -
				    4; /*   4 = size of cmd header */
		mlocation_config->civic_location = 0;
		mlocation_config->lci_request = 0;
		init_ntb_tlv->format_bw = mlocation_config->bw;
		init_ntb_tlv->max_i2r_sts_upto80 =
			mlocation_config->max_i2r_sts_upto80;
		init_ntb_tlv->max_r2i_sts_upto80 =
			mlocation_config->max_r2i_sts_upto80;
		init_ntb_tlv->az_measurement_freq =
			mlocation_config->az_measurement_freq;
		init_ntb_tlv->az_number_of_measurements =
			mlocation_config->az_number_of_measurements;
		init_ntb_tlv->civic_req = mlocation_config->civic_location;
		init_ntb_tlv->lci_req = mlocation_config->lci_request;
		init_tlv_len = sizeof(mlocation_Ranging_NTB_Params_tlv);
		mwu_hexdump(MSG_ERROR, "mlocation_init_config", (u8 *)config,
			    sizeof(mlocation_cmd_session_config) +
				    sizeof(mlocation_Ranging_NTB_Params_tlv));
	} else if (mlocation_config->protocol_type == DOT11MC_FTM) {
		init_tlv = (mlocation_init_tlv *)(config->tlvs);
		init_tlv->tag = MLOCATION_INIT_TLV_ID;
		if (nan_is_nan_ranging_responder()) {
			init_tlv->tag = MLOCATION_RESP_TLV_ID;
		}
		init_tlv->len = sizeof(mlocation_init_tlv) - 4; /* 4 = size of
								   cmd header */
		init_tlv->burst_exponent = mlocation_config->burst_exp;
		init_tlv->burst_duration = mlocation_config->burst_duration;
		init_tlv->min_delta_mlocation = mlocation_config->min_delta;
		init_tlv->asap = mlocation_config->asap;
		init_tlv->mlocation_per_burst =
			mlocation_config->mlocation_per_burst;
		init_tlv->chan_spacing = mlocation_config->bw;
		init_tlv->burst_period = mlocation_config->burst_period;
		init_tlv->civic_req = mlocation_config->civic_location;
		init_tlv->lci_req = mlocation_config->lci_request;
		init_tlv_len = sizeof(mlocation_init_tlv);
		mwu_hexdump(MSG_ERROR, "mlocation_init_config", (u8 *)config,
			    sizeof(mlocation_cmd_session_config) +
				    sizeof(mlocation_init_tlv));
	}

	if (mlocation_config->civic_location) {
		civic_tlv = (mlocation_location_civic_tlv *)(config->tlvs +
							     init_tlv_len);
		civic_tlv->tag = MLOCATION_LOCATION_CIVIC_TLV_ID;
		civic_tlv->len = sizeof(mlocation_location_civic_tlv) -
				 4; /* 4 = size of cmd header */
		civic_tlv->civic_location_type =
			mlocation_config->civic_location_type;
		civic_tlv->country_code = mlocation_config->country_code;
		civic_tlv->civic_address_type = mlocation_config->ca_type;
		mlocation_config->ca_length =
			strlen(mlocation_config->ca_value);
		civic_tlv->civic_address_length =
			strlen(mlocation_config->ca_value);

		memcpy(civic_tlv->civic_address, mlocation_config->ca_value,
		       civic_tlv->civic_address_length);
		civic_tlv->len += mlocation_config->ca_length;
	}

	if (mlocation_config->lci_request) {
		lci_tlv = (mlocation_session_cfg_lci_tlv *)(config->tlvs +
							    init_tlv_len +
							    civic_len);
		lci_tlv->tag = MLOCATION_LOCATION_CFG_LCI_TLV_ID;
		lci_tlv->len = sizeof(mlocation_session_cfg_lci_tlv) - 4;
		lci_tlv->longitude = mlocation_config->longitude;
		lci_tlv->latitude = mlocation_config->latitude;
		lci_tlv->altitude = mlocation_config->altitude;
		lci_tlv->lat_unc = mlocation_config->lat_unc;
		lci_tlv->long_unc = mlocation_config->long_unc;
		lci_tlv->alt_unc = mlocation_config->alt_unc;
	}

	ret = mlocation_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);
	return ret;
}

enum mlocation_error mlocation_init(struct module *mod,
				    struct mlocation_cfg *cfg)
{
	int ret = MLOCATION_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;

	if (!mod)
		return MLOCATION_ERR_INVAL;

	mlocation_mod = mod;

	INFO("Module id is: %d\n\n", MLOCATION_ID);
	cur_if = mwu_iface_init(mod->iface, MLOCATION_ID);

	if (!cur_if) {
		ERR("Failed to allocate memory for new interface.");
		return MLOCATION_ERR_NOMEM;
	}

	ret = mlocation_init_config(cur_if, cfg);

	return ret;
}

enum mlocation_error mlocation_send_neighbor_req(struct mwu_iface_info *cur_if,
						 mlocation_neighbor_req *req)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	mlocation_neighbor_req *ctrl;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(MLOCATION_HOST_CMD);

	mrvl_cmd = mlocation_cmdbuf_alloc(sizeof(mlocation_neighbor_req),
					  MLOCATION_HOST_CMD,
					  HostCmd_MLOCATION_NEIGHBOR_REQ);
	if (!mrvl_cmd)
		return MLOCATION_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	ctrl = (mlocation_neighbor_req *)cmd->cmd_data;
	ctrl->action = wlan_cpu_to_le16(req->action);
	ctrl->dialog_token = req->dialog_token;
	ctrl->lci_req = req->lci_req;
	ctrl->loc_civic_req = req->loc_civic_req;

	mwu_hexdump(MSG_ERROR, "mlocation_neighbor_req", (u8 *)ctrl,
		    sizeof(mlocation_neighbor_req));

	ret = mlocation_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);

	return ret;
}

static u32 ext_cap_status(u8 *ext_cap, u8 bit_pos)
{
	int index, bit_mask = 0;

	if (bit_pos < (MAX_EXTENDED_CAP * 8)) {
		index = bit_pos / 8;
		bit_mask = 1 << (bit_pos % 8);

		if (bit_mask & ext_cap[index])
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

static void process_anqp_scan(struct mwu_iface_info *cur_if,
			      struct SCAN_RESULTS *result)
{
	mlocation_anqp_cfg cfg;
	u32 civic_cap, lci_cap;

	memcpy(&cfg, &cur_if->pmlocation_info->anqp_cfg, sizeof(cfg));

	civic_cap = lci_cap = 0;

	civic_cap = ext_cap_status(result->extended_cap, EXT_CAP_CIVIC);
	lci_cap = ext_cap_status(result->extended_cap, EXT_CAP_LCI);

	cfg.civic &= civic_cap;
	cfg.lci &= lci_cap;
	cfg.fqdn = fqdn_support;
	INFO("fqdn = %d", fqdn_support);
	cfg.channel = mapping_freq_to_chan(result->freq);

	send_anqp_req(cur_if, &cfg);
	cur_if->pmlocation_info->anqp_in_progress = FALSE;
}

void mlocation_process_scan(struct mwu_iface_info *cur_if)
{
	struct SCAN_RESULTS *results = NULL;
	int count = 0, i = 0;
	int found = 0;

	cur_if->pmlocation_info->internal_scan = FALSE;

	results = malloc(SCAN_AP_LIMIT * sizeof(struct SCAN_RESULTS));

	if (results == NULL) {
		mwu_printf(MSG_ERROR, "Failed to allocate memory for scan "
				      "results");
		return;
	}

	count = wlan_get_scan_results(cur_if, results);

	mwu_printf(MSG_INFO, "Peers found: %d", count);

	if (cur_if->pmlocation_info->anqp_in_progress) {
		/* Lookup the bssid */
		for (i = 0; i < count; i++) {
			if (memcmp(results[i].bssid,
				   cur_if->pmlocation_info->anqp_cfg.mac,
				   ETH_ALEN) == 0) {
				found = 1;
				break;
			}
		}

		if (found) {
			INFO("Found AP at index : %d", i);
			process_anqp_scan(cur_if, &results[i]);
		} else {
			INFO("AP not found in the scan");
			cur_if->pmlocation_info->anqp_in_progress = FALSE;
		}
	}
}

enum mlocation_error mlocation_send_anqp_req(struct mwu_iface_info *cur_if,
					     mlocation_anqp_cfg *req)
{
	u8 ret = MLOCATION_ERR_SUCCESS;

	if (cur_if->pmlocation_info->anqp_in_progress) {
		ERR("ANQP in progress...");
		return MLOCATION_ERR_BUSY;
	}

	cur_if->pmlocation_info->anqp_in_progress = TRUE;

	memcpy(&cur_if->pmlocation_info->anqp_cfg, req,
	       sizeof(mlocation_anqp_cfg));

	/* Trigger Scan to locate the AP */
	INFO("Scanning...");
	cur_if->pmlocation_info->internal_scan = TRUE;
	ret = wps_wlan_scan(cur_if);

	return ret;
}

static enum mlocation_error send_anqp_req(struct mwu_iface_info *cur_if,
					  mlocation_anqp_cfg *req)
{
	location_anqp_frame *anqp_req;
	u8 buf[BUF_SIZE];
	u8 *pos;
	u16 *query_len;
	u16 *query_req_len;
	u16 *pos16, total_len;

	memset(buf, 0, sizeof(buf));
	total_len = 0;

	anqp_req = (location_anqp_frame *)buf;

	memcpy(anqp_req->mac, req->mac, ETH_ALEN);

	anqp_req->category = WIFI_CATEGORY_PUBLIC_ACTION_FRAME;
	anqp_req->action = GAS_INITIAL_REQUEST;
	anqp_req->dialog_token = 1;
	anqp_req->channel = req->channel;

	total_len += sizeof(location_anqp_frame);

	pos = anqp_req->ie_list;

	/* Tag */
	*pos = ADVERTISEMENT_PROTO;
	pos++;

	/* Length */
	*pos = 2;
	pos++;

	/* Value: 2 bytes of Advertisement protocol element: ANQP (0) */
	pos += 2;

	total_len += 4; /* Sizeof of Advertisement TLV */

	if (req->civic || req->lci || req->fqdn) {
		INFO("inside if");
		/* Prepare query list */
		query_req_len = (u16 *)pos;
		pos += 2;

		/* ANQP query list */
		pos16 = (u16 *)pos;
		*pos16 = 0x100;

		pos16++;
		*query_req_len += 2;

		query_len = pos16;

		pos16++;
		*query_req_len += 2;

		if (req->civic) {
			*pos16 = 0x10a;
			*query_len += 2;
			*query_req_len += 2;
			pos16++;
		}

		if (req->lci) {
			*pos16 = 0x109;
			*query_len += 2;
			*query_req_len += 2;
			pos16++;
		}

		if (req->fqdn) {
			INFO("inside fqdn");
			*pos16 = 0x10b;
			*query_len += 2;
			*query_req_len += 2;
			pos16++;
		}
	}

	if (*query_len) {
		/* Send ANQP only when at least one of the measurement type is
		 * supported */
		total_len += *query_req_len + sizeof(*query_req_len);

		mwu_hexdump(MSG_INFO, "Send ANQP Request", buf, total_len);
		send_action_frame(cur_if, buf, total_len);
	} else {
		INFO("no ANQP");
		return MLOCATION_ERR_INVAL;
	}
	return MLOCATION_ERR_SUCCESS;
}

enum mlocation_error
cmd_mlocation_session_ctrl(struct mwu_iface_info *cur_if,
			   mlocation_session_ctrl *mlocation_ctrl)
{
	int ret = 0;
	mrvl_cmd_head_buf *cmd = NULL;
	mlocation_session_ctrl *ctrl = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(MLOCATION_HOST_CMD);
	mrvl_cmd = mlocation_cmdbuf_alloc(sizeof(mlocation_session_ctrl),
					  MLOCATION_HOST_CMD,
					  HostCmd_MLOCATION_SESSION_CTRL);
	if (!mrvl_cmd)
		return MLOCATION_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	ctrl = (mlocation_session_ctrl *)cmd->cmd_data;
	ctrl->action = wlan_cpu_to_le16(mlocation_ctrl->action);
	ctrl->channel = mlocation_ctrl->channel;
	ctrl->ftm_for_nan_ranging = mlocation_ctrl->ftm_for_nan_ranging;
	memcpy(ctrl->peer_mac, mlocation_ctrl->peer_mac, ETH_ALEN);

	mwu_hexdump(MSG_ERROR, "mlocation_session_ctrl", (u8 *)ctrl,
		    sizeof(mlocation_session_ctrl));
	ret = mlocation_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	return ret;
}

enum mlocation_error do_mlocation_session_ctrl(struct module *mod,
					       struct mlocation_session *ctrl,
					       int nan_ranging)
{
	int ret = MLOCATION_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = NULL;
	mlocation_session_ctrl session;

	if (!mod)
		return MLOCATION_ERR_INVAL;

	mlocation_mod = mod;

	ERR("Module id is: %d\n\n", MLOCATION_ID);
	cur_if = mwu_get_interface(mod->iface, MLOCATION_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", mod->iface);
		return MLOCATION_ERR_INVAL;
	}

	session.action = ctrl->action;
	session.ftm_for_nan_ranging = nan_ranging;
	session.channel = ctrl->channel;
	memcpy(&session.peer_mac, ctrl->mac, ETH_ALEN);

	ret = cmd_mlocation_session_ctrl(cur_if, &session);

	if (ret != MLOCATION_ERR_SUCCESS) {
		ERR("MLOCATION cmd_mlocation_session_ctrl Failed");
	} else {
		INFO("MLOCATION cmd_mlocation_session_ctrl successful");
	}

	return ret;
}

static void process_radio_request(struct mwu_iface_info *cur_if, u8 *evt_buffer)
{
	mlocation_generic_event *radio_req;
	u8 *pos, *old_pos;
	u8 i = 0;
	u8 ap_count = 0;
	u8 total_buf_len = 0, process_len = 0;

	ENTER();
	memset(&range_req, 0, sizeof(range_req));
	range_req_count = 0;
	range_req_index = 0;

	radio_req = (mlocation_generic_event *)evt_buffer;

	old_pos = NULL;
	pos = (u8 *)&radio_req->u.rm_evt.buffer;
	total_buf_len = radio_req->u.rm_evt.len;
	process_len = 0;

	radio_req_bitmap = 0;
	memcpy(dest_bssid, &radio_req->u.rm_evt.bssid[0], 6);

	while (process_len < total_buf_len) {
		if (*pos == MEASUREMENT_REQUEST) {
			pos++;

			pos++;
			range_request_tokenid = *pos;
			pos += 2;

			process_len += 4;

			switch (*pos) {
				INFO("FTM req: %d", *pos);
			/*            if (*pos == FTM_RANGE_REQUEST) */
			case FTM_RANGE_REQUEST: {
				INFO("RANGE REQUEST");
				old_pos = pos;
				pos += 3;
				radio_req_bitmap |=
					RADIO_MEASUREMENT_RANGE_REQUEST_SET;

				INFO("ap_count: %d", *pos);
				ap_count = *pos;
				pos++;

				for (i = 0; i < ap_count; i++) {
					/* extract bssid, channel and bw for
					 * each range request */
					pos += 2;

					/* BSSID */
					memcpy(range_req[i].bssid, pos, 6);
					pos += 6;
					pos += 4;
					pos += 1;

					range_req[i].channel = *pos;
					pos += 4;
					range_req[i].bandwidth = *pos;

					pos += 3;

					INFO("Range Request %d - " UTIL_MACSTR
					     " - %d - %d\n",
					     i + 1,
					     UTIL_MAC2STR(range_req[i].bssid),
					     range_req[i].channel,
					     range_req[i].bandwidth);
				}
			} break;

			case LCI_REQUEST_SUBELEMENT_ID: {
				INFO("LCI REQUEST");
				lci_data *req_lci;
				req_lci = (lci_data *)(pos - 4);
				radio_req_bitmap |=
					RADIO_MEASUREMENT_LCI_REQUEST_SET;
				memcpy(&lcidata, req_lci,
				       (sizeof(location_request_element) +
					sizeof(ftm_lci_request) +
					req_lci->lci_req.length));
			} break;

			case CIVIC_REQUEST_SUBELEMENT_ID: {
				INFO("CIVIC REQUEST");
				radio_req_bitmap |=
					RADIO_MEASUREMENT_LCI_REQUEST_SET;
			} break;
			}
			process_len = process_len + (pos - old_pos);
		}
	}
	range_req_count = ap_count;
	if (range_req_count) {
		mlocation_session_ctrl session;

		/* Start FTM with first index */
		INFO("Start FTM with %d: " UTIL_MACSTR " at %d",
		     range_req_index + 1,
		     UTIL_MAC2STR(range_req[range_req_index].bssid),
		     range_req[range_req_index].channel);

		session.action = 1;
		session.ftm_for_nan_ranging = 0;
		memcpy(session.peer_mac, range_req[range_req_index].bssid,
		       ETH_ALEN);
		session.channel = range_req[range_req_index].channel;
		cmd_mlocation_session_ctrl(cur_if, &session);

		ftm_range_request_in_progress = TRUE;
	}

	LEAVE();
}

static void mlocation_event_parser(struct mwu_iface_info *cur_if,
				   u8 *evt_buffer, int evt_len)
{
	ENTER();
	struct event *event = NULL;
	mlocation_generic_event *ml_evt;
	/* event parser */
	event = malloc(sizeof(struct event) + sizeof(mlocation_generic_event));
	if (!event) {
		ERR("No memory.  Can't send registrar done event.\n");
		return;
	}
	memset(event, 0,
	       sizeof(struct event) + sizeof(mlocation_generic_event));
	ml_evt = (mlocation_generic_event *)evt_buffer;

	INFO("event subtype = %d", ml_evt->event_subtype);
	switch (ml_evt->event_subtype) {
	case MLOCATION_EVENT_SESSION_COMPLETE:

		if (ftm_range_request_in_progress) {
			mlocation_event *dev = (mlocation_event *)&ml_evt->u;

			free(event);

			/* Update measurement start time */
			range_req[range_req_index].start_time =
				dev->meas_start_time;
			mwu_hexdump(
				MSG_ERROR, "Start Time: ",
				(u8 *)&range_req[range_req_index].start_time,
				4);

			range_req_index++;

			sleep(5);

			if (range_req_index < range_req_count) {
				mlocation_session_ctrl session;

				/* Start FTM with next index */
				INFO("Start FTM with %d: " UTIL_MACSTR " at %d",
				     range_req_index + 1,
				     UTIL_MAC2STR(
					     range_req[range_req_index].bssid),
				     range_req[range_req_index].channel);

				session.action = 1;
				session.ftm_for_nan_ranging = 0;
				memcpy(session.peer_mac,
				       range_req[range_req_index].bssid,
				       ETH_ALEN);
				session.channel =
					range_req[range_req_index].channel;
				cmd_mlocation_session_ctrl(cur_if, &session);
			} else {
				INFO("Range request complete!!");
				mlocation_response_frame(cur_if,
							 range_req[0].bssid, 0);
				ftm_range_request_in_progress = FALSE;
				range_req_index = 0;
				range_req_count = 0;
			}

			LEAVE();
			return;

		} else {
			event->type = MLOCATION_EVENT_SESSION_COMPLETE;
			event->len = sizeof(mlocation_event);
			strncpy(event->iface, cur_if->ifname, IFNAMSIZ);
		}
		break;

	case MLOCATION_RADIO_REQUEST_RECEIVED:
		free(event);
		radio_req_bitmap = 0;
		process_radio_request(cur_if, evt_buffer);

		if (!(radio_req_bitmap & 0x1)) {
			mlocation_response_frame(cur_if, dest_bssid, 0);
		}

		LEAVE();
		return;
#if 0
            event->type = MLOCATION_RADIO_REQUEST_RECEIVED;
            event->len = sizeof(mlocation_radio_receive_event);
            strncpy(event->iface, "mlan0", IFNAMSIZ);
#endif
		break;

	case MLOCATION_RADIO_REPORT_RECEIVED:
		event->type = MLOCATION_RADIO_REPORT_RECEIVED;
		event->len = sizeof(struct mlocation_radio_report_event);
		strncpy(event->iface, cur_if->ifname, IFNAMSIZ);
		break;

	case MLOCATION_ANQP_RESP_RECEIVED: {
		mlocation_generic_event *radio_req;
		gas_frames *gas_resp;
		radio_req = (mlocation_generic_event *)evt_buffer;
		gas_resp = (gas_frames *)&radio_req->u.anqp_rsp.buffer[0];

		switch (gas_resp->hdr.action) {
		case GAS_INITIAL_RESPONSE:
			if (gas_resp->init_resp.GasComebackDelay) {
				process_anqp_comeback(
					cur_if,
					gas_resp->init_resp.GasComebackDelay,
					radio_req->u.anqp_rsp.mac);
			}
			break;
		case GAS_COMEBACK_RESPONSE:
			if (gas_resp->cbk_resp.GasComebackDelay) {
				process_anqp_comeback(
					cur_if,
					gas_resp->cbk_resp.GasComebackDelay,
					radio_req->u.anqp_rsp.mac);
			}
			break;
		}
		break;
	}
	default:
		ERR("Undefined Event Sybtype : %d\n", ml_evt->event_subtype);
	}
	memcpy(event->val, &ml_evt->u, evt_len);
	MODULE_DO_CALLBACK(mlocation_mod, event);
	FREE(event);
	LEAVE();
}

/* recevive and event from the driver and send it to the state machine */
void mlocation_driver_event(char *ifname, u8 *buffer, u16 size)
{
	struct mwu_iface_info *cur_if = NULL;

	ENTER();

	if (!ifname || !buffer) {
		ERR("Invalid input.");
		return;
	}

	cur_if = mwu_get_interface(ifname, MLOCATION_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", ifname);
		return;
	}

	mlocation_event_parser(cur_if, buffer, size);
	LEAVE();
}

static enum mlocation_error send_action_frame(struct mwu_iface_info *cur_info,
					      u8 *frame_buf, u16 frame_buf_size)
{
	enum mlocation_error ret;
	mrvl_cmd_head_buf *cmd;

	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(MLOCATION_ACTION_FRAME);

	ENTER();
	mrvl_cmd =
		mlocation_cmdbuf_alloc(frame_buf_size, MLOCATION_ACTION_FRAME,
				       HostCmd_CMD_802_11_ACTION_FRAME);

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	memcpy(cmd->cmd_data, frame_buf, frame_buf_size);

	ret = mlocation_cmdbuf_send(cur_info, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);
	LEAVE();
	return ret;
}

enum mlocation_error mlocation_request_frame(struct mwu_iface_info *cur_info,
					     unsigned char *mac)
{
	u8 ret = MLOCATION_ERR_SUCCESS;
	u8 buf[BUF_SIZE];
	location_request_frame *action_buf;
	location_request_element *req_element;
	int i;
	ftm_range_request_element *range_req;

	action_buf = (location_request_frame *)buf;
	for (i = 0; i < 6; i++)
		action_buf->mac[i] = mac[i];

	action_buf->category = RADIO_MEASUREMENT_CATEGORY_PUBLIC_ACTION_FRAME;
	action_buf->action = RADIO_MEASUREMENT_REQUEST;
	action_buf->dialog_token = 1;
	action_buf->repeatations = 0;

	req_element = (location_request_element *)&action_buf->ie_list[0];
	req_element->element_id = MEASUREMENT_REQUEST_ELEMENT_ID;
	req_element->length = sizeof(location_request_element) +
			      sizeof(ftm_range_request_element) -
			      FTM_TLV_HEADER_LENGTH;
	req_element->token = 1;
	req_element->request_mode.parallel = 1;
	req_element->request_mode.enable = 1;
	req_element->request_mode.request = 1;
	req_element->request_mode.report = 0;
	req_element->type = FTM_RANGE_REQUEST;
	range_req = (ftm_range_request_element *)req_element->request;
	range_req->random_interval = 1;
	range_req->min_ap_count = 1;
	ret = send_action_frame(cur_info, buf,
				sizeof(location_request_element) +
					sizeof(location_request_frame) +
					sizeof(ftm_range_request_element));
	return ret;
}

#if 0
enum mlocation_error mlocation_neighbor_report_request_frame(struct mwu_iface_info *cur_info, unsigned char* mac)
{
    u8 ret = MLOCATION_ERR_SUCCESS;
    u8 buf[BUF_SIZE];
    int i;
    neighbor_request_frame *action_buf;

    action_buf = (neighbor_request_frame *)buf;
    for(i=0;i<6;i++)
        action_buf->mac[i]=mac[i];
    action_buf->category = RADIO_MEASUREMENT_CATEGORY_PUBLIC_ACTION_FRAME;
    action_buf->action = NEIGHBOR_REPORT_REQUEST;
    action_buf->dialog_token = 1;

    INFO("send_frame_size = %d\n",sizeof(neighbor_request_frame));
    ret = send_action_frame(cur_info,buf, sizeof(neighbor_request_frame));
    return ret;
}
#endif

enum mlocation_error mlocation_response_frame(struct mwu_iface_info *cur_info,
					      u8 *mac, int dist)
{
	u8 ret = MLOCATION_ERR_SUCCESS;
	u8 buf[BUF_SIZE];
	location_response_frame *action_buf;
	location_response_element *res_element;
	int i, size = 0;
	u8 *pos;
	range_entry_feild *range_entry;
	u8 count = 0;
	ftm_lci_report *lci_report;
	ftm_lci_info *lci_info;
	ftm_lci_request *lci_req;

	action_buf = (location_response_frame *)buf;

	INFO("mac = ");
	for (i = 0; i < 6; i++) {
		action_buf->mac[i] = mac[i];
		INFO("%u", mac[i]);
	}
	INFO("\n");
	action_buf->category = RADIO_MEASUREMENT_CATEGORY_PUBLIC_ACTION_FRAME;
	action_buf->action = RADIO_MEASUREMENT_REPORT;
	action_buf->dialog_token = 1;
	size += sizeof(location_response_frame);

	if (radio_req_bitmap & RADIO_MEASUREMENT_RANGE_REQUEST_SET) {
		INFO("ADD RANGE RESPONSE ELEMENT");
		res_element =
			(location_response_element *)(&action_buf->ie_list[0] +
						      count);
		res_element->element_id = MEASUREMENT_REPORT_ELEMENT_ID;
		res_element->length = sizeof(location_response_element) -
				      FTM_TLV_HEADER_LENGTH;
		/* should copy the token id from request */
		res_element->token = range_request_tokenid;
		res_element->report_mode.late = 0;
		res_element->report_mode.incapable = 0;
		res_element->report_mode.refused = 0;
		res_element->report_mode.reserved = 0;
		res_element->type = FTM_RANGE_REQUEST;

		size += res_element->length + FTM_TLV_HEADER_LENGTH;
		pos = (u8 *)res_element->report;
		*pos = range_req_count;
		pos++;

		size += sizeof(u8) +
			range_req_count * sizeof(range_entry_feild);

		for (i = 0; i < range_req_count; i++) {
			range_entry = (range_entry_feild *)pos;

			range_entry->measurement_start_time =
				range_req[i].start_time;

			memcpy(range_entry->bssid, range_req[i].bssid, 6);

			range_entry->max_range_exponent = 0;
			INFO("int= %d,%x, aft =%x", dist, dist,
			     cpu_to_le32(dist));
			range_entry->range = dist;

			pos += sizeof(range_entry_feild);
		}
		*pos = 0; /* error entries */
		size += sizeof(u8);
		count = size;
		res_element->length +=
			range_req_count * sizeof(range_entry_feild) + 2;
	}

	if (radio_req_bitmap & RADIO_MEASUREMENT_CIVIC_REQUEST_SET) {
		INFO("ADD RADIO CIVIC ELEMENT");
		res_element =
			(location_response_element *)(&action_buf->ie_list[0] +
						      count);
		res_element->element_id = MEASUREMENT_REPORT_ELEMENT_ID;
		res_element->length = sizeof(location_response_element) -
				      FTM_TLV_HEADER_LENGTH;
		res_element->token = 2;
		res_element->report_mode.late = 0;
		res_element->report_mode.incapable = 0;
		res_element->report_mode.refused = 0;
		res_element->report_mode.reserved = 0;
		res_element->type = CIVIC_REQUEST_SUBELEMENT_ID;
	}

	if (radio_req_bitmap & RADIO_MEASUREMENT_LCI_REQUEST_SET) {
		INFO("ADD RADIO LCI ELEMENT");

		res_element =
			(location_response_element *)(&action_buf->ie_list[0] +
						      count);
		res_element->element_id = MEASUREMENT_REPORT_ELEMENT_ID;
		res_element->length = sizeof(location_response_element) -
				      FTM_TLV_HEADER_LENGTH;
		res_element->token = lcidata.ele.token;
		res_element->report_mode.late = 0;
		res_element->report_mode.incapable = 0;
		res_element->report_mode.refused = 0;
		res_element->report_mode.reserved = 0;
		res_element->type = LCI_REQUEST_SUBELEMENT_ID;

		lci_req = (ftm_lci_request *)&lcidata.ele.request[0];

		if (lci_req->subject == 1 && lci_req->subelement_id == 4) {
			lci_report = (ftm_lci_report *)res_element->report;
			lci_report->element_id = 0; // LCI element id
			lci_info = (ftm_lci_info *)lci_report->lci_feild;
			memset(lci_info, 0, sizeof(ftm_lci_info));
			lci_report->length = sizeof(ftm_lci_info);
			res_element->length +=
				sizeof(ftm_lci_report) + sizeof(ftm_lci_info);
			count += (res_element->length + FTM_TLV_HEADER_LENGTH);
			size += (res_element->length + FTM_TLV_HEADER_LENGTH);
		} else {
			res_element->report_mode.incapable = 1;
			INFO("LCI request incapable");
		}
	}

	ret = send_action_frame(cur_info, buf, size);
	return ret;
}

static void process_anqp_comeback(struct mwu_iface_info *cur_if, u16 delay,
				  u8 *mac)
{
	ENTER();
	location_response_frame *anqp_comeback;
	int size = 0, i = 0;
	u8 buf[BUF_SIZE];
	u32 timer;
	anqp_comeback = (location_response_frame *)buf;
	INFO("delay = %d", swap_byte_16(delay));
	timer = swap_byte_16(delay) * 1000; // swap_byte_16(delay);
	for (i = 0; i < 6; i++)
		INFO("%x", mac[i]);
	memcpy(anqp_comeback->mac, mac, 6);
	anqp_comeback->category = WIFI_CATEGORY_PUBLIC_ACTION_FRAME;
	anqp_comeback->action = GAS_COMEBACK_REQUEST;
	anqp_comeback->dialog_token = 1;
	size += sizeof(location_response_frame);
	INFO("timer = %d", timer - 120000);
	usleep(timer - 120000);
	send_action_frame(cur_if, buf, size);
	LEAVE();
}

void mlocationmod_halt(void)
{
	return;
}
