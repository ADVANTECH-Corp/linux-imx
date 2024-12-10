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

/* discovery_engine.c: implementation of the discovery engine of NAN
 *
 * The core of discovery engine is implemented here. Most of the helper
 * functions also implemented in this file. Pls note that, the dicovery engine
 * still shares the common NAN state machine implemented in nan/nan.c
 */
#include "wps_def.h"
//#include "mwu_def.h"
#include "crypto.h"
#include "nan_lib.h"
#include "wlan_hostcmd.h"
#include "mwu_log.h"
#include "discovery_engine.h"
#include "wps_wlan.h"

#include "data_engine.h"
#include "nan.h"
#include "mlocation.h"
#include "mlocation_lib.h"
#include "mlocation_api.h"

#include "os/include/mwu_if_manager.h"
#include "os/include/wps_os.h"
#include "mwu.h"
#include "nan_lib.h"

int discovery_mac_written = 0;

u32 compute_bloom_filter_crc(u8 index, u8 *mac_address, u8 num_of_mac);
void compute_hash(u16 *hash_arr, u8 *final_hash, int bloom_filter_len);
enum nan_error nan_send_sdf_event(struct mwu_iface_info *cur_if, u8 event_id,
				  struct nan_rx_sd_frame *sd_frame,
				  u8 local_instance_id);

/* coutns the number of bits set in an unsigned int */
int NBITS(unsigned int x)
{
	unsigned int c;
	for (c = 0; x; c++)
		x &= x - 1; // clear the least significant bit
	return c;
}

int is_bloom_filter_match_success(u8 *received_bloom_filter, u8 *my_mac_hash,
				  int bloom_filter_size)
{
	int i = 0;
	mwu_hexdump(MSG_INFO, "received_bloom_filter", received_bloom_filter,
		    bloom_filter_size);
	mwu_hexdump(MSG_INFO, "my mac hash", my_mac_hash, bloom_filter_size);
	for (i = 0; i < bloom_filter_size; i++) {
		if ((received_bloom_filter[i] & my_mac_hash[i]) !=
		    my_mac_hash[i])
			return 0;
	}
	return 1;
}

/**
 *  @brief Prints a MAC address in colon separated form from raw data
 *
 *  @param raw      A pointer to the hex data buffer
 *  @return         N/A
 */
void print_mac(u8 *raw)
{
	printf("%02x:%02x:%02x:%02x:%02x:%02x", (unsigned int)raw[0],
	       (unsigned int)raw[1], (unsigned int)raw[2], (unsigned int)raw[3],
	       (unsigned int)raw[4], (unsigned int)raw[5]);
	return;
}

/**
 *  Compute the 6-byte service hash from service name
 *
 *  params
 *      service - Pointer to struct asp_advertise_service
 *
 *  return
 *      none
 */
void compute_sha256_service_hash(char *name, unsigned char *service_hash)
{
	unsigned char service_name[MAX_SERVICE_NAME] = {0};
	unsigned char sha256_hash[64] = {0};
	const unsigned char *addr[2];
	size_t hash_len = strlen(name);
	char *tmp = name;

	for (; *name; ++name)
		*name = tolower(*name);

	memcpy(service_name, (unsigned char *)tmp, strlen(tmp));
	addr[0] = service_name;
	ERR("Service name used for computing service hash is %s", service_name);
	sha256_vector(1, addr, &hash_len, sha256_hash);
	memcpy(service_hash, sha256_hash, SERVICE_HASH_LEN);
	mwu_hexdump(MSG_INFO, "Service hash", service_hash, SERVICE_HASH_LEN);
	return;
}

/**
 *  @brief Creates a nan_ranging_request_frame and sends to the driver
 *
 *  params cur_if              struct mwu_iface_info pointer
 *      peer_mac               Mac address of peer to whom ranging request frame
 * is being sent return                 NAN_ERR_SUCCESS or NAN_ERR_COM
 */
enum nan_error nan_tx_ranging_request_frame(struct mwu_iface_info *cur_if,
					    char peer_mac[ETH_ALEN])
{
	int ret;
	u8 *var_attr_ptr;
	nan_action_frame *rf_buf = NULL;
	mrvl_cmd_head_buf *cmd;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_attr_len = sizeof(nan_action_frame);
	peer_availability_info *peer_info;

	ENTER();

	peer_info = &cur_if->pnan_info->peer_avail_info_published;

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */

	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	rf_buf = (nan_action_frame *)cmd->cmd_data;
	memcpy(rf_buf->peer_mac_addr, peer_mac, ETH_ALEN);

	rf_buf->tx_type = 3;
	rf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	rf_buf->action = 0x09;
	rf_buf->oui[0] = 0x50;
	rf_buf->oui[1] = 0x6F;
	rf_buf->oui[2] = 0x9A;
	rf_buf->oui_type = NAN_ACTION_FRAME;
	rf_buf->oui_sub_type = RANGING_REQUEST;

	var_attr_ptr = (u8 *)&rf_buf->information_content[0];

	INFO("Added Frame headers");

	// availability attribute
	nan_availability_attr *availability_attr;
	nan_availability_list *entry;
	nan_channel_entry_list *chan_list;
	u8 opt_fields_len = 7;
	u8 sz_of_entry_len = sizeof(u16);
	u32 ranging_req_bitmap, ranging_req_bitmap2;
	avail_entry_t *peer_entry = NULL;

	if (peer_info->potential_valid & ENTRY0_VALID)
		peer_entry = &peer_info->entry_potential[0];
	else if (peer_info->committed_valid & ENTRY0_VALID)
		peer_entry = &peer_info->entry_committed[0]; // in case some
							     // device sends
							     // committed entry
							     // in the publish
							     // frame

	availability_attr = (nan_availability_attr *)var_attr_ptr;
	availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
	availability_attr->attr_ctrl.map_id =
		cur_if->pnan_info->self_avail_info.map_id;
	availability_attr->seq_id =
		cur_if->pnan_info->self_avail_info.seq_id + 1;

	entry = availability_attr->entry;
	entry->entry_ctrl.avail_type = NDP_AVAIL_TYPE_COMMITTED;
	entry->entry_ctrl.usage_preference = 1;
	entry->entry_ctrl.utilization = 0;
	entry->entry_ctrl.rx_nss = 1;
	entry->entry_ctrl.time_bitmap_present = 1;

	/*Populate optional fields*/
	{
		time_bitmap_control *time_bitmap_ctrl =
			(time_bitmap_control *)&entry->optional[0];
		u8 *time_bitmap_len = &entry->optional[2];
		u8 *time_bitmap = &entry->optional[3];

		time_bitmap_ctrl->bit_duration = NDP_TIME_BM_DUR_16;
		time_bitmap_ctrl->bit_period = NDP_TIME_BM_PERIOD_512;
		time_bitmap_ctrl->start_offset = 0;
		*time_bitmap_len = 4; /*time_bitmap_len*/

		// Request the ranging schedule taking into consideration the
		// peer's bitmap
		ranging_req_bitmap =
			(DEFAULT_BITMAP1 & peer_entry->combined_time_bitmap);
		/* if(ranging_req_bitmap == 0)
		    ranging_req_bitmap = (PREFERRED_BITMAP2 &
		   peer_entry->combined_time_bitmap);
		*/

		memcpy(time_bitmap, &ranging_req_bitmap, 4); /*time bitmap*/
	}

	/*Hop over by 5 bytes(time_bitmap_len + time_bitmap) to get chan list */
	chan_list = (nan_channel_entry_list *)(var_attr_ptr +
					       sizeof(nan_availability_attr) +
					       sizeof(nan_availability_list) +
					       opt_fields_len);
	chan_list->entry_ctrl.entry_type = 1;
	chan_list->entry_ctrl.band_type = 0;
	chan_list->entry_ctrl.num_entries = 1;

	/*
	    chan_list->chan_band_entry.chan_entry.op_class =
	   peer_entry->op_class;
	    chan_list->chan_band_entry.chan_entry.chan_bitmap =
	   ndp_get_chan_bitmap(peer_entry->op_class, peer_entry->channels[0]);
	*/
	// ranging listen channel is channel 6
	chan_list->chan_band_entry.chan_entry.op_class = DEFAULT_2G_OP_CLASS;
	chan_list->chan_band_entry.chan_entry.chan_bitmap =
		ndp_get_chan_bitmap(DEFAULT_2G_OP_CLASS, DEFAULT_2G_OP_CHAN);
	chan_list->chan_band_entry.chan_entry.primary_chan_bitmap = 0x0;
	// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap = 0x0;

	entry->len = sizeof(nan_channel_entry_list) +
		     sizeof(nan_availability_list) + opt_fields_len -
		     sz_of_entry_len;
	availability_attr->len = sizeof(nan_availability_attr) -
				 NAN_ATTR_HDR_LEN + entry->len +
				 sz_of_entry_len;
	var_attr_ptr +=
		entry->len + sizeof(nan_availability_attr) + sz_of_entry_len;
	var_attr_len +=
		entry->len + sizeof(nan_availability_attr) + sz_of_entry_len;
	{ // We have to add another availability entry based on the peer's
	  // availability received case A] If the peer's availability was
	  // 'band_entry/availability at all slots' we have to add a potential
	  // entry as per the 1st test case case B] Else,  we have to add
	  // another committed entry as per the 2nd test case (e.g. specifying
	  // an alternate schedule).

		nan_availability_list *second_entry;
		second_entry = (nan_availability_list *)var_attr_ptr;

		second_entry->entry_ctrl.usage_preference = 1;
		second_entry->entry_ctrl.utilization = 0;
		second_entry->entry_ctrl.rx_nss = 1;
		second_entry->entry_ctrl.time_bitmap_present = 1;
		if (peer_info->band_entry_potential) // as per the case [A] in
						     // above comments
		{
			second_entry->entry_ctrl.avail_type =
				NDP_AVAIL_TYPE_POTENTIAL;
			ranging_req_bitmap2 = cur_if->pnan_info->self_avail_info
						      .entry_potential[0]
						      .time_bitmap[0];

		} else { // as per the case [B] in above comments

			second_entry->entry_ctrl.avail_type =
				NDP_AVAIL_TYPE_COMMITTED;
			ranging_req_bitmap2 =
				~(ranging_req_bitmap); // specify the available
						       // slots other than the
						       // ones given in the 1st
						       // entry
			ranging_req_bitmap2 =
				(ranging_req_bitmap2 & DEFAULT_BITMAP1);
		}
		/*Populate optional fields*/
		{
			time_bitmap_control *time_bitmap_ctrl =
				(time_bitmap_control *)&second_entry
					->optional[0];
			u8 *time_bitmap_len = &second_entry->optional[2];
			u8 *time_bitmap = &second_entry->optional[3];

			time_bitmap_ctrl->bit_duration = NDP_TIME_BM_DUR_16;
			time_bitmap_ctrl->bit_period = NDP_TIME_BM_PERIOD_512;
			time_bitmap_ctrl->start_offset = 0;
			*time_bitmap_len = 4; /*time_bitmap_len*/

			memcpy(time_bitmap, &ranging_req_bitmap2,
			       4); /*time bitmap*/
		}
		/*Hop over by 5 bytes(time_bitmap_len + time_bitmap) to get chan
		 * list */
		chan_list =
			(nan_channel_entry_list *)(var_attr_ptr +
						   sizeof(nan_availability_list) +
						   opt_fields_len);
		chan_list->entry_ctrl.entry_type = 1;
		chan_list->entry_ctrl.band_type = 0;
		chan_list->entry_ctrl.num_entries = 1;

		if (cur_if->pnan_info->a_band) {
			chan_list->chan_band_entry.chan_entry.op_class =
				DEFAULT_5G_OP_CLASS;
			chan_list->chan_band_entry.chan_entry.chan_bitmap =
				ndp_get_chan_bitmap(DEFAULT_5G_OP_CLASS,
						    DEFAULT_5G_OP_CHAN);
		} else {
			chan_list->chan_band_entry.chan_entry.op_class =
				DEFAULT_2G_OP_CLASS;
			chan_list->chan_band_entry.chan_entry.chan_bitmap =
				ndp_get_chan_bitmap(DEFAULT_2G_OP_CLASS,
						    DEFAULT_2G_OP_CHAN);
		}

		chan_list->chan_band_entry.chan_entry.primary_chan_bitmap = 0x0;

		// set the second entry's length
		second_entry->len = sizeof(nan_channel_entry_list) +
				    sizeof(nan_availability_list) +
				    opt_fields_len - sz_of_entry_len;
		// add the second entry's length in the main avail_attr and
		// other pointers
		availability_attr->len += second_entry->len + sz_of_entry_len;
		var_attr_ptr += second_entry->len + sz_of_entry_len;
		var_attr_len += second_entry->len + sz_of_entry_len;

	} // end of the block adding second entry

	INFO("Inserted availability_attr ");

	// Device capability attr
	nan_device_capability_attr device_capa_attr;

	memset(&device_capa_attr, 0, sizeof(device_capa_attr));

	device_capa_attr.attribute_id = NAN_DEVICE_CAPA_ATTR;
	device_capa_attr.len =
		sizeof(nan_device_capability_attr) - NAN_ATTR_HDR_LEN;
	device_capa_attr.committed_dw_info._2g_dw =
		cur_if->pnan_info->awake_dw_interval; // all slots on 2.4GHz
	if (cur_if->pnan_info->a_band)
		device_capa_attr.committed_dw_info._5g_dw = 1;
	device_capa_attr.supported_bands =
		cur_if->pnan_info->a_band ? 0x14 : 0x04;

	u8 operation_mode = 0x00;
	memcpy(&device_capa_attr.op_mode, &operation_mode, 1);
	device_capa_attr.no_of_antennas = 0x0;
	device_capa_attr.max_chan_sw_time = 5000;

	memcpy(var_attr_ptr, &device_capa_attr,
	       sizeof(nan_device_capability_attr));
	var_attr_len += sizeof(nan_device_capability_attr);
	var_attr_ptr += sizeof(nan_device_capability_attr);
	// INFO("Inserted Device capability attr
	// %lu",sizeof(nan_device_capability_attr));

	// Element container attr
	u8 supported_rates[6] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
	u8 ext_supported_rates[10] = {0x32, 0x08, 0x0c, 0x12, 0x18,
				      0x24, 0x30, 0x48, 0x60, 0x6c};
	u8 htcap[28] = {0x2d, 0x1a, 0xee, 0x01, 0x1f, 0xff, 0xff,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	u8 htinfo[24] = {0x3d, 0x16, 0x06, 0x05, 0x15, 0x00, 0x00, 0x00,
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	u8 *elem_ptr;
	u16 elem_len = 0;

	nan_element_container_attr *container_attr;

	container_attr = (nan_element_container_attr *)var_attr_ptr;
	elem_ptr = (u8 *)(&container_attr->elements[0]);
	container_attr->attribute_id = NAN_ELEMENT_CONTAINER_ATTR;
	container_attr->len = sizeof(supported_rates) +
			      sizeof(ext_supported_rates) + sizeof(htcap) +
			      sizeof(htinfo) + 1;
	memcpy(elem_ptr, supported_rates, sizeof(supported_rates));
	elem_ptr += sizeof(supported_rates);
	elem_len += sizeof(supported_rates);

	memcpy(elem_ptr, ext_supported_rates, sizeof(ext_supported_rates));
	elem_ptr += sizeof(ext_supported_rates);
	elem_len += sizeof(ext_supported_rates);

	memcpy(elem_ptr, htcap, sizeof(htcap));
	elem_ptr += sizeof(htcap);
	elem_len += sizeof(htcap);

	memcpy(elem_ptr, htinfo, sizeof(htinfo));
	elem_ptr += sizeof(htinfo);
	elem_len += sizeof(htinfo);

	var_attr_len += sizeof(nan_element_container_attr) + elem_len;
	var_attr_ptr += sizeof(nan_element_container_attr) + elem_len;
	// INFO("Element container attr %lu",sizeof(nan_element_container_attr)
	// + elem_len);

	// Ranging info attr

	nan_ranging_info_attr info_attr;

	info_attr.attribute_id = NAN_RANGING_INFO_ATTR;
	info_attr.location_info = 0;
	// info_attr.last_movement_indication = 0;
	info_attr.len = sizeof(nan_ranging_info_attr) - NAN_ATTR_HDR_LEN;

	memcpy(var_attr_ptr, &info_attr, sizeof(nan_ranging_info_attr));
	var_attr_len += sizeof(nan_ranging_info_attr);
	var_attr_ptr += sizeof(nan_ranging_info_attr);
	// INFO("Ranging info attr %d", sizeof(nan_ranging_info_attr));

	// Ranging setup attr

	nan_ranging_setup_attr *setup_attr;
	setup_attr = (nan_ranging_setup_attr *)var_attr_ptr;
	setup_attr->dialog_token = 1;
	setup_attr->attribute_id = NAN_RANGING_SETUP_ATTR;
	setup_attr->type_and_status = 0x00;

	var_attr_len += sizeof(nan_ranging_setup_attr);
	var_attr_ptr += sizeof(nan_ranging_setup_attr);
	setup_attr->len = sizeof(nan_ranging_setup_attr) - NAN_ATTR_HDR_LEN;

	if (cur_if->pnan_info->nan_ftm_params) {
		setup_attr->ranging_control.ftm_params_present = 0x1;
		memcpy(var_attr_ptr, cur_if->pnan_info->nan_ftm_params, 3);
		var_attr_len += 3;
		var_attr_ptr += 3;
		setup_attr->len += 3;
	}

	setup_attr->ranging_control.bitmap_params_present = 0x1;
	// copy the time bitmap fields from the NAN availability attribute.

	// Add the map_id field
	u8 map_id = cur_if->pnan_info->self_avail_info.map_id;
	map_id = availability_attr->attr_ctrl.map_id;
	memcpy(var_attr_ptr, &map_id, 1);
	var_attr_len += 1;
	var_attr_ptr += 1;
	setup_attr->len += 1;

	// Time bitmap ctrl
	time_bitmap_control time_bitmap_ctrl;
	time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
	time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
	time_bitmap_ctrl.start_offset = 0;
	time_bitmap_ctrl.reserved = 0;

	memcpy(var_attr_ptr, &time_bitmap_ctrl, sizeof(time_bitmap_control));
	var_attr_len += sizeof(time_bitmap_control);
	var_attr_ptr += sizeof(time_bitmap_control);
	setup_attr->len += sizeof(time_bitmap_control);

	u8 time_btmp_len;
	time_btmp_len = 4;

	// Time bitmap length
	memcpy(var_attr_ptr, &time_btmp_len, 1);
	var_attr_len += 1;
	var_attr_ptr += 1;
	setup_attr->len += 1;

	// Time bitmap
	memcpy(var_attr_ptr, &ranging_req_bitmap, 4);

	var_attr_len += 4;
	var_attr_ptr += 4;
	setup_attr->len += 4;

	// INFO("Ranging setup attr %d", sizeof(nan_ranging_setup_attr) +
	// av_attr->entry->time_bitmap_len);

	cmd_len = cmd_len + var_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;

	mwu_hexdump(MSG_ERROR, "RANGE REQ", cmd->cmd_data, var_attr_len);
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);
	LEAVE();
	return ret;
}

/**
 *  @brief Creates a nan_sd_frame and sends to the driver
 *
 *  params
 *      cur_if              struct mwu_iface_info pointer
 *      type                publish/subscribe
 *      dest                Mac address of peer to whom action frame is being
 * sent frame return                 NAN_ERR_SUCCESS or NAN_ERR_COM
 */
enum nan_error nan_tx_sdf(struct mwu_iface_info *cur_if, int type,
			  unsigned char *dest)
{
	int ret;
	mrvl_cmd_head_buf *cmd;
	nan_sd_frame *sdf_buf = NULL;
	nan_service_descriptor_attr *sd_attr = NULL;
	struct subscribed_service *sub_service_ptr = NULL;
	struct published_service *pub_service_ptr = NULL;
	u8 *sda_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int fixed_sd_attr_len = sizeof(nan_service_descriptor_attr) - 3;
	int var_sd_attr_len = 0;
	u8 dev_final_hash[BLOOM_FILTER_SIZE] = {0};
	u16 hash_arr[4]; // 4 = num_of_hash_func

	ENTER();

	pub_service_ptr = &cur_if->pnan_info->p_service;
	sub_service_ptr = &cur_if->pnan_info->s_service;

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_sd_frame *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr, dest, ETH_ALEN);
	sdf_buf->tx_type = 1; /* use default sd frame queue in FW - If firmware
				 is slow sending the frames out on air, there is
				 a possibility for MWU creating back pressure on
				 firmware queue..*/
	sdf_buf->ttl = 0;
	if (cur_if->pnan_info->peer_avail_info_published.single_band) {
		ERR("Peer is single band");
		sdf_buf->ttl = 1;
	}

	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_SD_FRAME;

	sd_attr = (nan_service_descriptor_attr *)&sdf_buf->sd_payload_frame[0];
	sd_attr->attribute_id = 0x3;

	sda_var_attr_ptr = (u8 *)(sd_attr->sda_var_attr);
	if (type == PUBLISH) {
		memcpy(sd_attr->service_hash, pub_service_ptr->service_hash,
		       SERVICE_HASH_LEN);
		sd_attr->instance_id = 0x05;
		sd_attr->requester_instance_id = cur_if->pnan_info->instance_id;
		sd_attr->service_control_bitmap = 0;
		ERR("==> pub_service_ptr->matching_filter_tx %s",
		    pub_service_ptr->matching_filter_tx);
		if ((strcasecmp(pub_service_ptr->matching_filter_tx, "null") !=
		     0)) {
			int total_filter_len = pub_service_ptr->tx_filter_len;
			mwu_hexdump(MSG_ERROR, "==>Adding matching filter",
				    (u8 *)pub_service_ptr->matching_filter_tx,
				    total_filter_len);

			*sda_var_attr_ptr = total_filter_len;
			sda_var_attr_ptr++;
			var_sd_attr_len += 1;

			memcpy(sda_var_attr_ptr,
			       pub_service_ptr->matching_filter_tx,
			       total_filter_len);
			var_sd_attr_len += total_filter_len;
			sda_var_attr_ptr += total_filter_len;
			sd_attr->service_control_bitmap |=
				SERVICE_CTRL_BITMAP_MF_PRESENT;

			ERR("==> %d", var_sd_attr_len);
		}

		if (cur_if->pnan_info->include_srf == 1) {
			if (cur_if->pnan_info->saved_srf.srf_ctrl & 1) {
				u8 bloom_filter_index = 0;
				u32 dev_crc;
				u8 i = 0, p = 0, q = 0;
				u8 mac_hash[BLOOM_FILTER_SIZE];
				print_mac(cur_if->device_mac_addr);
				INFO("");
				// print_mac(cur_if->pnan_info->saved_srf.mac_list);
				INFO("Bloom filter present. Transmit bloom filter in the frame at index %d",
				     bloom_filter_index);
				for (p = 0;
				     p < cur_if->pnan_info->saved_srf.num_mac;
				     p++) {
					for (i = 0; i < 4; i++) {
						dev_crc = compute_bloom_filter_crc(
							(i +
							 4 * bloom_filter_index),
							cur_if->pnan_info
								->saved_srf
								.mac_list[p],
							1);
						dev_crc = dev_crc & 0x0000ffff;
						hash_arr[i] =
							dev_crc %
							(8 * BLOOM_FILTER_SIZE);
						ERR("\nHash 1 bits %04x",
						    hash_arr[i]);
						// tmp++;
					}
					compute_hash(hash_arr, mac_hash,
						     BLOOM_FILTER_SIZE);
					for (q = 0; q < BLOOM_FILTER_SIZE; q++)
						dev_final_hash[q] |=
							mac_hash[q];
				}

				mwu_hexdump(MSG_INFO,
					    "Tx Final bloom filter hash",
					    dev_final_hash, BLOOM_FILTER_SIZE);

				*sda_var_attr_ptr =
					1 + BLOOM_FILTER_SIZE; /* 1 byte
								  srf_ctrl and 4
								  bytes bloom
								  filter */
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				*sda_var_attr_ptr =
					cur_if->pnan_info->saved_srf.srf_ctrl;
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				memcpy(sda_var_attr_ptr, dev_final_hash,
				       BLOOM_FILTER_SIZE);
				var_sd_attr_len += BLOOM_FILTER_SIZE;
				sda_var_attr_ptr += BLOOM_FILTER_SIZE;
				sd_attr->service_control_bitmap |=
					SERVICE_CTRL_BITMAP_SRF_PRESENT;

			} else {
				*sda_var_attr_ptr =
					sizeof(cur_if->pnan_info->saved_srf
						       .srf_ctrl) +
					6 * cur_if->pnan_info->saved_srf.num_mac;
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				*sda_var_attr_ptr =
					cur_if->pnan_info->saved_srf.srf_ctrl;
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				memcpy(sda_var_attr_ptr,
				       cur_if->pnan_info->saved_srf.mac_list,
				       6 * cur_if->pnan_info->saved_srf.num_mac);
				var_sd_attr_len +=
					6 *
					cur_if->pnan_info->saved_srf.num_mac;
				sda_var_attr_ptr +=
					6 *
					cur_if->pnan_info->saved_srf.num_mac;
				sd_attr->service_control_bitmap |=
					SERVICE_CTRL_BITMAP_SRF_PRESENT;
			}
		}

		if (pub_service_ptr->discovery_range == 0) { /* means discovery
								range is limited
								to close
								proximity */
			INFO("Discovery range is limited for this service");
			sd_attr->service_control_bitmap |=
				SERVICE_CTRL_BITMAP_DISC_RANGE_LTD;
		}

		sd_attr->len = fixed_sd_attr_len + var_sd_attr_len;
	}
	if (type == SUBSCRIBE) {
		memcpy(sd_attr->service_hash, sub_service_ptr->service_hash,
		       SERVICE_HASH_LEN);
		sd_attr->instance_id = 0x05;
		sd_attr->service_control_bitmap |=
			SERVICE_CTRL_BITMAP_SUBSCRIBE;
		ERR("==> sub_service_ptr->matching_filter_tx %s",
		    sub_service_ptr->matching_filter_tx);
		if ((strcasecmp(sub_service_ptr->matching_filter_tx, "null") !=
		     0)) {
			int total_filter_len = sub_service_ptr->tx_filter_len;
			mwu_hexdump(MSG_ERROR, "==>Adding Tx matching filter",
				    (u8 *)sub_service_ptr->matching_filter_tx,
				    total_filter_len);

			*sda_var_attr_ptr = total_filter_len;
			sda_var_attr_ptr++;
			var_sd_attr_len += 1;

			memcpy(sda_var_attr_ptr,
			       sub_service_ptr->matching_filter_tx,
			       total_filter_len);
			var_sd_attr_len += total_filter_len;
			sda_var_attr_ptr += total_filter_len;
			sd_attr->service_control_bitmap |=
				SERVICE_CTRL_BITMAP_MF_PRESENT;
			ERR("==> %d", var_sd_attr_len);
		}

		if (cur_if->pnan_info->include_srf == 1) {
			if (cur_if->pnan_info->saved_srf.srf_ctrl & 1) {
				u8 bloom_filter_index = 0;
				u32 dev_crc;
				u8 i = 0, p = 0, q = 0;
				u8 mac_hash[BLOOM_FILTER_SIZE];
				print_mac(cur_if->device_mac_addr);
				INFO("");
				// print_mac(cur_if->pnan_info->saved_srf.mac_list);
				INFO("Bloom filter present. Transmit bloom filter in the frame at index %d",
				     bloom_filter_index);
				for (p = 0;
				     p < cur_if->pnan_info->saved_srf.num_mac;
				     p++) {
					for (i = 0; i < 4; i++) {
						dev_crc = compute_bloom_filter_crc(
							(i +
							 4 * bloom_filter_index),
							cur_if->pnan_info
								->saved_srf
								.mac_list[p],
							1);
						dev_crc = dev_crc & 0x0000ffff;
						hash_arr[i] =
							dev_crc %
							(8 * BLOOM_FILTER_SIZE);
						ERR("\nHash 1 bits %04x",
						    hash_arr[i]);
						// tmp++;
					}
					compute_hash(hash_arr, mac_hash,
						     BLOOM_FILTER_SIZE);
					for (q = 0; q < BLOOM_FILTER_SIZE; q++)
						dev_final_hash[q] |=
							mac_hash[q];
				}

				mwu_hexdump(MSG_INFO,
					    "Tx Final bloom filter hash",
					    dev_final_hash, BLOOM_FILTER_SIZE);

				*sda_var_attr_ptr =
					1 + BLOOM_FILTER_SIZE; /* 1 byte
								  srf_ctrl and 4
								  bytes bloom
								  filter */
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				*sda_var_attr_ptr =
					cur_if->pnan_info->saved_srf.srf_ctrl;
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				memcpy(sda_var_attr_ptr, dev_final_hash,
				       BLOOM_FILTER_SIZE);
				var_sd_attr_len += BLOOM_FILTER_SIZE;
				sda_var_attr_ptr += BLOOM_FILTER_SIZE;
				sd_attr->service_control_bitmap |=
					SERVICE_CTRL_BITMAP_SRF_PRESENT;
			} else {
				*sda_var_attr_ptr =
					sizeof(cur_if->pnan_info->saved_srf
						       .srf_ctrl) +
					6 * cur_if->pnan_info->saved_srf.num_mac;
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				*sda_var_attr_ptr =
					cur_if->pnan_info->saved_srf.srf_ctrl;
				sda_var_attr_ptr++;
				var_sd_attr_len += 1;

				memcpy(sda_var_attr_ptr,
				       cur_if->pnan_info->saved_srf.mac_list,
				       6 * cur_if->pnan_info->saved_srf.num_mac);
				var_sd_attr_len +=
					6 *
					cur_if->pnan_info->saved_srf.num_mac;
				sda_var_attr_ptr +=
					6 *
					cur_if->pnan_info->saved_srf.num_mac;
				sd_attr->service_control_bitmap |=
					SERVICE_CTRL_BITMAP_SRF_PRESENT;
			}
		}

		if (sub_service_ptr->discovery_range == 0) { /* means discovery
								range is limited
								to close
								proximity */
			INFO("Discovery range is limited for this service");
			sd_attr->service_control_bitmap |=
				SERVICE_CTRL_BITMAP_DISC_RANGE_LTD;
		}

		sd_attr->len = fixed_sd_attr_len + var_sd_attr_len;
	}

#ifdef NAN1_TESTBED

	{
		u8 nan_further_sd_attr[8] = {0x09, 0x05, 0x00, 0x00,
					     0x00, 0xfc, 0xff, 0x00};
		u8 nan_fa_attribute[11] = {0x0a, 0x08, 0x00, 0x00, 0x00, 0x51,
					   0x06, 0x00, 0xfc, 0xff, 0x00};

		ERR("Copying further availability attr");
		memcpy(sda_var_attr_ptr, nan_further_sd_attr, 8);
		sda_var_attr_ptr = sda_var_attr_ptr + 8;
		var_sd_attr_len = var_sd_attr_len + 8;
		memcpy(sda_var_attr_ptr, nan_fa_attribute, 11);
		sda_var_attr_ptr = sda_var_attr_ptr + 11;
		var_sd_attr_len = var_sd_attr_len + 11;
	}

#else
	if (cur_if->pnan_info->include_fa_attr == 1) {
		nan_fa_attr *fa_attr;
		nan_fa_p2p_attr *fa_p2p_attr;
		struct nan_params_p2p_fa *p2p_fa;
		struct nan_params_fa fap;

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
		if (ret != NAN_ERR_SUCCESS)
			ERR("Failed to set FA parameters in FW");

		INFO("Copying further availability attr");

		/* P2P FA attribute */
		fa_p2p_attr = (nan_fa_p2p_attr *)sda_var_attr_ptr;

		fa_p2p_attr->attribute_id = NAN_FURTHER_P2P_ATTR;
		fa_p2p_attr->len = sizeof(nan_fa_p2p_attr) - 3;
		fa_p2p_attr->device_role = p2p_fa->dev_role;
		memcpy(&fa_p2p_attr->mac, &p2p_fa->mac, 6);
		fa_p2p_attr->map_ctrl = p2p_fa->ctrl;
		fa_p2p_attr->availability_map = p2p_fa->availability_map;

		sda_var_attr_ptr += sizeof(nan_fa_p2p_attr);
		var_sd_attr_len += sizeof(nan_fa_p2p_attr);

		fa_attr = (nan_fa_attr *)sda_var_attr_ptr;
		fa_attr->attribute_id = NAN_FURTHER_AVAIL_MAP_ATTR;
		fa_attr->len = sizeof(nan_fa_attr) - 3;
		fa_attr->map_id = p2p_fa->map_id;
		fa_attr->e.ctrl = p2p_fa->ctrl;
		fa_attr->e.op_class = p2p_fa->op_class;
		fa_attr->e.chan_num = p2p_fa->op_chan;
		fa_attr->e.availability_btmp = p2p_fa->availability_map;

		sda_var_attr_ptr += sizeof(nan_fa_attr);
		var_sd_attr_len += sizeof(nan_fa_attr);
	}
#endif

	{
		nan_device_capa_attr *device_capa_attr;
		device_capa_attr = (nan_device_capa_attr *)sda_var_attr_ptr;
		device_capa_attr->attribute_id = NAN_DEVICE_CAPA_ATTR;
		device_capa_attr->len = sizeof(nan_device_capa_attr) - 3;
		device_capa_attr->committed_dw_info =
			cur_if->pnan_info->awake_dw_interval; /*awake dw info
								 for 2.4 ghz
								 band*/
		if (cur_if->pnan_info->a_band) {
			device_capa_attr->supported_bands = 0x14; // gbhat@HC:only
								  // 5GHz
			device_capa_attr->committed_dw_info |= 0x08; // gbhat@HC:available
								     // on all
								     // slots on
								     // 5GHz
		} else {
			device_capa_attr->supported_bands = 0x04; // gbhat@HC:only
								  // 2.4GHz
		}
		if (cur_if->pnan_info->ndpe_attr_supported) {
			device_capa_attr->capabilities.ndpe_attr_supported =
				TRUE;
		}
		device_capa_attr->max_chan_switch_time = 5000;
		sda_var_attr_ptr += sizeof(nan_device_capa_attr);
		var_sd_attr_len += sizeof(nan_device_capa_attr);
	}

	if (cur_if->pnan_info->ranging_required == 1) {
		nan_ranging_info_attr info_attr;

		info_attr.attribute_id = NAN_RANGING_INFO_ATTR;
		info_attr.len =
			sizeof(nan_ranging_info_attr) - NAN_ATTR_HDR_LEN;
		memcpy(sda_var_attr_ptr, &info_attr,
		       sizeof(nan_ranging_info_attr));
		var_sd_attr_len += sizeof(nan_ranging_info_attr);
		sda_var_attr_ptr += sizeof(nan_ranging_info_attr);
	}

	if (cur_if->pnan_info->data_path_needed ||
	    cur_if->pnan_info->ranging_required == 1) {
		nan_service_desc_ext_attr *service_desc_ext_attr;
		service_desc_ext_attr =
			(nan_service_desc_ext_attr *)sda_var_attr_ptr;
		service_desc_ext_attr->attribute_id = NAN_SERVICE_DISC_EXT_ATTR;
		service_desc_ext_attr->len =
			sizeof(nan_service_desc_ext_attr) - 3;
		service_desc_ext_attr->instance_id =
			cur_if->pnan_info->instance_id;
		service_desc_ext_attr->control |=
			SDEA_CTRL_BITMAP_DATAPATH_REQUIRED;

		// qos required bit set by default
		service_desc_ext_attr->control |= SDEA_CTRL_QOS_REQUIRED;

		if (cur_if->pnan_info->security_required) { /*Proper check to be
							       added*/
			service_desc_ext_attr->control |=
				SDEA_CTRL_BITMAP_SECURITY_REQUIRED; // gbhat@HC:Data
								    // path
								    // required
								    // =1(b2) |
								    // Data path
								    // type =
								    // 0(b3)
		}
		if (cur_if->pnan_info->ranging_required == 1) {
			service_desc_ext_attr->control |=
				SDEA_CTRL_RANGING_REQUIRED;
			INFO("service_desc_ext_attr->control %d",
			     service_desc_ext_attr->control);
		}

		sda_var_attr_ptr += sizeof(nan_service_desc_ext_attr);
		var_sd_attr_len += sizeof(nan_service_desc_ext_attr);
		//        mwu_hexdump(MSG_ERROR, "==>SDEA:",
		//        service_desc_ext_attr,
		//        sizeof(nan_service_desc_ext_attr));
	}
	/*Include security attrs*/
	if (cur_if->pnan_info->security_required) {
		{
			/*Cipher suite info attr*/
			nan_cipher_suite_info_attr *csinfo =
				(nan_cipher_suite_info_attr *)sda_var_attr_ptr;
			csinfo->attribute_id = NAN_CIPHER_SUITE_INFO_ATTR;
			csinfo->capabilities = 0;
			csinfo->attr_info[0].cipher_suite_id = NCS_SK_CCM_128;
			csinfo->attr_info[0].instance_id = 5; /*All services*/

			csinfo->len = sizeof(nan_cipher_suite_info_attr) +
				      sizeof(nan_cipher_suite_attr_info) -
				      NAN_ATTR_HDR_LEN;
			sda_var_attr_ptr += sizeof(nan_cipher_suite_info_attr) +
					    sizeof(nan_cipher_suite_attr_info);
			var_sd_attr_len += sizeof(nan_cipher_suite_info_attr) +
					   sizeof(nan_cipher_suite_attr_info);
		}
		{
			/*Security context info attr*/
			nan_security_context_info_attr *sec_ctx_info =
				(nan_security_context_info_attr *)
					sda_var_attr_ptr;
#if 0
            u8 pmkid[16] = {0};
            u8 dest_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            u8 *service_name = "test" ;
            u8 service_id[SERVICE_ID_LEN] ;
             /*TODO : call pass correct service_name in lower case*/
            nan_generate_service_id(service_name, strlen(service_name), service_id);
#endif
			u8 service_id[SERVICE_ID_LEN];
			u8 pmkid[16] = {0};
			unsigned char b_cast_mac[6] = {0xff, 0xff, 0xff,
						       0xff, 0xff, 0xff};

			if (type == SUBSCRIBE) {
				memcpy(service_id,
				       cur_if->pnan_info->s_service.service_hash,
				       SERVICE_HASH_LEN);
			}
			if (type == PUBLISH) {
				memcpy(service_id,
				       cur_if->pnan_info->p_service.service_hash,
				       SERVICE_HASH_LEN);
			}

			nan_generate_pmkid(
				cur_if->pnan_info->nan_security.pmk,
				(size_t)cur_if->pnan_info->nan_security.pmk_len,
				(u8 *)b_cast_mac, (u8 *)cur_if->device_mac_addr,
				service_id, pmkid);

			sec_ctx_info->attribute_id = NAN_SEC_CONTEXT_INFO_ATTR;
			sec_ctx_info->identifier_list[0].identifier_len =
				PMKID_LEN;
			sec_ctx_info->identifier_list[0].identifier_type =
				1 /*PMKID*/;
			sec_ctx_info->identifier_list[0].instance_id =
				5 /*All services*/;
			// memset(&sec_ctx_info->identifier_list[0].sec_context_identifier,0,16);/*Copy
			// PMKID*/
			memcpy(&sec_ctx_info->identifier_list[0]
					.sec_context_identifier,
			       pmkid, 16); /*Copy PMKID*/

			sec_ctx_info->len =
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) + 16 -
				NAN_ATTR_HDR_LEN;

			sda_var_attr_ptr +=
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) + 16;
			var_sd_attr_len +=
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) + 16;
		}
	}
	if (cur_if->pnan_info->data_path_needed ||
	    cur_if->pnan_info->ranging_required == 1) {
		if (cur_if->pnan_info->avail_attr.len > 0 &&
		    (!(cur_if->pnan_info->ranging_required == 1))) {
			nan_availability_attr *availability_attr =
				(nan_availability_attr *)sda_var_attr_ptr;
			memcpy(availability_attr,
			       cur_if->pnan_info->avail_attr.data,
			       cur_if->pnan_info->avail_attr.len);
			availability_attr->seq_id =
				cur_if->pnan_info->self_avail_info.seq_id;
			sda_var_attr_ptr += cur_if->pnan_info->avail_attr.len;
			var_sd_attr_len += cur_if->pnan_info->avail_attr.len;
		} else {
			nan_availability_attr *availability_attr;
			nan_availability_list *entry;
			nan_channel_entry_list *chan_list;
			u8 opt_fields_len = 7;

			availability_attr =
				(nan_availability_attr *)sda_var_attr_ptr;
			availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
			availability_attr->attr_ctrl.map_id =
				cur_if->pnan_info->self_avail_info.map_id;
			INFO("==> Publish Map id is %d",
			     availability_attr->attr_ctrl.map_id);
			/*Report Change in potential entry*/
			availability_attr->attr_ctrl.committed_changed =
				cur_if->pnan_info->self_avail_info
					.committed_changed;
			availability_attr->attr_ctrl.potential_changed =
				cur_if->pnan_info->self_avail_info
					.potential_changed;
			availability_attr->seq_id =
				cur_if->pnan_info->self_avail_info.seq_id;

			entry = availability_attr->entry;
			entry->entry_ctrl.avail_type = NDP_AVAIL_TYPE_POTENTIAL;
			entry->entry_ctrl.usage_preference = 0x1;
			entry->entry_ctrl.utilization = 0x0;
			entry->entry_ctrl.rx_nss = 0x1;

			if (cur_if->pnan_info->nan_other_if_active == TRUE) {
				cur_if->pnan_info->self_avail_info
					.band_entry_potential = FALSE;
				INFO("other interface active");
			} else {
				INFO("only nan0 active");
			}

			if ((cur_if->pnan_info->self_avail_info
				     .band_entry_potential) &&
			    (!(cur_if->pnan_info->ranging_required == 1))) {
				u8 sz_of_chan_entry_list =
					sizeof(chan_entry_list_t);
				u8 sz_of_band_id = sizeof(u8);
				u8 sz_of_entry_len = sizeof(u16);
				u8 band_id_len = 0;

				if (cur_if->pnan_info->self_avail_info
					    .time_bitmap_present_potential) {
					entry->entry_ctrl.time_bitmap_present =
						1;
					/*Polulate optional attributes*/
					{
						time_bitmap_control *time_bitmap_ctrl =
							(time_bitmap_control
								 *)&entry
								->optional[0];
						u8 *time_bitmap_len =
							&entry->optional[2];
						u8 *time_bitmap =
							&entry->optional[3];
						time_bitmap_ctrl->bit_duration =
							NDP_TIME_BM_DUR_16;
						time_bitmap_ctrl->bit_period =
							NDP_TIME_BM_PERIOD_512;
						time_bitmap_ctrl->start_offset =
							0;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						memcpy(time_bitmap,
						       &cur_if->pnan_info
								->self_avail_info
								.entry_potential
									[0]
								.time_bitmap[0],
						       4); /*time bitmap*/
					}

				} else {
					/*Device available on all slots on the
					 * given band*/
					entry->entry_ctrl.time_bitmap_present =
						0;
					opt_fields_len = 0;
				}
				chan_list =
					(nan_channel_entry_list
						 *)(sda_var_attr_ptr +
						    sizeof(nan_availability_attr) +
						    sizeof(nan_availability_list) +
						    opt_fields_len);
				chan_list->entry_ctrl.entry_type = 0x0;
				chan_list->entry_ctrl.band_type = 0x0;
				if (cur_if->pnan_info->a_band)
					chan_list->entry_ctrl.num_entries = 0x2;
				else
					chan_list->entry_ctrl.num_entries = 0x1;

				chan_list->chan_band_entry.band_id =
					0x02; /*2.4 GHz*/

				if (cur_if->pnan_info->a_band) {
					u8 *temp =
						(u8 *)(&chan_list
								->chan_band_entry
								.band_id);
					temp++;
					*temp = 0x04; /*5 GHz*/
					band_id_len++;
				}

				entry->len = sizeof(nan_channel_entry_list) +
					     sizeof(nan_availability_list) +
					     opt_fields_len - sz_of_entry_len -
					     sz_of_chan_entry_list +
					     sz_of_band_id + band_id_len;
				availability_attr->len =
					sizeof(nan_availability_attr) -
					NAN_ATTR_HDR_LEN + entry->len +
					sz_of_entry_len;
				sda_var_attr_ptr +=
					entry->len +
					sizeof(nan_availability_attr) +
					sz_of_entry_len;
				var_sd_attr_len +=
					entry->len +
					sizeof(nan_availability_attr) +
					sz_of_entry_len;

			} else {
				entry->entry_ctrl.time_bitmap_present = 0x1;
				/*Polulate optional attributes*/
				{
					time_bitmap_control *time_bitmap_ctrl =
						(time_bitmap_control *)&entry
							->optional[0];
					u8 *time_bitmap_len =
						&entry->optional[2];
					u8 *time_bitmap = &entry->optional[3];
					time_bitmap_ctrl->bit_duration =
						NDP_TIME_BM_DUR_16;
					time_bitmap_ctrl->bit_period =
						NDP_TIME_BM_PERIOD_512;
					time_bitmap_ctrl->start_offset = 0;
					*time_bitmap_len =
						4; /*time_bitmap_len*/
					if (cur_if->pnan_info
						    ->ranging_required == 1)
						cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.time_bitmap[0] =
							NAN_DEFAULT_BITMAP;
					else
						cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.time_bitmap[0] =
							NAN_DEFAULT_CONCURRENT_BITMAP;

					memcpy(time_bitmap,
					       &cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.time_bitmap[0],
					       4); /*time bitmap*/
				}

				/*Jump over (time_bitmap_len + time_bitmap)
				 * bytes to get chan list*/
				chan_list =
					(nan_channel_entry_list
						 *)(sda_var_attr_ptr +
						    sizeof(nan_availability_attr) +
						    sizeof(nan_availability_list) +
						    opt_fields_len);
				chan_list->entry_ctrl.entry_type = 0x1;
				chan_list->entry_ctrl.band_type = 0x0;
				chan_list->entry_ctrl.num_entries = 0x1;

				if (cur_if->pnan_info->a_band) {
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.op_class = DEFAULT_5G_OP_CLASS;
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.channels[0] =
						DEFAULT_5G_OP_CHAN;
				} else {
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.op_class = DEFAULT_2G_OP_CLASS;
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.channels[0] =
						DEFAULT_2G_OP_CHAN;
				}

				chan_list->chan_band_entry.chan_entry.op_class =
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.op_class;
				chan_list->chan_band_entry.chan_entry
					.chan_bitmap = ndp_get_chan_bitmap(
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.op_class,
					cur_if->pnan_info->self_avail_info
						.entry_potential[0]
						.channels[0]);
				chan_list->chan_band_entry.chan_entry
					.primary_chan_bitmap = 0x0;
				// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
				// = 0x0;

				entry->len = sizeof(nan_channel_entry_list) +
					     sizeof(nan_availability_list) +
					     opt_fields_len - 2;
				availability_attr->len =
					sizeof(nan_availability_attr) -
					NAN_ATTR_HDR_LEN + entry->len + 2;
				sda_var_attr_ptr +=
					entry->len +
					sizeof(nan_availability_attr) + 2;
				var_sd_attr_len +=
					entry->len +
					sizeof(nan_availability_attr) + 2;
			}
			{
				/*Add Committed entry if available*/
				nan_availability_list *entry;
				nan_channel_entry_list *chan_list;
				u8 opt_fields_len = 7, i = 0, k = 0;

				for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
					/*Send all the valid conditional entries
					 * in proposal*/
					if ((cur_if->pnan_info->self_avail_info
						     .committed_valid &
					     (1 << i))) {
						for (k = 0;
						     k <
						     cur_if->pnan_info
							     ->self_avail_info
							     .entry_committed[i]
							     .time_bitmap_count;
						     k++) {
							INFO("Adding committed entry#%u",
							     i);
							entry = (nan_availability_list
									 *)
								sda_var_attr_ptr;
							entry->entry_ctrl
								.avail_type =
								NDP_AVAIL_TYPE_COMMITTED;
							entry->entry_ctrl
								.usage_preference =
								0x1;
							entry->entry_ctrl
								.utilization =
								0x0;
							entry->entry_ctrl
								.rx_nss = 0x1;
							entry->entry_ctrl
								.time_bitmap_present =
								0x1;
							/*Polulate optional
							 * attributes*/
							{
								time_bitmap_control *time_bitmap_ctrl =
									(time_bitmap_control
										 *)&entry
										->optional
											[0];
								u8 *time_bitmap_len =
									&entry->optional
										 [2];
								u8 *time_bitmap =
									&entry->optional
										 [3];
								time_bitmap_ctrl
									->bit_duration =
									NDP_TIME_BM_DUR_16;
								time_bitmap_ctrl
									->bit_period =
									cur_if->pnan_info
										->self_avail_info
										.entry_committed
											[i]
										.period;
								time_bitmap_ctrl
									->start_offset =
									cur_if->pnan_info
										->self_avail_info
										.entry_committed
											[i]
										.start_offset
											[k] /
									16;
								*time_bitmap_len =
									4; /*time_bitmap_len*/
								memcpy(time_bitmap,
								       &cur_if->pnan_info
										->self_avail_info
										.entry_committed
											[i]
										.time_bitmap
											[k],
								       4); /*time
									      bitmap*/
							}

							chan_list =
								(nan_channel_entry_list
									 *)(sda_var_attr_ptr +
									    sizeof(nan_availability_list) +
									    opt_fields_len);
							chan_list->entry_ctrl
								.entry_type =
								0x1;
							chan_list->entry_ctrl
								.band_type =
								0x0;
							chan_list->entry_ctrl
								.num_entries =
								0x1;

							chan_list
								->chan_band_entry
								.chan_entry
								.op_class =
								cur_if->pnan_info
									->self_avail_info
									.entry_committed
										[i]
									.op_class;
							chan_list
								->chan_band_entry
								.chan_entry
								.chan_bitmap = ndp_get_chan_bitmap(
								cur_if->pnan_info
									->self_avail_info
									.entry_committed
										[i]
									.op_class,
								cur_if->pnan_info
									->self_avail_info
									.entry_committed
										[i]
									.channels[0]);
							chan_list
								->chan_band_entry
								.chan_entry
								.primary_chan_bitmap =
								0x0;
							// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
							// = 0x0;

							entry->len =
								sizeof(nan_channel_entry_list) +
								sizeof(nan_availability_list) +
								opt_fields_len -
								2;
							availability_attr->len +=
								entry->len + 2;
							sda_var_attr_ptr +=
								entry->len + 2;
							var_sd_attr_len +=
								entry->len + 2;
						}
					}
				}
			}
			mwu_hexdump(MSG_ERROR,
				    "==>AVAIL_ATTR:", (u8 *)availability_attr,
				    sizeof(nan_availability_attr));
		}
	}

	if (type == FOLLOW_UP) {
		/* Change Tx Type to 2 for follow up frames so that they go out
		 * immediately */
		sdf_buf->tx_type = 2; /* use alternate queue in FW */
		sd_attr->service_control_bitmap |=
			SERVICE_CTRL_BITMAP_FOLLOW_UP;
		ERR("Preparing a follow up message %d",
		    sd_attr->service_control_bitmap);
		memcpy(sd_attr->service_hash, pub_service_ptr->service_hash,
		       SERVICE_HASH_LEN);

		if (memcmp(sd_attr->service_hash, "\x00\x00\x00\x00\x00\x00",
			   6) == 0)
			memcpy(sd_attr->service_hash,
			       sub_service_ptr->service_hash, SERVICE_HASH_LEN);

		sd_attr->instance_id = 0x05;
		sd_attr->requester_instance_id = cur_if->pnan_info->instance_id;

		sd_attr->len = fixed_sd_attr_len + var_sd_attr_len;
	}

	cmd_len = cmd_len + sizeof(nan_sd_frame) +
		  sizeof(nan_service_descriptor_attr) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);
	LEAVE();
	return ret;
}

void nan_tx_unsolicited_publish_sdf(void *context)
{
	unsigned char mcast_mac[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = (struct mwu_iface_info *)context;
	int type = PUBLISH;
	ret = nan_tx_sdf(cur_if, type, mcast_mac);
	int announcement_period_in_us =
		cur_if->pnan_info->p_service.announcement_period * 1000;
	INFO("Announcement period is %d", announcement_period_in_us);

	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to tx sdf");
	} else {
		INFO("Tx SDF successful");
	}
}

enum nan_error
nan_start_unsolicited_publish(struct mwu_iface_info *cur_if,
			      struct nan_publish_service_conf *p_service)
{
	int ret = NAN_ERR_SUCCESS;
	int announcement_period_in_us = p_service->announcement_period * 1000;

	ERR("Announcement period is %d", announcement_period_in_us);

	nan_tx_unsolicited_publish_sdf((void *)cur_if);
	return ret;
}

void nan_tx_unsolicited_subscribe_sdf(void *context)
{
	unsigned char mcast_mac[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
	int ret = NAN_ERR_SUCCESS;
	struct mwu_iface_info *cur_if = (struct mwu_iface_info *)context;
	int type = SUBSCRIBE;
	ret = nan_tx_sdf(cur_if, type, mcast_mac);
	int query_period_in_us =
		cur_if->pnan_info->s_service.query_period * 1000;
	ERR("Query period is %d", query_period_in_us);

	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to tx sdf");
	} else {
		ERR("Tx SDF successful");
	}
}

enum nan_error
nan_start_unsolicited_subscribe(struct mwu_iface_info *cur_if,
				struct nan_subscribe_service *s_service)
{
	int ret = NAN_ERR_SUCCESS;
	int query_period_in_us = s_service->query_period * 1000;

	ERR("Query period is %d", query_period_in_us);

	nan_tx_unsolicited_subscribe_sdf((void *)cur_if);
	return ret;
}

u32 compute_bloom_filter_crc(u8 index, u8 *mac_address, u8 num_of_mac)
{
	u8 A_buff[300];
	memset(A_buff, 0, sizeof(A_buff));
	A_buff[0] = index;
	memcpy(&A_buff[1], &mac_address[0], num_of_mac * ETH_ALEN);
	return crc32(0xffffffff, A_buff, 1 + (num_of_mac * ETH_ALEN));
}

// parse the ranging  frame.
enum nan_error nan_parse_rx_rf(struct mwu_iface_info *cur_if, u8 *buffer,
			       int size,
			       struct nan_ranging_frame_attr *rx_rf_attr)
{
	u8 *tmp_ptr = buffer;
	int left_len = size;
	u8 attr_id;
	u16 len;

	nan_device_capability_attr *device_capa_attr =
		&rx_rf_attr->device_capability_attr;
	nan_element_container_attr *ele_cont_attr =
		&rx_rf_attr->element_container_attr;
	nan_ranging_info_attr *info_attr = &rx_rf_attr->ranging_info_attr;
	nan_ranging_setup_attr *setup_attr = &rx_rf_attr->ranging_setup_attr;
	nan_ranging_ftm_report_attr *ftm_report_attr =
		&rx_rf_attr->ranging_ftm_report_attr;
	u8 *time_bitmap_rsu = (u8 *)&rx_rf_attr->time_bitmap_rsu;
	peer_availability_info *peer_info;
	nan_availability_attr *avail_attr;

	/* first 6 bytes of buffer are peer_mac */
	memcpy(rx_rf_attr->peer_mac, tmp_ptr, ETH_ALEN);
	tmp_ptr = tmp_ptr + 6;
	left_len = left_len - 6;
	INFO("rx_rf->peer_mac: ");
	// print_mac(rx_rf_attr->peer_mac);
	INFO("\nrx_rf_attr->peer_mac: " UTIL_MACSTR,
	     UTIL_MAC2STR(rx_rf_attr->peer_mac));

	/* next 11 bytes are ttl(4),  category(1), action(1), OUI(3),
	 * OUI_Type(1) and oui_subtype so safe to skip them */
	tmp_ptr = tmp_ptr + 11;
	left_len = left_len - 11;

	rx_rf_attr->oui_subtype = *(tmp_ptr - 1);
	INFO("ost = %d", rx_rf_attr->oui_subtype);

	while (left_len > NAN_ATTR_HDR_LEN) {
		attr_id = *tmp_ptr;
		memcpy(&len, tmp_ptr + 1, sizeof(u16));
		len = wlan_le16_to_cpu(len);
		INFO("rx_rf->attr: 0x%02x", attr_id);
		INFO("rx_rf->attr_len: 0x%02x", len);

		switch (attr_id) {
		case NAN_RANGING_INFO_ATTR:
			INFO("Found ranging info attr");
			memcpy(info_attr, tmp_ptr,
			       sizeof(nan_ranging_info_attr));
			tmp_ptr = tmp_ptr + (len + NAN_ATTR_HDR_LEN);
			left_len = left_len - (len + NAN_ATTR_HDR_LEN);
			break;

		case NAN_RANGING_FTM_REPORT_ATTR:
			INFO("Found ranging report attribute");
			memcpy(ftm_report_attr, tmp_ptr,
			       sizeof(nan_ranging_ftm_report_attr));
			tmp_ptr = tmp_ptr + (len + NAN_ATTR_HDR_LEN);
			left_len = left_len - (len + NAN_ATTR_HDR_LEN);

			break;

		case NAN_RANGING_SETUP_ATTR:
			INFO("Found ranging setup attr");
			memcpy(setup_attr, tmp_ptr,
			       sizeof(nan_ranging_setup_attr));
			tmp_ptr += sizeof(nan_ranging_setup_attr);
			left_len = left_len - sizeof(nan_ranging_setup_attr);

			if (setup_attr->ranging_control.ftm_params_present) {
				memcpy(&rx_rf_attr->nan_ftm_params, tmp_ptr, 3);
				tmp_ptr += 3;
				left_len = left_len - 3;
			}

			if (setup_attr->ranging_control.bitmap_params_present) {
				// Parse the map_id
				memcpy(&rx_rf_attr->map_id_rsu, tmp_ptr, 1);
				tmp_ptr += 1;
				left_len = 1;

				// Parse the time_bitmap_control
				memcpy(&rx_rf_attr->time_btmp_ctrl_rsu, tmp_ptr,
				       2);
				tmp_ptr += 2;
				left_len = 2;

				// Parse the time_bitmap_len
				memcpy(&rx_rf_attr->time_bitmap_len_rsu,
				       tmp_ptr, 1);
				tmp_ptr += 1;
				left_len = 1;

				// Parse the time_bitmap
				memcpy(time_bitmap_rsu, tmp_ptr,
				       rx_rf_attr->time_bitmap_len_rsu);
				tmp_ptr += rx_rf_attr->time_bitmap_len_rsu;
				left_len = left_len -
					   rx_rf_attr->time_bitmap_len_rsu;
			}
			break;

		case NAN_DEVICE_CAPABILITY_ATTR:
			INFO("Found Device capability attr");
			memcpy(device_capa_attr, tmp_ptr,
			       sizeof(nan_device_capability_attr));
			tmp_ptr = tmp_ptr + (len + NAN_ATTR_HDR_LEN);
			left_len = left_len - (len + NAN_ATTR_HDR_LEN);
			break;

		case NAN_ELEMENT_CONTAINER_ATTR:
			INFO("Found Element container attr");
			ele_cont_attr->attribute_id = attr_id;
			ele_cont_attr->len = len;
			tmp_ptr = tmp_ptr + (len + NAN_ATTR_HDR_LEN);
			left_len = left_len - (len + NAN_ATTR_HDR_LEN);

			/*
				    memcpy(supported_rates, tmp_ptr,
			   sizeof(rx_rf_attr->supported_rates)); tmp_ptr +=
			   sizeof(rx_rf_attr->supported_rates); left_len =
			   left_len - sizeof(rx_rf_attr->supported_rates);

				    memcpy(ext_supported_rates, tmp_ptr,
			   sizeof(rx_rf_attr->ext_supported_rates)); tmp_ptr +=
			   sizeof(rx_rf_attr->ext_supported_rates); left_len =
			   left_len - sizeof(rx_rf_attr->ext_supported_rates);

				    memcpy(htcap, tmp_ptr,
			   sizeof(rx_rf_attr->htcap)); tmp_ptr +=
			   sizeof(rx_rf_attr->htcap); left_len = left_len -
			   sizeof(rx_rf_attr->htcap);

				    memcpy(htinfo, tmp_ptr,
			   sizeof(rx_rf_attr->htinfo)); tmp_ptr +=
			   sizeof(rx_rf_attr->htinfo); left_len = left_len -
			   sizeof(rx_rf_attr->htinfo);
			*/
			break;

		case NAN_AVAILABILITY_ATTR:

			INFO("Found NAN availability attr");
			avail_attr = (nan_availability_attr *)tmp_ptr;
			peer_info = &cur_if->pnan_info->peer_avail_info;
			memcpy(peer_info->peer_mac, rx_rf_attr->peer_mac,
			       ETH_ALEN);
			nan_parse_availability(avail_attr, peer_info);
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			break;

		default:
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			ERR("Attribute not parsed: attr_id %x, len %x", attr_id,
			    len);
			break;
		}
	}

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_rx_sdf(struct mwu_iface_info *cur_if, u8 *buffer,
				int size, struct nan_rx_sd_frame *rx_sdf,
				u8 sdf_type)
{
	u8 *tmp_ptr = buffer;
	int left_len = size;
	u8 attr_id, rx_sdf_type;
	u16 len;
	int i = 1;
	u8 *raw;
	FILE *fp = NULL;

	nan_fa_availability_entry_attr *e = &rx_sdf->fa.e;
	nan_ranging_attr *r = &rx_sdf->ranging;
	nan_fa_sd_attr *nansd = &rx_sdf->further_nan_sd;
	nan_fa_p2p_attr *nan_p2p_attr = &rx_sdf->further_nan_p2p;
	nan_service_descriptor_attr *rx_sda;
	nan_service_desc_ext_attr *sdea = &rx_sdf->desc_ext_attr;
	nan_device_capability_attr *dev_cap_attr = &rx_sdf->device_capa_attr;
	peer_availability_info *peer_info;
	nan_availability_attr *avail_attr = NULL;

	peer_info = &cur_if->pnan_info->peer_avail_info_published;

	mwu_hexdump(MSG_INFO, "tmp_ptr", tmp_ptr, 32);

	/* first 6 bytes of buffer are peer_mac */
	memcpy(rx_sdf->peer_mac, tmp_ptr, ETH_ALEN);
	tmp_ptr = tmp_ptr + 6;
	left_len = left_len - 6;
	INFO("rx_sdf->peer_mac: ");
	print_mac(rx_sdf->peer_mac);

	if (discovery_mac_written == 0) {
		raw = (u8 *)cur_if->device_mac_addr;
		system("rm /tmp/discovery_mac.txt");
		fp = fopen("/tmp/discovery_mac.txt", "w");
		fprintf(fp, "%02x:%02x:%02x:%02x:%02x:%02x",
			(unsigned int)raw[0], (unsigned int)raw[1],
			(unsigned int)raw[2], (unsigned int)raw[3],
			(unsigned int)raw[4], (unsigned int)raw[5]);
		fclose(fp);
		discovery_mac_written = 1;
	}

	/* next 4 bytes are for rssi */
	memcpy(&rx_sdf->rssi, tmp_ptr, sizeof(u32));
	rx_sdf->rssi = wlan_le32_to_cpu(rx_sdf->rssi);
	tmp_ptr = tmp_ptr + sizeof(u32);
	left_len = left_len - sizeof(u32);
	INFO("\nrx_sdf->rssi: %d", rx_sdf->rssi);

	/* next 6 bytes are category(1), action(1), OUI(3) and OUI_Type(1) so
	 * safe to skip them */
	tmp_ptr = tmp_ptr + 6;
	left_len = left_len - 6;

	while (left_len > NAN_ATTR_HDR_LEN) {
		attr_id = *tmp_ptr;
		memcpy(&len, tmp_ptr + 1, sizeof(u16));
		len = wlan_le16_to_cpu(len);
		// tmp_ptr = tmp_ptr + NAN_ATTR_HDR_LEN;
		// left_len = left_len - NAN_ATTR_HDR_LEN;

		INFO("rx_sdf->attr: 0x%02x", attr_id);
		INFO("rx_sdf->attr_len: 0x%02x", len);

		if (left_len < len) {
			WARN("Corrupt NAN IE, attr_id = 0x%x, len = %d, left = %d\n",
			     attr_id, len, left_len);
			return NAN_ERR_INVAL;
		}

		switch (attr_id) {
		case NAN_SERVICE_DISCRIPTOR_ATTR:
			rx_sda = (nan_service_descriptor_attr *)tmp_ptr;
			rx_sdf_type = rx_sda->service_control_bitmap &
				      3; /* last 2 bits of control btmap is
					    frame type */

			if (rx_sdf_type != sdf_type) {
				tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
				left_len = left_len - len - NAN_ATTR_HDR_LEN;
				break;
			}
			tmp_ptr = tmp_ptr + NAN_ATTR_HDR_LEN;
			left_len = left_len - NAN_ATTR_HDR_LEN;
			ERR("==================> Found a SD attribute %d", i++);
			memcpy(rx_sdf->service_hash, tmp_ptr, 6);
			tmp_ptr = tmp_ptr + 6;
			left_len = left_len - 6;
			mwu_hexdump(MSG_INFO, "rx_sdf->service_hash",
				    rx_sdf->service_hash, 6);

			rx_sdf->instance_id = *tmp_ptr;
			tmp_ptr = tmp_ptr + 1;
			left_len = left_len - 1;
			cur_if->pnan_info->instance_id =
				rx_sdf->instance_id; /*Temporary change Done for
							NAN2 PF4*/
			INFO("rx_sdf->instance_id: 0x%02x",
			     rx_sdf->instance_id);

			rx_sdf->requester_instance_id = *tmp_ptr;
			tmp_ptr = tmp_ptr + 1;
			left_len = left_len - 1;
			INFO("rx_sdf->req_instance_id: 0x%02x",
			     rx_sdf->requester_instance_id);

			rx_sdf->service_ctrl_btmap = *tmp_ptr;
			tmp_ptr = tmp_ptr + 1;
			left_len = left_len - 1;
			INFO("rx_sdf->sercice_control_btmap: 0x%02x",
			     rx_sdf->requester_instance_id);

			if (rx_sdf->service_ctrl_btmap & (1 << 6)) {
				memcpy(&rx_sdf->binding_btmap, tmp_ptr, 2);
				rx_sdf->binding_btmap =
					mwu_htons(rx_sdf->binding_btmap);
				tmp_ptr = tmp_ptr + 2;
				left_len = left_len - 2;
			}

			if (rx_sdf->service_ctrl_btmap & (1 << 2)) {
				rx_sdf->matching_filter_len = *tmp_ptr;
				tmp_ptr = tmp_ptr + 1;
				left_len = left_len - 1;
				memcpy(rx_sdf->matching_filter, tmp_ptr,
				       rx_sdf->matching_filter_len);
				tmp_ptr = tmp_ptr + rx_sdf->matching_filter_len;
				left_len =
					left_len - rx_sdf->matching_filter_len;
			}

			if (rx_sdf->service_ctrl_btmap & (1 << 3)) {
				rx_sdf->srf_len = *tmp_ptr;
				rx_sdf->srf.num_mac =
					((*tmp_ptr - 1) /*sizeof (srf_ctrl)*/ /
					 ETH_ALEN);
				tmp_ptr = tmp_ptr + 1;
				left_len = left_len - 1;

				rx_sdf->srf.srf_ctrl = *tmp_ptr;
				tmp_ptr = tmp_ptr + 1;
				left_len = left_len - 1;
				if (rx_sdf->srf.srf_ctrl & 1) {
					INFO("\nBloom filter present in rx frame");
					memcpy(rx_sdf->bloom_filter, tmp_ptr,
					       rx_sdf->srf_len - 1);
					mwu_hexdump(MSG_INFO,
						    "rx_sdf->bloom_filter",
						    rx_sdf->bloom_filter,
						    rx_sdf->srf_len - 1);
				} else {
					memcpy(rx_sdf->srf.mac_list, tmp_ptr,
					       rx_sdf->srf_len - 1); /* why -1 =
									to
									exclude
									the
									control
									bitmap
								      */
				}
				tmp_ptr = tmp_ptr + (rx_sdf->srf_len - 1);
				left_len = left_len - (rx_sdf->srf_len - 1);
			}

			if (rx_sdf->service_ctrl_btmap & (1 << 4)) {
				rx_sdf->service_info_len = *tmp_ptr;
				tmp_ptr = tmp_ptr + 1;
				left_len = left_len - 1;
				memcpy(rx_sdf->service_info, tmp_ptr,
				       rx_sdf->service_info_len);
				tmp_ptr = tmp_ptr + rx_sdf->service_info_len;
				left_len = left_len - rx_sdf->service_info_len;
			}
			break;

		case NAN_FURTHER_AVAIL_MAP_ATTR:
			tmp_ptr = tmp_ptr + NAN_ATTR_HDR_LEN;
			left_len = left_len - NAN_ATTR_HDR_LEN;
			INFO("Found further availability map attr");
			rx_sdf->fa.map_id = *tmp_ptr;
			tmp_ptr = tmp_ptr + 1;
			left_len = left_len - 1;
			INFO("rx_sdf->fa.map_id: 0x%02x", rx_sdf->fa.map_id);

			e = (nan_fa_availability_entry_attr *)tmp_ptr;
			INFO("entry e->ctrl: 0x%02x", e->ctrl);
			rx_sdf->fa.e.ctrl = e->ctrl;
			INFO("entry e->op_class: 0x%02x", e->op_class);
			rx_sdf->fa.e.op_class = e->op_class;
			INFO("entry e->op_channel: 0x%02x", e->chan_num);
			rx_sdf->fa.e.chan_num = e->chan_num;
			e->availability_btmp =
				wlan_le32_to_cpu(e->availability_btmp);
			INFO("entry e->availability_map: 0x%08x",
			     e->availability_btmp);
			rx_sdf->fa.e.availability_btmp = e->availability_btmp;
			tmp_ptr = tmp_ptr +
				  sizeof(nan_fa_availability_entry_attr);
			left_len = left_len -
				   sizeof(nan_fa_availability_entry_attr);
			rx_sdf->fa_found = 1;
			break;

		case NAN_RANGING_ATTR:
			INFO("Found ranging attr");
			r = (nan_ranging_attr *)tmp_ptr;
			INFO("mac r->mac");
			print_mac(r->mac);
			INFO("");
			INFO("r->map_ctrl: 0x%02x", r->map_ctrl);
			INFO("r->protocol: 0x%02x", r->proto);
			r->availability_btmp =
				wlan_le32_to_cpu(e->availability_btmp);
			INFO("r->availability_map: 0x%08x",
			     e->availability_btmp);
			tmp_ptr = tmp_ptr + sizeof(nan_ranging_attr);
			left_len = left_len - sizeof(nan_ranging_attr);
			break;

		case NAN_FURTHER_SD_ATTR:
			INFO("Found further NAN SD attr");
			nansd = (nan_fa_sd_attr *)tmp_ptr;
			INFO("nansd->map_ctrl: 0x%02x", nansd->map_ctrl);
			nansd->availability_btmp =
				wlan_le32_to_cpu(nansd->availability_btmp);
			INFO("nansd->availability_map: 0x%08x",
			     nansd->availability_btmp);
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			break;

		case NAN_FURTHER_P2P_ATTR:
			INFO("Found further NAN P2P attr");
			memcpy(nan_p2p_attr, tmp_ptr, sizeof(nan_fa_p2p_attr));
			mwu_hexdump(MSG_ERROR, "P2P ATTR", (u8 *)nan_p2p_attr,
				    sizeof(nan_fa_p2p_attr));
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			break;

		case SERVICE_DESC_EXT_ATTR:
			INFO("Found SERVICE_DESC_EXT_ATTR");
			memcpy(sdea, tmp_ptr,
			       sizeof(nan_service_desc_ext_attr));
			INFO("sdea->control %d", sdea->control);
			if ((sdea->control & SDEA_CTRL_RANGING_REQUIRED) ==
			    SDEA_CTRL_RANGING_REQUIRED)
				peer_info->peer_ranging_required = TRUE;
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			break;

		case NAN_AVAILABILITY_ATTR:
			INFO("Found NAN availability attr");
			avail_attr = (nan_availability_attr *)tmp_ptr;
			nan_parse_availability(avail_attr, peer_info);
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			break;

		case NAN_DEVICE_CAPA_ATTR:
			INFO("Found Device capability attr");
			mwu_hexdump(MSG_ERROR, "dev_cap_attr", tmp_ptr,
				    (len + NAN_ATTR_HDR_LEN));
			memcpy(dev_cap_attr, tmp_ptr, (len + NAN_ATTR_HDR_LEN));
			if (dev_cap_attr->supported_bands == 0x04) {
				peer_info->single_band = TRUE;
				INFO("Found single band peer");
			}

			if (dev_cap_attr->capabilities.ndpe_attr_supported) {
				peer_info->ndpe_attr_supported = TRUE;
				INFO("Peer supports NDPE attribute");
			}

			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			break;

		default:
			tmp_ptr = tmp_ptr + len + NAN_ATTR_HDR_LEN;
			left_len = left_len - len - NAN_ATTR_HDR_LEN;
			ERR("Attribute not parsed: attr_id %x, len %x", attr_id,
			    len);
			break;
		}
	}

	if (i < 2)
		return NAN_ERR_UNSUPPORTED;

	return NAN_ERR_SUCCESS;
}

/* this is a improved version of match filter control function
 * this takes care multiple matching filters.*/
int nan_match_filter_ext(char *mf1, char *mf2, int mf1_len, int mf2_len)
{
	int lens, lend;
	int i = 0, match = 0;

	ERR("==> mf1 len is %d", mf1_len);
	ERR("==> mf2 len is %d", mf2_len);

	while (mf1_len && mf2_len) {
		ERR("\nComparing matching filter %d", i);
		lens = *mf1;
		lend = *mf2;
		mwu_hexdump(MSG_ERROR, "mf1", (u8 *)mf1, lens + 1);
		mwu_hexdump(MSG_ERROR, "mf2", (u8 *)mf2, lend + 1);
		/* compare lengths */
		if (lens != lend) {
			/* unequal length indicate match filter mismatch
			 * exception 0 length is wildcard match */
			if (lens == 0 || lend == 0) {
				ERR("one of the filters is 0, consider positive match..");
				match = 1;
				mf1 = mf1 + lens + 1; /* move to next filter
							 <l.v> pair */
				mf2 = mf2 + lend + 1;
				mf1_len = mf1_len - (lens + 1);
				mf2_len = mf2_len - (lend + 1);
				// ERR("All further filters are wildcart
				// match...");
				match = 1;
				continue;
			}
			ERR("No match because of length mismatch");
			match = 0;
			break;
		}

		/* compare vals */
		if (memcmp(mf1 + 1, mf2 + 1, lens) == 0) {
			match = 1;
			mf1 = mf1 + lens + 1; /* move to next filter <l.v> pair
					       */
			mf2 = mf2 + lend + 1;
			ERR("Filter %d match successful", i);
			mf1_len = mf1_len - (lens + 1);
			mf2_len = mf2_len - (lend + 1);

			continue;
		} else {
			match = 0;
			break;
		}
	}

	if (mf1_len != 0) {
		if (*mf1 == 0) /* this means mf1 is only wildcard filter
				  configured */
			match = 1;
		else
			match = 0;
	}
#if 0
    if (mf2_len != 0) {
        ERR("Affected by a fixxxx!!!");
        if (*mf2 == 0) /* this means mf2 is only wildcard filter configured */
            match = 1;
        else
            match = 0;
    }
#endif
	return match;
}

int is_mac_in_srf(unsigned char *mac, struct nan_srf *srf)
{
	int i = 0;
	for (i = 0; i < srf->num_mac; i++) {
		if (memcmp(mac, srf->mac_list[i], ETH_ALEN) == 0) {
			INFO("MAC found in SRF MAC list at pos %d", i);
			return 1;
		}
	}
	ERR("Peer mac is not in mac list of SRF");
	return 0;
}

enum nan_error nan_tx_ranging_result_frame(struct mwu_iface_info *cur_if,
					   u8 oui_sub_type, u8 reason,
					   u32 range_rslt, unsigned char *dest)
{
	int ret;
	u8 *var_attr_ptr;
	nan_action_frame *rf_buf = NULL;
	mrvl_cmd_head_buf *cmd;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_attr_len = sizeof(nan_action_frame);

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */

	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	rf_buf = (nan_action_frame *)cmd->cmd_data;
	memcpy(rf_buf->peer_mac_addr, dest, ETH_ALEN);
	print_mac(rf_buf->peer_mac_addr);
	rf_buf->tx_type = 3;
	rf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	rf_buf->action = 0x09;
	rf_buf->oui[0] = 0x50;
	rf_buf->oui[1] = 0x6F;
	rf_buf->oui[2] = 0x9A;
	rf_buf->oui_type = NAN_ACTION_FRAME;
	rf_buf->oui_sub_type = oui_sub_type;

	if (oui_sub_type == RANGING_REPORT) {
		var_attr_ptr = (u8 *)&rf_buf->information_content[0];
		nan_ranging_ftm_report_attr ftm_report_attr;
		ftm_report_attr.attribute_id = NAN_RANGING_FTM_REPORT_ATTR;
		ftm_report_attr.len =
			sizeof(nan_ranging_info_attr) + 17 - NAN_ATTR_HDR_LEN;

		ftm_range_report range_report;
		range_report.range_entry_count = 1;
		range_report.range_entry.range = (range_rslt / 4096);
		range_report.error_entry_count = 0;

		memcpy(&range_report.range_entry.bssid, cur_if->device_mac_addr,
		       ETH_ALEN);

		memcpy(var_attr_ptr, &ftm_report_attr,
		       sizeof(nan_ranging_ftm_report_attr));
		var_attr_len += sizeof(nan_ranging_ftm_report_attr);
		var_attr_ptr += sizeof(nan_ranging_ftm_report_attr);

		memcpy(var_attr_ptr, &range_report, 17);
		var_attr_len += 17;
		var_attr_ptr += 17;

		INFO("Ranging Report");
		mwu_hexdump(MSG_INFO, "ftm_report_attr", (u8 *)&ftm_report_attr,
			    20);
	} else if (oui_sub_type == RANGING_TERMINATION) {
		var_attr_ptr = (u8 *)&rf_buf->information_content[0];

		// Ranging setup attr

		nan_ranging_setup_attr *setup_attr;
		u8 ranging_ctrl = 0;
		setup_attr = (nan_ranging_setup_attr *)var_attr_ptr;
		setup_attr->attribute_id = NAN_RANGING_SETUP_ATTR;
		memcpy(&setup_attr->ranging_control, &ranging_ctrl, 1);
		setup_attr->dialog_token = 0; // zero for termination frame
		setup_attr->reason_code = reason;
		if (reason > NAN_REASON_CODE_UNSPECIFIED) {
			setup_attr->type_and_status = 0x12; // Termination due
							    // to some
							    // rejection.
		} else {
			setup_attr->type_and_status = 0x02; // Normal
							    // termination
		}

		var_attr_len += sizeof(nan_ranging_setup_attr);
		var_attr_ptr += sizeof(nan_ranging_setup_attr);
		setup_attr->len =
			sizeof(nan_ranging_setup_attr) - NAN_ATTR_HDR_LEN;

		INFO("Ranging Termination");
	}

	cmd_len = cmd_len + var_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;

	mwu_hexdump(MSG_ERROR, "RANGE RESULT", cmd->cmd_data, cmd_len);
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	FREE(mrvl_cmd);
	LEAVE();
	return ret;
}

enum nan_error
nan_handle_ranging_response_frame(struct mwu_iface_info *cur_if,
				  struct nan_ranging_frame_attr *rf_attr_frame)
{
	avail_entry_t *peer_entry = NULL;
	u32 ranging_resp_bitmap;

	ENTER();

	// Check if the peer has rejected the ranging request
	if (rf_attr_frame->ranging_setup_attr.type_and_status == 0x11) {
		rf_attr_frame->rejected = 1;
		return NAN_ERR_SUCCESS;
	}

	// Check if the schedule is suitable

	if (cur_if->pnan_info->peer_avail_info.committed_valid & ENTRY0_VALID)
		peer_entry =
			&cur_if->pnan_info->peer_avail_info.entry_committed[0];

	ranging_resp_bitmap = peer_entry->combined_time_bitmap;

	if ((ranging_resp_bitmap == 0) &&
	    (cur_if->pnan_info->peer_avail_info.committed_valid &
	     ENTRY1_VALID)) {
		peer_entry =
			&cur_if->pnan_info->peer_avail_info.entry_committed[1];
		ranging_resp_bitmap = peer_entry->combined_time_bitmap;
	}

	if (ranging_resp_bitmap == 0) {
		rf_attr_frame->rejected = 1;
		rf_attr_frame->ranging_setup_attr.reason_code =
			NAN_REASON_CODE_INVALID_AVAILABILITY;
	} else {
		cur_if->pnan_info->self_avail_info.seq_id++;
		cur_if->pnan_info->self_avail_info.committed_valid |=
			ENTRY0_VALID;
		cur_if->pnan_info->self_avail_info.entry_committed[0].op_class =
			peer_entry->op_class;
		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.channels[0] = peer_entry->channels[0];
		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.time_bitmap[0] = ranging_resp_bitmap;

		// Update the potential entry
		cur_if->pnan_info->self_avail_info.entry_potential[0]
			.time_bitmap[0] &= ~(ranging_resp_bitmap);
	}

	// Check if the ftm parameters received in the response are suitable
	if ((rf_attr_frame->nan_ftm_params.max_burst_dur > 9) ||
	    (rf_attr_frame->nan_ftm_params.max_burst_dur == 0)) {
		rf_attr_frame->rejected = 1;
		rf_attr_frame->ranging_setup_attr.reason_code =
			NAN_REASON_CODE_FTM_PARAM_INCAPABLE;
	}
	if ((rf_attr_frame->nan_ftm_params.min_delta_ftm > 10) ||
	    (rf_attr_frame->nan_ftm_params.min_delta_ftm == 0)) {
		rf_attr_frame->rejected = 1;
		rf_attr_frame->ranging_setup_attr.reason_code =
			NAN_REASON_CODE_FTM_PARAM_INCAPABLE;
	}
	if ((rf_attr_frame->nan_ftm_params.max_ftms_per_burst > 12) ||
	    (rf_attr_frame->nan_ftm_params.max_ftms_per_burst == 0)) {
		rf_attr_frame->rejected = 1;
		rf_attr_frame->ranging_setup_attr.reason_code =
			NAN_REASON_CODE_FTM_PARAM_INCAPABLE;
	}

	LEAVE();
	return NAN_ERR_SUCCESS;
}

enum nan_error
nan_tx_ranging_response_frame(struct mwu_iface_info *cur_if,
			      struct nan_ranging_frame_attr *rf_attr_frame)
{
	int ret;
	u8 *var_attr_ptr;
	nan_action_frame *rf_buf = NULL;
	mrvl_cmd_head_buf *cmd;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_attr_len = sizeof(nan_action_frame);
	rf_attr_frame->rejected = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */

	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	rf_buf = (nan_action_frame *)cmd->cmd_data;
	memcpy(rf_buf->peer_mac_addr, rf_attr_frame->peer_mac, ETH_ALEN);

	rf_buf->tx_type = 3;
	rf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	rf_buf->action = 0x09;
	rf_buf->oui[0] = 0x50;
	rf_buf->oui[1] = 0x6F;
	rf_buf->oui[2] = 0x9A;
	rf_buf->oui_type = NAN_ACTION_FRAME;
	rf_buf->oui_sub_type = RANGING_RESPONSE;

	INFO("Added Frame headers");

	var_attr_ptr = (u8 *)&rf_buf->information_content[0];

	// NAN availability attr

	nan_availability_attr *availability_attr;
	nan_availability_list *entry;
	nan_channel_entry_list *chan_list;
	u8 opt_fields_len = 7;
	u8 sz_of_entry_len = sizeof(u16);
	u32 ranging_resp_bitmap = 0;
	avail_entry_t *peer_entry;
	availability_attr = (nan_availability_attr *)var_attr_ptr;
	availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
	availability_attr->attr_ctrl.map_id =
		cur_if->pnan_info->self_avail_info.map_id;
	availability_attr->seq_id =
		cur_if->pnan_info->self_avail_info.seq_id + 1;

	entry = availability_attr->entry;
	entry->entry_ctrl.avail_type = NDP_AVAIL_TYPE_COMMITTED;
	entry->entry_ctrl.usage_preference = 1;
	entry->entry_ctrl.utilization = 0;
	entry->entry_ctrl.rx_nss = 1;
	entry->entry_ctrl.time_bitmap_present = 1;

	/*Populate optional fields*/
	{
		time_bitmap_control *time_bitmap_ctrl =
			(time_bitmap_control *)&entry->optional[0];
		u8 *time_bitmap_len = &entry->optional[2];
		u8 *time_bitmap = &entry->optional[3];

		time_bitmap_ctrl->bit_duration = NDP_TIME_BM_DUR_16;
		time_bitmap_ctrl->bit_period = NDP_TIME_BM_PERIOD_512;
		time_bitmap_ctrl->start_offset = 0;
		*time_bitmap_len = 4; /*time_bitmap_len*/
		// Respond with the ranging schedule taking into consideration
		// the peer's bitmap

		cur_if->pnan_info->self_avail_info.entry_potential[0]
			.time_bitmap[0] = NAN_POTENTIAL_BITMAP;

		if (cur_if->pnan_info->peer_avail_info.committed_valid &
		    ENTRY1_VALID) {
			peer_entry = &cur_if->pnan_info->peer_avail_info
					      .entry_committed[1];
			ranging_resp_bitmap =
				(cur_if->pnan_info->self_avail_info
					 .entry_potential[0]
					 .time_bitmap[0] &
				 peer_entry->combined_time_bitmap);
		}
		if ((ranging_resp_bitmap == 0) &&
		    (cur_if->pnan_info->peer_avail_info.committed_valid &
		     ENTRY0_VALID)) {
			peer_entry = &cur_if->pnan_info->peer_avail_info
					      .entry_committed[0];
			ranging_resp_bitmap =
				(cur_if->pnan_info->self_avail_info
					 .entry_potential[0]
					 .time_bitmap[0] &
				 peer_entry->combined_time_bitmap);
		}
		if ((ranging_resp_bitmap == 0) &&
		    (cur_if->pnan_info->peer_avail_info.conditional_valid &
		     ENTRY1_VALID)) {
			peer_entry = &cur_if->pnan_info->peer_avail_info
					      .entry_conditional[1];
			ranging_resp_bitmap =
				(cur_if->pnan_info->self_avail_info
					 .entry_potential[0]
					 .time_bitmap[0] &
				 peer_entry->combined_time_bitmap);
		}
		if ((ranging_resp_bitmap == 0) &&
		    (cur_if->pnan_info->peer_avail_info.conditional_valid &
		     ENTRY0_VALID)) {
			peer_entry = &cur_if->pnan_info->peer_avail_info
					      .entry_conditional[0];
			ranging_resp_bitmap =
				(cur_if->pnan_info->self_avail_info
					 .entry_potential[0]
					 .time_bitmap[0] &
				 peer_entry->combined_time_bitmap);
		}
		memcpy(time_bitmap, &ranging_resp_bitmap, 4); /*time bitmap*/
	}

	/*Hop over by 5 bytes(time_bitmap_len + time_bitmap) to get chan list */
	chan_list = (nan_channel_entry_list *)(var_attr_ptr +
					       sizeof(nan_availability_attr) +
					       sizeof(nan_availability_list) +
					       opt_fields_len);
	chan_list->entry_ctrl.entry_type = 1;
	chan_list->entry_ctrl.band_type = 0;
	chan_list->entry_ctrl.num_entries = 1;
	chan_list->chan_band_entry.chan_entry.op_class = peer_entry->op_class;
	chan_list->chan_band_entry.chan_entry.chan_bitmap = ndp_get_chan_bitmap(
		peer_entry->op_class, peer_entry->channels[0]);
	ERR("op_class= %d chan_bitmap %d\n",
	    chan_list->chan_band_entry.chan_entry.op_class,
	    chan_list->chan_band_entry.chan_entry.chan_bitmap);
	chan_list->chan_band_entry.chan_entry.primary_chan_bitmap = 0x0;
	entry->len = sizeof(nan_channel_entry_list) +
		     sizeof(nan_availability_list) + opt_fields_len -
		     sz_of_entry_len;
	availability_attr->len = sizeof(nan_availability_attr) -
				 NAN_ATTR_HDR_LEN + entry->len +
				 sz_of_entry_len;
	var_attr_ptr +=
		entry->len + sizeof(nan_availability_attr) + sz_of_entry_len;
	var_attr_len +=
		entry->len + sizeof(nan_availability_attr) + sz_of_entry_len;
	// Copy the final ranging schedule in the self_avail_info's committed
	// entry
	cur_if->pnan_info->self_avail_info.seq_id++;
	cur_if->pnan_info->self_avail_info.committed_valid |= ENTRY0_VALID;
	cur_if->pnan_info->self_avail_info.entry_committed[0].op_class =
		peer_entry->op_class;
	cur_if->pnan_info->self_avail_info.entry_committed[0].channels[0] =
		peer_entry->channels[0];
	cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap[0] =
		ranging_resp_bitmap;
	// Update the potential entry
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] &=
		~(ranging_resp_bitmap);

	INFO("NAN availability attr");

	// Device capability attr
	nan_device_capability_attr device_capa_attr;

	memset(&device_capa_attr, 0, sizeof(device_capa_attr));

	device_capa_attr.attribute_id = NAN_DEVICE_CAPA_ATTR;
	device_capa_attr.len =
		sizeof(nan_device_capability_attr) - NAN_ATTR_HDR_LEN;
	device_capa_attr.committed_dw_info._2g_dw =
		cur_if->pnan_info->awake_dw_interval; // all slots on 2.4GHz
	if (cur_if->pnan_info->a_band)
		device_capa_attr.committed_dw_info._5g_dw = 1;
	device_capa_attr.supported_bands =
		cur_if->pnan_info->a_band ? 0x14 : 0x04;

	u8 operation_mode = 0x00;
	memcpy(&device_capa_attr.op_mode, &operation_mode, 1);
	device_capa_attr.no_of_antennas = 0x0;
	device_capa_attr.max_chan_sw_time = 5000;

	memcpy(var_attr_ptr, &device_capa_attr,
	       sizeof(nan_device_capability_attr));
	var_attr_len += sizeof(nan_device_capability_attr);
	var_attr_ptr += sizeof(nan_device_capability_attr);
	INFO("Device capability attr");

	// Element container attr
	u8 supported_rates[6] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
	u8 ext_supported_rates[10] = {0x32, 0x08, 0x0c, 0x12, 0x18,
				      0x24, 0x30, 0x48, 0x60, 0x6c};
	u8 htcap[28] = {0x2d, 0x1a, 0xee, 0x01, 0x1f, 0xff, 0xff,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	u8 htinfo[24] = {0x3d, 0x16, 0x06, 0x05, 0x15, 0x00, 0x00, 0x00,
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	u8 *elem_ptr;
	u16 elem_len = 0;

	nan_element_container_attr *container_attr;
	container_attr = (nan_element_container_attr *)var_attr_ptr;
	elem_ptr = (u8 *)(&container_attr->elements[0]);
	container_attr->attribute_id = NAN_ELEMENT_CONTAINER_ATTR;
	container_attr->len = sizeof(supported_rates) +
			      sizeof(ext_supported_rates) + sizeof(htcap) +
			      sizeof(htinfo) + 1;
	memcpy(elem_ptr, supported_rates, sizeof(supported_rates));
	elem_ptr += sizeof(supported_rates);
	elem_len += sizeof(supported_rates);

	memcpy(elem_ptr, ext_supported_rates, sizeof(ext_supported_rates));
	elem_ptr += sizeof(ext_supported_rates);
	elem_len += sizeof(ext_supported_rates);

	memcpy(elem_ptr, htcap, sizeof(htcap));
	elem_ptr += sizeof(htcap);
	elem_len += sizeof(htcap);

	memcpy(elem_ptr, htinfo, sizeof(htinfo));
	elem_ptr += sizeof(htinfo);
	elem_len += sizeof(htinfo);

	var_attr_len += sizeof(nan_element_container_attr) + elem_len;
	var_attr_ptr += sizeof(nan_element_container_attr) + elem_len;
	INFO("Element container attr");

	// Ranging info attr

	// TODO location_info to be set accordingly
	nan_ranging_info_attr info_attr;

	info_attr.location_info = 0;
	// info_attr.last_movement_indication = 0;
	info_attr.attribute_id = NAN_RANGING_INFO_ATTR;
	info_attr.len = sizeof(nan_ranging_info_attr) - NAN_ATTR_HDR_LEN;

	memcpy(var_attr_ptr, &info_attr, sizeof(nan_ranging_info_attr));
	var_attr_len += sizeof(nan_ranging_info_attr);
	var_attr_ptr += sizeof(nan_ranging_info_attr);
	INFO("Ranging info attr");

	// Ranging setup attr

	nan_ranging_setup_attr *setup_attr;
	setup_attr = (nan_ranging_setup_attr *)var_attr_ptr;
	setup_attr->attribute_id = NAN_RANGING_SETUP_ATTR;
	setup_attr->dialog_token =
		rf_attr_frame->ranging_setup_attr.dialog_token;
	setup_attr->type_and_status = 0x01; // Response and Accepted, this will
					    // be updated below in case the
					    // response is to be rejected.
	if (ranging_resp_bitmap == 0) {
		setup_attr->type_and_status = 0x11; // Response and Rejected
		rf_attr_frame->rejected = 1;
		setup_attr->reason_code = NAN_REASON_CODE_RESOURCE_LIMITATION;
		/*
		    } else if ((peer_entry->op_class != DEFAULT_2G_OP_CLASS) ||
		   (peer_entry->channels[0] != DEFAULT_2G_OP_CHAN)){

			setup_attr->type_and_status = 0x11; // Response and
		   Rejected rf_attr_frame->rejected = 1; setup_attr->reason_code
		   = NAN_REASON_CODE_INVALID_AVAILABILITY;
		*/
	} else if (rf_attr_frame->ranging_setup_attr.ranging_control
			   .ftm_params_present) {
		// check the received FTM parameters
		if (rf_attr_frame->nan_ftm_params.max_burst_dur > 9)
			rf_attr_frame->nan_ftm_params.max_burst_dur = 9;
		if (rf_attr_frame->nan_ftm_params.min_delta_ftm > 10)
			rf_attr_frame->nan_ftm_params.min_delta_ftm = 10;
		if (rf_attr_frame->nan_ftm_params.max_ftms_per_burst > 12)
			rf_attr_frame->nan_ftm_params.max_ftms_per_burst = 12;
	}

	setup_attr->ranging_control.report_required = 0x1; // Ranging report
							   // required bit set

	var_attr_len += sizeof(nan_ranging_setup_attr);
	var_attr_ptr += sizeof(nan_ranging_setup_attr);
	setup_attr->len = sizeof(nan_ranging_setup_attr) - NAN_ATTR_HDR_LEN;

	if (rf_attr_frame->ranging_setup_attr.ranging_control
		    .ftm_params_present) {
		// Propose valid values in case some zeros are received
		setup_attr->ranging_control.ftm_params_present = 0x1;
		if (rf_attr_frame->nan_ftm_params.max_burst_dur == 0)
			rf_attr_frame->nan_ftm_params.max_burst_dur = 0x3;
		if (rf_attr_frame->nan_ftm_params.min_delta_ftm == 0)
			rf_attr_frame->nan_ftm_params.min_delta_ftm = 0x2;
		if (rf_attr_frame->nan_ftm_params.max_ftms_per_burst == 0)
			rf_attr_frame->nan_ftm_params.max_ftms_per_burst = 0x3;

		memcpy(var_attr_ptr, &rf_attr_frame->nan_ftm_params, 3);
		var_attr_len += 3;
		var_attr_ptr += 3;
		setup_attr->len += 3;
	}

	setup_attr->ranging_control.bitmap_params_present = 0x1;
	// copy the time bitmap fields from the NAN availability attribute.

	// Add the map_id field
	u8 map_id = 0;
	map_id = availability_attr->attr_ctrl.map_id;
	memcpy(var_attr_ptr, &map_id, 1);
	var_attr_len += 1;
	var_attr_ptr += 1;
	setup_attr->len += 1;

	// Time bitmap ctrl
	time_bitmap_control time_bitmap_ctrl;
	time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
	time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
	time_bitmap_ctrl.start_offset = 0;
	time_bitmap_ctrl.reserved = 0;

	memcpy(var_attr_ptr, &time_bitmap_ctrl, sizeof(time_bitmap_control));
	var_attr_len += sizeof(time_bitmap_control);
	var_attr_ptr += sizeof(time_bitmap_control);
	setup_attr->len += sizeof(time_bitmap_control);

	u8 time_btmp_len;
	time_btmp_len = 4;

	// Time bitmap length
	memcpy(var_attr_ptr, &time_btmp_len, 1);
	var_attr_len += 1;
	var_attr_ptr += 1;
	setup_attr->len += 1;

	// Add the time_bitmap
	memcpy(var_attr_ptr, &ranging_resp_bitmap, 4);

	var_attr_len += 4;
	var_attr_ptr += 4;
	setup_attr->len += 4;

	INFO("Ranging setup attr");

	cmd_len = cmd_len + var_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	mwu_hexdump(MSG_ERROR, "RANGE RESP", cmd->cmd_data, var_attr_len);
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);
	LEAVE();

	return ret;
}

enum nan_error nan_process_and_send_ftm_init(NAN_FTM_PARAMS *ftm)
{
	struct mlocation_cfg cfg;
	static struct module mlocation_mod;
	int status = NAN_ERR_SUCCESS;

	ENTER();

	memset(&cfg, 0, sizeof(cfg));

	cfg.burst_duration = ftm->max_burst_dur;
	cfg.min_delta = ftm->min_delta_ftm;
	cfg.mlocation_per_burst = ftm->max_ftms_per_burst;
	cfg.bw = ftm->ftm_fmt_bw;
	cfg.asap = 1;

	ERR("burst_duration = %d\n", cfg.burst_duration);
	ERR("min_delta= %d\n", cfg.min_delta);
	ERR("mlocation_per_burst= %d\n", cfg.mlocation_per_burst);
	ERR("bw= %d\n", cfg.bw);

	/* prepare the module struct */
	strncpy(mlocation_mod.iface, "nan0", IFNAMSIZ);
	mlocation_mod.cb = NULL;
	mlocation_mod.cbpriv = NULL;
	ERR("Calling mlocation_init\n");
	status = mlocation_init(&mlocation_mod, &cfg);
	if (status == MLOCATION_ERR_SUCCESS) {
		ERR("mlocation_init SUCCESS\n");
	} else {
		ERR("mlocation_init FAIL, status = %d\n", status);
	}

	LEAVE();

	return status;
}

enum nan_error nan_start_ftm_session(unsigned char *mac)
{
	struct mlocation_session mlocations;
	static struct module mlocation_mod;
	int status = NAN_ERR_SUCCESS;

	ENTER();

	memset(&mlocations, 0, sizeof(struct mlocation_session));

	mlocations.action = 4; // Unassociated FTM Session
	INFO("rx_rf->peer_mac: ");
	print_mac(mac);
	memcpy(mlocations.mac, mac, ETH_ALEN);
	mlocations.channel = 0;

	mwu_hexdump(MSG_INFO, "mlocation_session", (u8 *)&mlocations,
		    sizeof(struct mlocation_session));

	INFO("mlocations.action : %d", mlocations.action);
	// INFO("mlocations.mac: %s", UTIL_MAC2STR(mlocations.peer_mac));

	/* prepare the module struct */
	strncpy(mlocation_mod.iface, "nan0", IFNAMSIZ);
	mlocation_mod.cb = nan_mwu_event_ftm_cb;
	mlocation_mod.cbpriv = &mlocation_mod;
	ERR("Calling mlocation_init\n");

	status = do_mlocation_session_ctrl(&mlocation_mod, &mlocations, TRUE);

	if (status == MLOCATION_ERR_SUCCESS) {
		ERR("mlocation_session_ctrl SUCCESS\n");
	} else {
		ERR("mlocation_session_ctrl FAIL, status = %d\n", status);
	}

	LEAVE();

	return status;
}

enum nan_error nan_set_ranging_bitmap(struct mwu_iface_info *cur_if,
				      u32 ranging_bitmap)
{
	struct nan_params_fa fa;
	int status = NAN_ERR_SUCCESS;
	avail_entry_t *peer_entry;

	ENTER();

	peer_entry =
		&cur_if->pnan_info->self_avail_info.entry_committed[0]; // Final
									// schedule
									// is in
									// the
									// committed
									// entry.

	fa.availability_map = ranging_bitmap;
	fa.interval = 16;
	fa.repeat_entry = 255;
	fa.op_class = peer_entry->op_class;
	fa.op_chan = peer_entry->channels[0];

	status = nancmd_set_ranging_bitmap(cur_if, &fa);
	if (status != NAN_ERR_SUCCESS) {
		ERR("nancmd_set_ranging_bitmap failed");
	} else {
		ERR("nancmd_set_ranging_bitmap success");
	}

	LEAVE();

	return status;
}

enum nan_error nan_handle_rx_ranging(struct mwu_iface_info *cur_if,
				     unsigned char *buffer, int size)
{
	struct nan_ranging_frame_attr rf_attr_frame;
	NAN_FTM_PARAMS ftm;
	int ret = NAN_ERR_SUCCESS;

	ENTER();

	memset(&rf_attr_frame, 0, sizeof(struct nan_ranging_frame_attr));
	ret = nan_parse_rx_rf(cur_if, buffer, size, &rf_attr_frame);

	// if it was a ranging request.
	if (rf_attr_frame.oui_subtype == RANGING_REQUEST) {
		// Set that this is the ranging responder side
		cur_if->pnan_info->is_nan_ranging_responder = 1;

		ret = nan_tx_ranging_response_frame(cur_if, &rf_attr_frame);
		if (ret != NAN_ERR_SUCCESS) {
			ERR("Failed to send ranging response frame");
			return NAN_ERR_INVAL;
		} else {
			INFO("Tx Range response success");
		}

		if (rf_attr_frame.rejected == 0) {
			// Initialize FTM params
			memcpy(&ftm, &rf_attr_frame.nan_ftm_params, 3);
			ret = nan_process_and_send_ftm_init(&ftm);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to  process ftm init");
				return NAN_ERR_INVAL;
			} else {
				ERR("Process ftm init success");
			}

			ret = nan_set_ranging_bitmap(
				cur_if, rf_attr_frame.time_bitmap_rsu);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to  set ranging bitmap");
				return NAN_ERR_INVAL;
			} else {
				ERR("Set ranging bitmap success");
			}
		}
		cur_if->pnan_info->is_nan_ranging_responder = 0;
	}
	// if it was a ranging response.
	else if (rf_attr_frame.oui_subtype == RANGING_RESPONSE) {
		INFO("Ranging response parsed");

		ret = nan_handle_ranging_response_frame(cur_if, &rf_attr_frame);

		if (rf_attr_frame.rejected == 0) {
			// Check for error status and init session
			ret = nan_set_ranging_bitmap(
				cur_if, rf_attr_frame.time_bitmap_rsu);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to  set ranging bitmap");
				return NAN_ERR_INVAL;
			} else {
				ERR("Set ranging bitmap success");
			}

			INFO("Initiating FTM session");
			ret = nan_start_ftm_session(rf_attr_frame.peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to start FTM session");
			} else {
				ERR("FTM session start success");
			}

		} else {
			// send ranging termination to the peer
			ret = nan_tx_ranging_result_frame(
				cur_if, RANGING_TERMINATION,
				rf_attr_frame.ranging_setup_attr.reason_code, 0,
				rf_attr_frame.peer_mac);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP terminate");
				return ret;
			}
			nan_set_ranging_bitmap(cur_if, BITMAP_RESET);
		}
	}

	// if it was a ranging result
	else if (rf_attr_frame.oui_subtype == RANGING_REPORT) {
		INFO("Ranging result received");
	}

	// if it was a ranging termination
	else if (rf_attr_frame.oui_subtype == RANGING_TERMINATION) {
		nan_set_ranging_bitmap(cur_if, BITMAP_RESET);
		INFO("Ranging termination received");
	}

	LEAVE();
	return ret;
}

enum nan_error nan_handle_rx_publish(struct mwu_iface_info *cur_if,
				     unsigned char *buffer, int size)
{
	struct subscribed_service *psrvc = &cur_if->pnan_info->s_service;
	struct nan_rx_sd_frame rx_sd_frame;
	int ret = NAN_ERR_SUCCESS;

	memset(&rx_sd_frame, 0, sizeof(struct nan_rx_sd_frame));

	ret = nan_parse_rx_sdf(cur_if, buffer, size, &rx_sd_frame,
			       SERVICE_CTRL_BITMAP_PUBLISH);

	/* check if service hash matches to the one we are looking for */
	if (memcmp(psrvc->service_hash, rx_sd_frame.service_hash, ETH_ALEN) !=
	    0) {
		mwu_hexdump(MSG_INFO, "Sebscribed service Hash -",
			    psrvc->service_hash, 6);
		mwu_hexdump(MSG_INFO, "Received service Hash -",
			    &rx_sd_frame.service_hash[0], 6);
		ERR("Ignore publish sdf, service hash mismatch");
		return NAN_ERR_INVAL;
	}

	/* check if proximity filter is configured */
	if (psrvc->discovery_range == 0 &&
	    rx_sd_frame.rssi < NAN_RSSI_LOW_THRESHOLD) {
		ERR("Discovery range is limited, ignore this publish sdf since its rssi is %d (< %d)",
		    rx_sd_frame.rssi, NAN_RSSI_LOW_THRESHOLD);
		return NAN_ERR_RANGE;
	}

	// if ((rx_sd_frame.service_ctrl_btmap & (1 << 2)) &&
	// (strcmp(psrvc->matching_filter_rx, "null") != 0)) {
	if (strcmp(psrvc->matching_filter_rx, "null") != 0) {
		int p = 0, q = 0;
		q = 1;

		/* check rx matching filter  with   matching filter recveived in
		 * frame */
		/* test for multiple matching filters */
		p = nan_match_filter_ext(psrvc->matching_filter_rx,
					 (char *)rx_sd_frame.matching_filter,
					 psrvc->rx_filter_len,
					 rx_sd_frame.matching_filter_len);

		ERR("===> q is %d and p is %d", q, p);
		if (p == 0) {
			ERR("FIlter did not match");
			return NAN_ERR_INVAL;
		}
	}

	if (cur_if->pnan_info->process_fa_attr == 1 && rx_sd_frame.fa_found) {
		struct nan_params_fa fap;
		nan_fa_attr *fa_attr = &rx_sd_frame.fa;
		int num_bits = 0;

		/* fill up nan_params values */
		fap.interval = 16 * (1 << fa_attr->e.ctrl);
		fap.repeat_entry = 0; /* valid only for current DW */
		fap.op_class = fa_attr->e.op_class;
		fap.op_chan = fa_attr->e.chan_num;
		fap.availability_map = fa_attr->e.availability_btmp;
		INFO("Connfiguring FW for further availability parameters");
		ret = nancmd_set_fa(cur_if, &fap);
		if (ret != NAN_ERR_SUCCESS)
			ERR("Failed to set FA parameters in FW");

		num_bits = NBITS(fa_attr->e.availability_btmp);

		/* Enqueue num_bits number of unicast frames to firmwrare to
		 * probe further availability on each of the further available
		 * window advertised by remote */
		ERR("Sending %d unicast frames to probe further availability",
		    num_bits);
	}

	/* send successful discovery result event */
	nan_send_sdf_event(cur_if, NAN_EVENT_DISCOVERY_RESULT, &rx_sd_frame,
			   0x5);

	return NAN_ERR_SUCCESS;
}

void compute_hash(u16 *hash_arr, u8 *final_hash, int bloom_filter_len)
{
	int j = 0;
	u16 filter_byte;
	u16 filter_bit;
	for (j = 0; j < bloom_filter_len; j++)
		final_hash[j] = 0;

	for (j = 0; j < 4; j++) {
		filter_byte = hash_arr[j] / 8;
		filter_bit = hash_arr[j] % 8;
		ERR("hash array index %d,%02x, filter_byte=%d, filter_bit=%d",
		    j, hash_arr[j], filter_byte, filter_bit);
		final_hash[filter_byte] |= 1 << filter_bit;
	}
	ERR("\nFinal Hash");
	for (j = 0; j < bloom_filter_len; j++)
		ERR(" %02x", final_hash[j]);
}

enum nan_error nan_handle_rx_subscribe(struct mwu_iface_info *cur_if,
				       unsigned char *buffer, int size)
{
	struct published_service *psrvc = &cur_if->pnan_info->p_service;
	struct nan_rx_sd_frame rx_sd_frame;
	// char dest[6];
	char descision = 0;
	char bloom_filter_index = 0;
	int p = 0; /* is positive match filter */
	int q = 0; /* is match filter bit set */
	int r = 0; /* is STAUT mac included */
	int s = 0; /* is include bit set */
	int tx_sd_frame = 0, m = 0;
	int ret = 0;
	u8 i = 0;
	u16 hash_arr[4];
	u16 mac_hash[4];
	memset(&rx_sd_frame, 0, sizeof(struct nan_rx_sd_frame));

	/* check if publish is configured for solicited publish */
	if (psrvc->publish_type != PUBLISH_TYPE_SOLICITED) {
		ERR("Not configured to do solicited publish, ignore subscribe sdf");
		return NAN_ERR_NOTREADY;
	}

	ret = nan_parse_rx_sdf(cur_if, buffer, size, &rx_sd_frame,
			       SERVICE_CTRL_BITMAP_SUBSCRIBE);
	if (ret == NAN_ERR_UNSUPPORTED) {
		return ret;
	}

	if (ret != NAN_ERR_SUCCESS) {
		ERR("Failed to parse rx SD frame");
		return NAN_ERR_INVAL;
	}

	/* check if service hash matches to the one we are publishing */
	if (memcmp(psrvc->service_hash, rx_sd_frame.service_hash, ETH_ALEN) !=
	    0) {
		mwu_hexdump(MSG_INFO, "Sebscribed service Hash -",
			    psrvc->service_hash, 6);
		mwu_hexdump(MSG_INFO, "Received service Hash -",
			    &rx_sd_frame.service_hash[0], 6);
		ERR("Ignore publish sdf, service hash mismatch");
		return NAN_ERR_INVAL;
	}

	if (cur_if->pnan_info->process_fa_attr == 1 && rx_sd_frame.fa_found) {
		struct nan_params_fa fap;
		nan_fa_attr *fa_attr = &rx_sd_frame.fa;
		int num_bits = 0;

		/* fill up nan_params values */
		fap.interval = 16 * (1 << fa_attr->e.ctrl);
		fap.repeat_entry = 0; /* valid only for current DW */
		fap.op_class = fa_attr->e.op_class;
		fap.op_chan = fa_attr->e.chan_num;
		fap.availability_map = fa_attr->e.availability_btmp;
		INFO("Connfiguring FW for further availability parameters");
		ret = nancmd_set_fa(cur_if, &fap);
		if (ret != NAN_ERR_SUCCESS)
			ERR("Failed to set FA parameters in FW");

		num_bits = NBITS(fa_attr->e.availability_btmp);

		ERR("Sending %d unicast frames to probe further availability",
		    num_bits);
	}

	/* check if proximity filter is configured */
	if (psrvc->discovery_range == 0 &&
	    rx_sd_frame.rssi < NAN_RSSI_LOW_THRESHOLD) {
		ERR("Discovery range is limited, ignore subscribe sdf since its rssi is %d (< %d)",
		    rx_sd_frame.rssi, NAN_RSSI_LOW_THRESHOLD);
		return NAN_ERR_RANGE;
	}

	ERR("MACCC--->");
	print_mac(rx_sd_frame.peer_mac);

#if 0
    /* check if solicited publish type is broadcast or unicast */
    if (psrvc->solicited_tx_type == TX_TYPE_BCAST)
        memcpy(dest, "\x0xff,\x0xff,\x0xff,\x0xff,\x0xff,\x0xff", 6);
    else
        memcpy(&dest[0], &rx_sd_frame.peer_mac[0], ETH_ALEN);
#endif

	// Bloom filter present
	if ((rx_sd_frame.service_ctrl_btmap & (1 << 3)) &&
	    (rx_sd_frame.srf.srf_ctrl & 1)) {
		u32 dev_crc;
		u8 recvd_bloom_filter_len = 0;

		q = !!(rx_sd_frame.srf.srf_ctrl & (1 << 1));

		bloom_filter_index = (rx_sd_frame.srf.srf_ctrl & 0x0c) >> 2;
		ERR("======> Bloom filter present index is %d",
		    bloom_filter_index);
		ERR("\n Number of Self macs = %d", 1);
		recvd_bloom_filter_len = rx_sd_frame.srf_len - 1;
		ERR("\n Length of received bloom filter  = %d",
		    recvd_bloom_filter_len);
		INFO("Own mac:");
		print_mac(cur_if->device_mac_addr);
		m = recvd_bloom_filter_len * 8;

		for (i = 0; i < 4; i++) {
			dev_crc = compute_bloom_filter_crc(
				(i + 4 * bloom_filter_index),
				(u8 *)&cur_if->device_mac_addr, 1);
			dev_crc = dev_crc & 0x0000ffff;
			hash_arr[i] = dev_crc % m;
			ERR("\nHash 1 bits %04x", hash_arr[i]);
		}
		compute_hash(hash_arr, (u8 *)mac_hash, recvd_bloom_filter_len);
		if (is_bloom_filter_match_success(rx_sd_frame.bloom_filter,
						  (u8 *)mac_hash,
						  recvd_bloom_filter_len)) {
			ERR("\nBloom filter Match Success");
			p = 1;
		} else {
			p = 0;
			ERR("Bloom filter match failed");
		}
	}

	/* check if matching filter is available */
	if (rx_sd_frame.service_ctrl_btmap & (1 << 2)) {
		q = 1;
		p = nan_match_filter_ext((char *)rx_sd_frame.matching_filter,
					 psrvc->matching_filter_rx,
					 rx_sd_frame.matching_filter_len,
					 psrvc->rx_filter_len);
		ERR("===> q is %d and p is %d", q, p);
	}

	/* check for include bit and srf */
	if (!(rx_sd_frame.srf.srf_ctrl & 1)) {
		if ((rx_sd_frame.service_ctrl_btmap & (1 << 3))) {
			s = !!(rx_sd_frame.srf.srf_ctrl & (1 << 1));
			r = is_mac_in_srf(cur_if->device_mac_addr,
					  &rx_sd_frame.srf);
			ERR("===> s is %d and r is %d", s, r);
		}
	}

	descision = 8 * s + 4 * r + 2 * q + p + 1;

	ERR("======> Descision is %d", descision);
	ERR("====> req instance id %d", rx_sd_frame.instance_id);
	cur_if->pnan_info->instance_id = rx_sd_frame.instance_id;

	switch (descision) {
	case 1:
		tx_sd_frame = 1;
		break;
	case 2:
		tx_sd_frame = 0;
		ERR("Invalid combination of include bit, mac_address and matchig filter");
		break;
	case 3:
		tx_sd_frame = 0;
		ERR("Ignore this subscribe message since match condition not met");
		break;
	case 4:
		tx_sd_frame = 1;
		break;
	case 5:
		tx_sd_frame = 0;
		ERR("Ignore this subscribe message since match condition not met");
		break;
	case 6:
		tx_sd_frame = 0;
		ERR("Invalid combination of include bit, mac_address and matchig filter");
		break;
	case 7:
		tx_sd_frame = 0;
		ERR("Invalid combination of include bit, mac_address and matchig filter");
		break;
	case 8:
		tx_sd_frame = 0;
		ERR("Ignore this subscribe message since match condition not met");
		break;
	case 9:
		tx_sd_frame = 0;
		ERR("Ignore this subscribe message since match condition not met");
		break;
	case 10:
		tx_sd_frame = 0;
		ERR("Invalid combination of include bit, mac_address and matchig filter");
		break;
	case 11:
		tx_sd_frame = 0;
		ERR("Invalid combination of include bit, mac_address and matchig filter");
		break;
	case 12:
		tx_sd_frame = 0;
		ERR("Ignore this subscribe message since match condition not met");
		break;
	case 13:
		tx_sd_frame = 1;
		break;
	case 14:
		tx_sd_frame = 0;
		ERR("Invalid combination of include bit, mac_address and matchig filter");
		break;
	case 15:
		tx_sd_frame = 0;
		ERR("Ignore this subscribe message since match condition not met");
		break;
	case 16:
		tx_sd_frame = 1;
		break;
	}

	if (tx_sd_frame) {
		ret = nan_tx_sdf(cur_if, PUBLISH, rx_sd_frame.peer_mac);
		if (ret == NAN_ERR_SUCCESS) {
			INFO("Tx SDF Successful");
			/* send successful discovery result event */
			nan_send_sdf_event(cur_if, NAN_EVENT_REPLIED,
					   &rx_sd_frame, 0x5);
		} else {
			ERR("Tx SDF failed");
		}
	}

	return ret;
}
