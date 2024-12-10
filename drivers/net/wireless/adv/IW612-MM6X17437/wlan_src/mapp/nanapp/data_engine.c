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

/* data_engine.c: implementation of the nan data engine
 */

#include "nan_lib.h"
#include "discovery_engine.h"
#include "mwu.h"
#include "util.h"
#include "mwu_log.h"
#include "data_engine.h"
#include "nan.h"
#include "assert.h"
#include "mwu_key_material.h"
#include "nan_eapol.h"
u8 a_band_flag = TRUE;
void change_ndp_state(struct mwu_iface_info *cur_if, enum ndp_state new);
void nan_send_ndp_event(struct mwu_iface_info *cur_if, u8 event_id,
			u8 *event_buffer, unsigned int buffer_len);

typedef struct _oper_bw_chan {
	unsigned char op_class; // operating class
	unsigned char bandwidth; // bandwidth 0-20M 1-40M 2-80M 3-160M
	unsigned char channel_list[13]; // channel list
} oper_bw_chan;

static oper_bw_chan global_oper_bw_chan[] = {
	/** Global oper class, bandwidth, channel list*/
	{81, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
	{82, 0, {14}},
	{83, 1, {1, 2, 3, 4, 5, 6, 7}},
	{84, 1, {8, 9, 10, 11, 13}},
	{115, 0, {36, 40, 44, 48}},
	{116, 1, {36, 44}},
	{117, 1, {40, 48}},
	{124, 0, {149, 153, 157, 161}},
	{125, 0, {149, 153, 157, 161, 165, 169}},
	{126, 1, {149, 157}},
	{127, 1, {153, 161}},
	{128, 2, {42, 58, 106, 122, 138, 155}},
	{129, 3, {50, 114}},
	{130, 2, {42, 58, 106, 122, 138, 155}},
};

static u16 repeat_interval[] = {
	0, 128, 256, 512, 1024, 2048, 4096, 8192,
};

int count_bits(u32 bitmap)
{
	int i, count = 0;

	for (i = 0; i < DEFAULT_BITMAP_LEN; i++) {
		if (bitmap & (1 << i))
			count++;
	}

	return count;
}
u16 ndp_get_chan_bitmap(int op_class, int chan)
{
	int i, j, temp_chan;
	static int num_op_class =
		sizeof(global_oper_bw_chan) / sizeof(oper_bw_chan);

	for (i = 0; i < num_op_class; i++) {
		if (global_oper_bw_chan[i].op_class == op_class) {
			for (j = 0; j < 13; j++) {
				temp_chan =
					global_oper_bw_chan[i].channel_list[j];
				if (temp_chan && chan == temp_chan)
					return (1U << j);
				else if (!temp_chan)
					break;
			}
		}
	}

	return 0;
}

u8 nan_get_channels_from_bitmap(u8 op_class, u16 bitmap, u8 primary_chan_bitmap,
				u8 *channels)
{
	int i, j, k = 0;
	static int num_op_class =
		sizeof(global_oper_bw_chan) / sizeof(oper_bw_chan);

	for (i = 0; i < num_op_class; i++) {
		// TODO:This needs more generic handling
		if ((op_class == 128 || op_class == 130) &&
		    global_oper_bw_chan[i].op_class == 125) {
			channels[k++] =
				global_oper_bw_chan[i]
					.channel_list[primary_chan_bitmap -
						      1]; // the index
							  // primary_chan_bitmap
							  // needs update
			return k;
		}

		if (global_oper_bw_chan[i].op_class == op_class) {
			for (j = 0; j < 13; j++) {
				if (bitmap & (1U << j))
					channels[k++] =
						global_oper_bw_chan[i]
							.channel_list[j];
			}
		}
	}
	return k;
}

/*============NEW NAN Data Engine functions start from here=============*/

/*Function to parse NDP frame buffer and return the
  pointer to the requested NAN attribute */
tlv_header *nan_get_nan_attr(u8 attr, u8 *buffer, int size)
{
	u8 *tmp_ptr = buffer;
	int bytes_left = size;
	tlv_header *tlv = NULL;
	u8 attr_id;

	tmp_ptr = buffer + NDP_ATTR_START_OFFSET;
	bytes_left -= NDP_ATTR_START_OFFSET;

	while (bytes_left > NAN_ATTR_HDR_LEN) {
		tlv = (tlv_header *)tmp_ptr;
		attr_id = tlv->type;

		if (attr_id == attr) {
			/*Match found return address*/
			return tlv;
		}
		bytes_left -= (tlv->length + NAN_ATTR_HDR_LEN);
		tmp_ptr += tlv->length + NAN_ATTR_HDR_LEN;
	}
	/*Match Not found Return NULL*/
	INFO("attribute id 0x%x Not present", attr);
	return NULL;
}
/*Function to parse NDP frame buffer and return the
  pointer to the requested NAN attribute's 2nd occurence */
tlv_header *nan_get_nan_attr_second(u8 attr, u8 *buffer, int size)
{
	u8 *tmp_ptr = buffer;
	int bytes_left = size;
	tlv_header *tlv = NULL;
	u8 attr_id;
	u16 attr_len;
	u8 count = 0;

	tmp_ptr = buffer + NDP_ATTR_START_OFFSET;
	bytes_left -= NDP_ATTR_START_OFFSET;

	while (bytes_left > NAN_ATTR_HDR_LEN) {
		tlv = (tlv_header *)tmp_ptr;
		attr_id = tlv->type;
		attr_len = tlv->length;

		if (attr_id == attr) {
			/*Match found*/
			count++;
		}
		if (count == 2)
			return tlv;

		bytes_left -= (attr_len + NAN_ATTR_HDR_LEN);
		tmp_ptr += attr_len + NAN_ATTR_HDR_LEN;
	}
	/*Match Not found Return NULL*/
	INFO("attribute id 0x%x Not present", attr);
	return NULL;
}
/*Function to append the NAN attr to the tx buffer; This
   is to be used for trasmitting the NDP frames */
void nan_append_nan_attr(tlv_header *attr, u8 *buffer, int *size)
{
	u8 *temp = buffer + (*size);
	memcpy(temp, (u8 *)attr, (attr->length + NAN_ATTR_HDR_LEN));
	*size += (attr->length + NAN_ATTR_HDR_LEN);
	return;
}

#define WFA_OUI_LEN 3
u8 wfa_oui[WFA_OUI_LEN] = {0x50, 0x6f, 0x9a};
u8 ipv6_iface_standard_id[IPV6_IFACE_IDENTIFIER_LEN] = {0x02, 0x50, 0x43, 0xff,
							0xfe, 0x00, 0x00, 0x00};

int nan_get_ipv6_iface_identifier(u8 *iface_identifier, u8 *device_mac_addr)
{
	int i = 0, retval = 0;

	memcpy(iface_identifier, ipv6_iface_standard_id,
	       IPV6_IFACE_IDENTIFIER_LEN);

	for (i = 3; i < ETH_ALEN; i++)
		iface_identifier[2 + i] = device_mac_addr[i];

	return retval;
}

int is_wfa_oui(u8 *oui)
{
	return (memcmp(oui, wfa_oui, WFA_OUI_LEN) ? 0 : 1);
}

static int nan_process_service_spc_info(
	u8 *serv_spc_info, transport_port_sub_attr **tport_sub_attr,
	transport_protocol_sub_attr **tprotocol_sub_attr, u16 total_len)
{
	int ret = 0;
	u16 processed_len = 0;
	tlv_header *tlv = NULL;

	INFO("process serv info (resp):333");
	while (processed_len < total_len) {
		tlv = (tlv_header *)(serv_spc_info + processed_len);
		switch (tlv->type) {
		case SERVICE_PROTOCOL_TRANSPORT_PORT_SUB_ATTR:
			*tport_sub_attr = (transport_port_sub_attr *)tlv;
			INFO("process serv info (resp):444");
			break;
		case SERVICE_PROTOCOL_TRANSPORT_PORTOCOL_SUB_ATTR:
			*tprotocol_sub_attr =
				(transport_protocol_sub_attr *)tlv;
			INFO("process serv info (resp):555");
			break;
		default:
			break;
		}
		processed_len += (tlv->length + sizeof(tlv_header));
	}

	return ret;
}

void nan_get_optional_ndp_ext_attr(nan_ndp_attr *ndp_attr, u8 **tlv_list,
				   u16 *total_tlv_len)
{
	u8 *temp = ndp_attr->optional;
	u16 optional_attr_len =
		ndp_attr->len -
		(offsetof(nan_ndp_attr, optional) - NAN_ATTR_HDR_LEN);

	if (ndp_attr->ndp_ctrl.publish_id_present) {
		temp += 1; /** publish id, 1 byte */
		optional_attr_len -= 1;
	}

	if (ndp_attr->ndp_ctrl.responder_ndi_present) {
		temp += ETH_ALEN; /** responder_ndi, 6 byte */
		optional_attr_len -= ETH_ALEN;
	}

	*tlv_list = temp;
	*total_tlv_len = optional_attr_len;
}

void nan_get_ipv6_link_local_tlv(struct mwu_iface_info *cur_if,
				 ipv6_link_local_tlv *ipv6_tlv)
{
	u8 *tlv_list = NULL;
	u16 processed_tlv_len = 0, total_tlv_len = 0, i = 0;
	nan_ndp_attr *ndp_attr;
	tlv_header *tlv = NULL;

	if ((!cur_if->pnan_info->ndpe_not_present) &&
	    (cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported)) {
		ndp_attr = (nan_ndp_attr *)(cur_if->pnan_info->peer_ndp_attr);
		nan_get_optional_ndp_ext_attr(ndp_attr, &tlv_list,
					      &total_tlv_len);
		INFO("Optional(ndpe) attributes extracted");

		while (processed_tlv_len < total_tlv_len) {
			tlv = (tlv_header *)(tlv_list + processed_tlv_len);
			switch (tlv->type) {
			case TLV_TYPE_IPV6_LINK_LOCAL:
				ipv6_tlv = (ipv6_link_local_tlv *)tlv;
				INFO("ipv6 identifier(req):");
				for (i = 0; i < IPV6_IFACE_IDENTIFIER_LEN; i++)
					INFO("%x", ipv6_tlv->iface_id[i]);
				break;
			default:
				break;
			}
			processed_tlv_len += (tlv->length + sizeof(tlv_header));
		}
	}
}

void nan_send_ndpe_data_event(struct mwu_iface_info *cur_if,
			      ipv6_link_local_tlv *ipv6_tlv,
			      transport_port_sub_attr *tport_sub_attr,
			      transport_protocol_sub_attr *tprotocol_sub_attr)
{
	ndp_ndpe_data *ndpe_data_event = NULL;
	int ndpe_data_event_len = 0;

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported)) {
		INFO("Sending NDPE data event");
	} else {
		INFO("ndpe_attr not supported");
		return;
	}

	ndpe_data_event_len = sizeof(ndp_ndpe_data);
	ndpe_data_event = (ndp_ndpe_data *)malloc(ndpe_data_event_len);
	memset(ndpe_data_event, 0, ndpe_data_event_len);
	ndpe_data_event->type = NDP_TYPE_UNICAST;
	memcpy(ndpe_data_event->peer_mac, cur_if->pnan_info->peer_mac,
	       ETH_ALEN);

	if (ipv6_tlv) {
		memcpy(ndpe_data_event->iface_identifier, ipv6_tlv->iface_id,
		       IPV6_IFACE_IDENTIFIER_LEN);
	} else {
		/** no ipv6 addr */
		memset(ndpe_data_event->iface_identifier, 0,
		       IPV6_IFACE_IDENTIFIER_LEN);
	}

	if (tport_sub_attr) {
		ndpe_data_event->transport_port =
			tport_sub_attr->transport_port;
	} else {
		/** no port */
		memset(&ndpe_data_event->transport_port, 0xff, sizeof(u16));
	}

	if (tprotocol_sub_attr) {
		ndpe_data_event->transport_protocol =
			tprotocol_sub_attr->transport_portocol;
	} else {
		/** no protocol */
		ndpe_data_event->transport_protocol = 0xff;
	}

	INFO("NDPE Data Event buffer populated");
	/*Send event to app*/
	nan_send_ndp_event(cur_if, NAN_EVENT_NDPE_DATA, (u8 *)ndpe_data_event,
			   ndpe_data_event_len);
	FREE(ndpe_data_event);
}
static inline void nan_get_optional_ndp_attr(nan_ndp_attr *ndp_attr,
					     u8 *publish_id, u8 **responder_ndi,
					     u8 **service_info,
					     int *service_len)
{
	u8 *temp = ndp_attr->optional;
	int optional_attr_len =
		ndp_attr->len -
		(offsetof(nan_ndp_attr, optional) - NAN_ATTR_HDR_LEN);
	int bytes_left = optional_attr_len;

	/*Extract publish id if present*/
	if (ndp_attr->ndp_ctrl.publish_id_present) {
		*publish_id = *temp;
		temp += 1; /*publish id is 1 byte*/
		bytes_left -= 1;
	}
	/*Extract responder ndi if present*/
	if (ndp_attr->ndp_ctrl.responder_ndi_present) {
		*responder_ndi = temp;
		temp += ETH_ALEN; /*responder_ndi is 6 byte*/
		bytes_left -= ETH_ALEN;
	}
	/*Extract service info if present*/
	if (ndp_attr->ndp_ctrl.ndp_specific_info_present) {
		*service_info = temp;
		service_len += bytes_left;
	}
}

static inline u8 nan_scan_chan_list(u8 *list, u8 chan)
{
	int i = 0;
	for (i = 0; i < 8; i++) {
		if (list[i] == chan)
			return chan;
	}
	return 0;
}

u8 na_array[5][32];
enum nan_error validate_na_array(u32 bitmap, u8 channel, u8 map_id)
{
	u8 i;

	ERR("NAN2 : validating array for slots 0x%x, channel %d", bitmap,
	    channel);
	for (i = 0; i < 32; i++) {
		if (bitmap & (1 << i)) {
			if (!na_array[map_id][i] ||
			    na_array[map_id][i] == channel)
				na_array[map_id][i] = channel;
			else
				return NAN_ERR_INVAL;
		}
	}
	return NAN_ERR_SUCCESS;
}

/*Function detects overlapping entries on different channels.
  This will be used for NDP negative test cases */
static u8 nan_validate_avail_entries(struct mwu_iface_info *cur_if)
{
	u8 reject = FALSE, i = 0;
	u32 bitmap = 0;
	u8 channel = 0;
	u8 map = -1, multiple_map_ids = 0;

	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (cur_if->pnan_info->peer_avail_info.committed_valid &
		    (1 << i)) {
			if (map == -1)
				map = cur_if->pnan_info->peer_avail_info
					      .entry_committed[i]
					      .map_id;

			if (map != cur_if->pnan_info->peer_avail_info
					   .entry_committed[i]
					   .map_id)
				multiple_map_ids = 1;
		}

		if (cur_if->pnan_info->peer_avail_info.conditional_valid &
		    (1 << i)) {
			if (map == -1)
				map = cur_if->pnan_info->peer_avail_info
					      .entry_conditional[i]
					      .map_id;

			if (map != cur_if->pnan_info->peer_avail_info
					   .entry_conditional[i]
					   .map_id)
				multiple_map_ids = 1;
		}
	}

#if 1
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (cur_if->pnan_info->peer_avail_info.committed_valid &
		    (1 << i)) {
			bitmap = cur_if->pnan_info->peer_avail_info
					 .entry_committed[i]
					 .combined_time_bitmap;
			channel = cur_if->pnan_info->peer_avail_info
					  .entry_committed[i]
					  .channels[0];
			if (NAN_ERR_SUCCESS !=
			    validate_na_array(bitmap, channel,
					      cur_if->pnan_info->peer_avail_info
						      .entry_committed[i]
						      .map_id)) {
				reject = TRUE;
				break;
			}

			if (multiple_map_ids == 1) {
				/*  PLEASE FIX THIS IN DIFFERENT WAY.
						if(cur_if->pnan_info->peer_avail_info.entry_committed[i].op_class
				   > 86 && !(cur_if->pnan_info->a_band))
						{
						    valid_map = (1<<i);
						    cur_if->pnan_info->peer_avail_info.committed_valid
				   &= ~(valid_map);
						}

						if(cur_if->pnan_info->peer_avail_info.entry_committed[i].op_class
				   < 86 && cur_if->pnan_info->a_band)
						{
						    valid_map = (1<<i);
						    cur_if->pnan_info->peer_avail_info.committed_valid
				   &= ~(valid_map);
						}
				*/
			}
		}
		if (cur_if->pnan_info->peer_avail_info.conditional_valid &
		    (1 << i)) {
			if (map == -1)
				map = cur_if->pnan_info->peer_avail_info
					      .entry_conditional[i]
					      .map_id;

			bitmap = cur_if->pnan_info->peer_avail_info
					 .entry_conditional[i]
					 .combined_time_bitmap;
			channel = cur_if->pnan_info->peer_avail_info
					  .entry_conditional[i]
					  .channels[0];
			if (NAN_ERR_SUCCESS !=
			    validate_na_array(bitmap, channel,
					      cur_if->pnan_info->peer_avail_info
						      .entry_conditional[i]
						      .map_id)) {
				reject = TRUE;
				break;
			}

			if (multiple_map_ids == 1) {
				/*
						if(cur_if->pnan_info->peer_avail_info.entry_conditional[i].op_class
				   > 86 && !(cur_if->pnan_info->a_band))
						{
						    valid_map = (1<<i);
						    cur_if->pnan_info->peer_avail_info.conditional_valid
				   &= ~(valid_map);
						}

						if(cur_if->pnan_info->peer_avail_info.entry_conditional[i].op_class
				   < 86 && cur_if->pnan_info->a_band)
						{
						    valid_map = (1<<i);
						    cur_if->pnan_info->peer_avail_info.conditional_valid
				   &= ~(valid_map);
						}
				*/
			}
		}
	}
	memset(na_array, 0, sizeof(na_array));
#endif
	return reject;
}

static u8 nan_populate_schedule(avail_entry_t *self_entry,
				u8 band_entry_potential,
				avail_entry_t *peer_entry,
				struct nan_schedule *schedule, u8 entry_index)
{
	u8 final_channel = 0, final_opclass = 0, i = 0, j = 0;
	/* Iterate through n self avail entries and match channel in peer's
	 * entry.              */
	/* If channel match found, take subset of self and peer bitmap to
	 * generate final bitmap.*/
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		/*Match channel*/
		if (band_entry_potential) {
			/*Band entry so accept proposed channel*/
			final_channel = peer_entry->channels[0];
			final_opclass = peer_entry->op_class;
		} else {
			final_channel =
				nan_scan_chan_list(self_entry[i].channels,
						   peer_entry->channels[0]);
			final_opclass = peer_entry->op_class;
		}
		if (final_channel) {
			ERR("NAN2 : Own availability map 0x%x at index %d",
			    self_entry[i].time_bitmap[0], i);

			for (j = 0; j < peer_entry->time_bitmap_count; j++) {
				schedule[entry_index].availability_map[j] =
					peer_entry->time_bitmap[j] &
					self_entry[i].time_bitmap[0];
				schedule[entry_index].start_offset[j] =
					peer_entry->start_offset[j];
			}
			schedule[entry_index].op_chan = final_channel;
			schedule[entry_index].op_class = final_opclass;
			schedule[entry_index].time_bitmap_count =
				peer_entry->time_bitmap_count;
			schedule[entry_index].period = peer_entry->period;
			ERR("NAN2 : channel %u set at index %u", final_channel,
			    entry_index);

			break;
		}
	}

	if (final_channel)
		return TRUE;
	else
		return FALSE;
}

static void nan_process_peer_proposal(struct mwu_iface_info *cur_if,
				      struct nan_schedule *schedule,
				      u8 *num_entries, u32 *final_bitmap,
				      u8 *final_chan_set)
{
	u8 entry_index = 0, i = 0, band_entry_potential = 0;
	peer_availability_info *peer_avail_info =
		&cur_if->pnan_info->peer_avail_info;
	peer_availability_info *self_avail_info =
		&cur_if->pnan_info->self_avail_info;
	avail_entry_t *self_entry = NULL;

	/*Process peers proposal against own committed/potential entries*/
	if (cur_if->pnan_info->self_avail_info.committed_valid) {
		self_entry = cur_if->pnan_info->self_avail_info.entry_committed;
		ERR("NAN2 : Comparing proposal against own committed entries");
	} else if (cur_if->pnan_info->self_avail_info.potential_valid) {
		self_entry = cur_if->pnan_info->self_avail_info.entry_potential;
		band_entry_potential = self_avail_info->band_entry_potential;
		ERR("NAN2 : Comparing proposal against own potential entries");
	} else {
		assert(0); /*Should never reach here*/
	}

	/* If both potential and committed entries are present,
	take the one which has a bigger bitmap */
	if ((cur_if->pnan_info->self_avail_info.committed_valid != 0) &&
	    (cur_if->pnan_info->self_avail_info.potential_valid != 0)) {
		if (count_bits(cur_if->pnan_info->self_avail_info
				       .entry_committed[0]
				       .combined_time_bitmap) >
		    count_bits(cur_if->pnan_info->self_avail_info
				       .entry_potential[0]
				       .combined_time_bitmap)) {
			self_entry = cur_if->pnan_info->self_avail_info
					     .entry_committed;
			ERR("Update: Comparing proposal against own committed entries");
		} else {
			self_entry = cur_if->pnan_info->self_avail_info
					     .entry_potential;
			band_entry_potential =
				self_avail_info->band_entry_potential;
			ERR("Update: Comparing proposal against own potential entries");
		}
	}

	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (entry_index >= MAX_SCHEDULE_ENTRIES)
			break;
		if (cur_if->pnan_info->peer_avail_info.conditional_valid &
		    (1 << i)) {
			if (nan_populate_schedule(
				    self_entry, band_entry_potential,
				    &peer_avail_info->entry_conditional[i],
				    schedule, entry_index)) {
				entry_index++;
			}
		}
	}
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (entry_index >= MAX_SCHEDULE_ENTRIES)
			break;
		if (cur_if->pnan_info->peer_avail_info.committed_valid &
		    (1 << i)) {
			if (nan_populate_schedule(
				    self_entry, band_entry_potential,
				    &peer_avail_info->entry_committed[i],
				    schedule, entry_index)) {
				entry_index++;
			}
		}
	}
	*num_entries = entry_index;
	if (*num_entries) {
		short int i = 0, j = 0;
		/*Generate combined map*/
		for (i = 0; i < *num_entries; i++) {
			for (j = 0; j < schedule[i].time_bitmap_count; j++) {
				*final_bitmap |=
					schedule[i].availability_map[j];
			}
		}
		*final_chan_set = TRUE;
		// self_avail_info->map_id = peer_avail_info->map_id;
	}
}

static enum nan_error nan_validate_bitmap_for_qos(u32 bitmap, ndl_qos_t *qos)
{
	/*Check min no of slots*/
	int i = 0, cnt = 0, lat = 0;
	for (i = 0; i < 32; i++) {
		if (bitmap & (1 << i))
			cnt++;
	}
	if (cnt < qos->min_slots) {
		INFO("NAN2 NDL QOS min slots failed");
		return NAN_ERR_INVAL;
	}

	INFO("NAN2 NDL Qos max_latency = %d, min_slots= %d", qos->max_latency,
	     qos->min_slots);

	/*Check latency*/
	for (i = 0; i < 32; i++) {
		if (bitmap & (1 << i)) {
			lat = 0;
		} else {
			if (++lat > qos->max_latency) {
				INFO("NAN2 NDL Qos Latency failed, lat %d",
				     lat);
				return NAN_ERR_INVAL;
			}
		}
	}
	return NAN_ERR_SUCCESS;
}

static enum nan_error
nan_generate_counter_proposal(struct mwu_iface_info *cur_if, u32 *bitmap,
			      u8 *reason)
{
	peer_availability_info *peer_info = &cur_if->pnan_info->peer_avail_info;
	peer_availability_info *self_info = &cur_if->pnan_info->self_avail_info;
	ndl_qos_t qos = {0};
	u32 final_bitmap = self_info->entry_potential[0].time_bitmap[0] &
			   NAN_COUNTER_BITMAP;

	if (peer_info->entry_potential[0].combined_time_bitmap != 0)
		final_bitmap =
			final_bitmap &
			peer_info->entry_potential[0].combined_time_bitmap;

	/*Calculate combined QoS of peer and self*/
	qos.min_slots = (peer_info->qos.min_slots < self_info->qos.min_slots) ?
				self_info->qos.min_slots :
				peer_info->qos.min_slots;
	qos.max_latency =
		(peer_info->qos.max_latency > self_info->qos.max_latency) ?
			self_info->qos.max_latency :
			peer_info->qos.max_latency;

	if (final_bitmap) {
		if (cur_if->pnan_info->qos_enabled) {
			if (NAN_ERR_SUCCESS ==
			    nan_validate_bitmap_for_qos(final_bitmap, &qos)) {
				*bitmap = final_bitmap;
				*reason = NAN_REASON_CODE_DEFAULT;
				return NAN_ERR_SUCCESS;
			} else {
				*bitmap = 0;
				*reason = NAN_REASON_CODE_NDL_UNACCEPTABLE;
				return NAN_ERR_INVAL;
			}
		} else {
			*bitmap = final_bitmap;
			*reason = NAN_REASON_CODE_DEFAULT;
			return NAN_ERR_SUCCESS;
		}
	} else {
		*reason = NAN_REASON_CODE_NDL_UNACCEPTABLE;
		*bitmap = 0;
		return NAN_ERR_INVAL;
	}
}

static void generate_ndc_bitmap(u32 final_bitmap, u32 *ndc_bitmap)
{
	int i = 0;
	for (i = 1; i < 31; i++) {
		if (final_bitmap & (1 << i)) {
			*ndc_bitmap = (1 << i);
			ERR("NAN2 : Selected NDC - 0x%x", *ndc_bitmap);
			break;
		}
	}
}

static void generate_imm_bitmap(u32 final_bitmap, u32 *imm_bitmap)
{
	int i = 0;
	for (i = 30; i > 0; i--) {
		if (final_bitmap & (1 << i)) {
			*imm_bitmap = (1 << i);
			ERR("NAN2 : Selected immutable - 0x%x", *imm_bitmap);
			break;
		}
	}
}

static void generate_sched_update_bitmap(u32 current_sched_bitmap, u32 *bitmap)
{
	int i = 0;
	for (i = 30; i > 0; i--) {
		if (!(current_sched_bitmap & (1 << i))) {
			*bitmap = (1 << i);
			if (((*bitmap) & NAN_POTENTIAL_BITMAP) == 0)
				continue;

			ERR("NAN2 : Selected the random update bitmap - 0x%x",
			    *bitmap);
			break;
		}
	}
}

enum nan_error nan_parse_schedule_req(struct mwu_iface_info *cur_if,
				      u8 *ndl_status, u8 *ndp_status,
				      u8 *reason)
{
	nan_availability_attr *avail_attr = NULL;
	nan_ndl_attr *ndl_attr = NULL;
	nan_ndl_qos_attr *ndl_qos = NULL;
	nan_ndp_attr *ndp_attr = NULL;
	nan_ndc_attr *ndc_attr = NULL;
	u8 peer_ndc = FALSE, i = 0, j = 0;
	struct nan_generic_buf *ndp_req_buf;
	peer_availability_info *peer_info = NULL;
	u32 final_bitmap = 0, peer_bitmap = 0,
	    ndc_time_bitmap[MAX_SCHEDULE_ENTRIES] = {0},
	    immutable_bitmap[MAX_SCHEDULE_ENTRIES] = {0};
	u32 combined_ndc_bitmap = 0, combined_immutable_bitmap = 0;
	struct nan_schedule schedule[MAX_SCHEDULE_ENTRIES];
	u8 final_chan_set = 0, num_entries = 0, immutable_bitmap_count = 0,
	   ndc_time_bitmap_count = 0;
	NDC_INFO *ndc = NULL;
	NDP_INFO *ndp = NULL;
	NDL_INFO *ndl = NULL;

	memset(schedule, 0, sizeof(schedule));

	*reason = NAN_REASON_CODE_DEFAULT;

	ndp_req_buf = cur_if->pnan_info->rx_ndp_req;

	a_band_flag = cur_if->pnan_info->a_band ? TRUE : FALSE;

	/* Process NDL attribute */
	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(
		NAN_NDL_ATTR, ndp_req_buf->buf, ndp_req_buf->size);

	if (ndl_attr &&
	    (ndl_attr->type_and_status.type == NDP_NDL_TYPE_REQUEST)) {
		if (ndl_attr->type_and_status.status !=
		    NDP_NDL_STATUS_CONTINUED) {
			ERR("NAN2 : NDL Attr status(%d) should be Continued(0) in NDL Request",
			    ndl_attr->type_and_status.status);
			return NAN_ERR_INVAL;
		}
		if (ndl_attr->ndl_ctrl.immutable_sched_present) {
			immutable_schedule *imm_sched;
			u8 i;

			ERR("NAN2 : Immutable entry present");
			if (ndl_attr->ndl_ctrl.ndl_peer_id_present)
				imm_sched = (immutable_schedule *)&ndl_attr
						    ->optional[1];
			else
				imm_sched = (immutable_schedule *)&ndl_attr
						    ->optional[0];
			// for (i = 0; i < imm_sched->no_of_entries; i++)
			//  For now assume only 1 immutable entry
			for (i = 0; i < 1; i++) {
				immutable_bitmap_count =
					nan_parse_generic_time_bitmap(
						&imm_sched->entry[i]
							 .time_bitmap_ctrl,
						imm_sched->entry[i]
							.time_bitmap_len,
						imm_sched->entry[i].bitmap,
						immutable_bitmap);
			}
			for (i = 0; i < immutable_bitmap_count; i++) {
				combined_immutable_bitmap |=
					immutable_bitmap[i];
			}
		}
		if (ndl_attr->ndl_ctrl.ndc_attr_present) {
			ERR("NAN2 : NDC for this NDL is present");
			// set flag to consider the NDC if propsed
		}
	}

	/* Process NDL QoS attribute*/
	ndl_qos = (nan_ndl_qos_attr *)nan_get_nan_attr(
		NAN_NDL_QOS_ATTR, ndp_req_buf->buf, ndp_req_buf->size);
	peer_info = &cur_if->pnan_info->peer_avail_info;
	if (ndl_qos) {
		peer_info->qos.min_slots = ndl_qos->min_slots;
		memcpy(&peer_info->qos.max_latency, ndl_qos->max_latency,
		       sizeof(peer_info->qos.max_latency));
	} else {
		peer_info->qos.min_slots = NAN_QOS_MIN_SLOTS;
		peer_info->qos.max_latency = NAN_QOS_MAX_LATENCY;
	}

	/* Process NDC attribute*/
	ndc_attr = (nan_ndc_attr *)nan_get_nan_attr(
		NAN_NDC_ATTR, ndp_req_buf->buf, ndp_req_buf->size);

	if (ndc_attr && ndc_attr->sched_ctrl.proposed_ndc) {
		ndc_time_bitmap_count = nan_parse_generic_time_bitmap(
			&ndc_attr->time_bitmap_ctrl, ndc_attr->time_bitmap_len,
			ndc_attr->bitmap, ndc_time_bitmap);
		// ERR("NAN2 : Proposed NDC with slots - %x", ndc_time_bitmap);
		//  Check whether NDC slot conflicts with our own availability
		// if (ndc_time_bitmap == 0x02 || ndc_time_bitmap == 0x0200)
		for (i = 0; i < ndc_time_bitmap_count; i++) {
			combined_ndc_bitmap |= ndc_time_bitmap[i];
		}

		if (combined_ndc_bitmap) {
			peer_ndc = TRUE;
		} else {
			ERR("NAN2 : Proposed NDC(%x) abnormal! Propose preferred/existing NDC slot",
			    combined_ndc_bitmap);
		}
	}

	/*Extract NDP attribute*/
	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(
			NAN_NDP_EXT_ATTR, ndp_req_buf->buf, ndp_req_buf->size);
	else
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(
			NAN_NDP_ATTR, ndp_req_buf->buf, ndp_req_buf->size);

	if (ndp_attr) {
		/*Update Peer_mac, peer/initiator ndi, dialog token, ndp_id*/
		ndc = &cur_if->pnan_info->ndc_info[0];
		ndl = &ndc->ndl_info[0];
		memcpy(ndl->peer_mac, ndp_req_buf->buf, ETH_ALEN);
		if (ndl_attr->ndl_ctrl.ndl_peer_id_present)
			ndl->peer_id = ndl_attr->optional[0];

		ndp = &ndl->ndp_info[0]; // NDP Identifier, 0 is reserved
		memcpy(ndp->initiator_ndi, ndp_attr->initiator_ndi, ETH_ALEN);
		memcpy(ndp->peer_ndi, ndp_attr->initiator_ndi, ETH_ALEN);
		memcpy(ndp->responder_ndi, cur_if->device_mac_addr, ETH_ALEN);
		ndp->ndp_id = ndp_attr->ndp_id;
		ndp->dialogue_token = ndp_attr->dialogue_token;
	}

	/*Extract and parse availability attribute*/
	avail_attr = (nan_availability_attr *)nan_get_nan_attr(
		NAN_AVAILABILITY_ATTR, ndp_req_buf->buf, ndp_req_buf->size);
	peer_info = &cur_if->pnan_info->peer_avail_info;
	nan_parse_availability(avail_attr, peer_info);

	ERR("NAN2 : parsed entries - committed %d, conditional %d, potential %d",
	    cur_if->pnan_info->peer_avail_info.committed_valid,
	    cur_if->pnan_info->peer_avail_info.conditional_valid,
	    cur_if->pnan_info->peer_avail_info.potential_valid);

	/*Copy sequence ID*/
	cur_if->pnan_info->self_avail_info.seq_id = peer_info->seq_id;

	/*Get the schedule proposal into single combined bitmap*/
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (cur_if->pnan_info->peer_avail_info.conditional_valid &
		    (1 << i)) {
			for (j = 0; j < cur_if->pnan_info->peer_avail_info
						.entry_conditional[i]
						.time_bitmap_count;
			     j++) {
				peer_bitmap |=
					cur_if->pnan_info->peer_avail_info
						.entry_conditional[i]
						.time_bitmap[j];
			}
		}
		if (cur_if->pnan_info->peer_avail_info.committed_valid &
		    (1 << i)) {
			for (j = 0; j < cur_if->pnan_info->peer_avail_info
						.entry_committed[i]
						.time_bitmap_count;
			     j++) {
				peer_bitmap |=
					cur_if->pnan_info->peer_avail_info
						.entry_committed[i]
						.time_bitmap[j];
			}
		}
	}
	ERR("NAN2 : peer's combined bitmap (0x%x)!", peer_bitmap);

	/* Sanity on NAN2 parameters of NDP Req */
	if (combined_immutable_bitmap &&
	    ((combined_immutable_bitmap & peer_bitmap) !=
	     combined_immutable_bitmap)) {
		/* Sanity on immutable */
		ERR("NAN2 : Immutable(0x%x) is not a subset of peer's bitmap (0x%x)!",
		    combined_immutable_bitmap, peer_bitmap);
		INFO("NAN2 : NDP request rejected");
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE;
		return NAN_ERR_SUCCESS;
	}

	/* Sanity on NDC */

	if (peer_ndc &&
	    ((combined_ndc_bitmap & peer_bitmap) != combined_ndc_bitmap)) {
		ERR("NAN2 : NDC(0x%x) is not a subset of NA(0x%x). Reject.",
		    combined_ndc_bitmap, peer_bitmap);
		INFO("NAN2 : NDP request rejected");
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_INVALID_AVAILABILITY;
		return NAN_ERR_SUCCESS;
	}

	/*Validate avail entries & detect overlap*/
	if (nan_validate_avail_entries(cur_if)) {
		INFO("NAN2 : NDP request rejected");
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_INVALID_AVAILABILITY;
		return NAN_ERR_SUCCESS;
	}
	/*Sanity on NAN2 parameters of NDP Req Over*/

	/*Process peers proposal and prepare response*/
	nan_process_peer_proposal(cur_if, schedule, &num_entries, &final_bitmap,
				  &final_chan_set);
	ERR("NAN2 : Number of final entries(%u) ", num_entries);
	ERR("NAN2 : Combined Final bit map set(%x) ", final_bitmap);

	{ // store the immutable schedule details in NDL_INFO
		if (ndl_attr && ndl_attr->ndl_ctrl.immutable_sched_present) {
			ndl->immutable_sched_present = TRUE;
			ndl->immutable_sched_bitmap = combined_immutable_bitmap;
		}

		// set the ndc_setup bit
		if (ndc)
			ndc->ndc_setup = TRUE;
	}

	/*Join or select new NDC*/

	if (peer_ndc &&
	    ((combined_ndc_bitmap & peer_bitmap) == combined_ndc_bitmap)) {
		// Joining the Peer NDC
		ndc = &cur_if->pnan_info->ndc_info[0];
		ndc->slot = combined_ndc_bitmap;
		memcpy(ndc->ndc_id, ndc_attr->ndc_id, ETH_ALEN);
		*ndl_status = NDP_NDL_STATUS_ACCEPTED;
		ERR("NAN2 : Accepted proposed NDC with slot - %x",
		    combined_ndc_bitmap);
	} else {
		u8 wfa_ndc_id[6] = {0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00};
		// TODO Use existing NDC if available or create a new one
		ndc = &cur_if->pnan_info->ndc_info[0];
		generate_ndc_bitmap(final_bitmap, &ndc->slot);
		memcpy(ndc->ndc_id, wfa_ndc_id, ETH_ALEN);
		*ndl_status = NDP_NDL_STATUS_CONTINUED;
	}

#if 0
    /*Match channel*/
    if(cur_if->pnan_info->self_avail_info.band_entry_potential)
    {
        /*Band entry so accept proposed channel*/
        final_channel = entry->channels[0];
        final_opclass = entry->op_class;
    }
    else
    {
        final_channel = nan_scan_chan_list(cur_if->pnan_info->self_avail_info.entry_potential[0].channels,
                                            entry->channels[0]);
        final_opclass = entry->op_class;
    }
    INFO("NAN2 : final op class (%x),final op chan(%u) ", final_opclass, final_channel);
#endif

	/*If Explicit counter required*/
	if (cur_if->pnan_info->counter_proposal_needed) {
		u8 final_channel = 0, final_opclass = 0, entry_index = 0,
		   rc = 0;
		int k;

		for (k = 0; k < MAX_SCHEDULE_ENTRIES; k++) {
			/*Pick up opclass and channel in a band*/
			if ((cur_if->pnan_info->a_band) &&
			    (schedule[k].op_class > 86)) {
				final_opclass = schedule[k].op_class;
				final_channel = schedule[k].op_chan;
				break;
			}

			/*Pick up opclass and channel in 2.4 GHZ band*/
			if ((!cur_if->pnan_info->a_band) &&
			    (schedule[k].op_class == 81)) {
				final_opclass = schedule[k].op_class;
				final_channel = schedule[k].op_chan;
				break;
			}
		}
		/*If no a_band entry(5GHZ) is published by
		 peer pickup the 1st available entry*/
		if (cur_if->pnan_info->a_band && !final_channel) {
			final_opclass = schedule[0].op_class;
			final_channel = schedule[0].op_chan;
		}

		INFO("NAN2 : final op class (%x),final op chan(%u) ",
		     final_opclass, final_channel);
		/*Counter the time bitmap*/
		if (cur_if->pnan_info->counter_proposal.availability_map[0])
			final_bitmap = cur_if->pnan_info->counter_proposal
					       .availability_map[0];
		else {
			/*if (final_bitmap & 0xffff)
			    final_bitmap &=  0xfefe;
			else
			    final_bitmap &=  0xfefe0000;*/
			/*Generate counter proposal bitmap*/
			if (NAN_ERR_SUCCESS !=
			    nan_generate_counter_proposal(cur_if, &final_bitmap,
							  &rc)) {
				/* Reject the proposal as counter proposal
				 * generation failed */
				INFO("NAN2 : NDP request rejected at counter proposal");
				*ndp_status = NDP_NDL_STATUS_REJECTED;
				*ndl_status = NDP_NDL_STATUS_REJECTED;
				*reason = rc;
				return NAN_ERR_SUCCESS;
			}
		}

		INFO("NAN2 : Countered bit map set(%x) ", final_bitmap);

		/*Counter the channel*/
		if (cur_if->pnan_info->counter_proposal.op_class &&
		    cur_if->pnan_info->counter_proposal.op_chan) {
			final_channel =
				cur_if->pnan_info->counter_proposal.op_chan;
			final_opclass =
				cur_if->pnan_info->counter_proposal.op_class;
		}
		INFO("NAN2 : Countered op class (%x),final op chan(%u) ",
		     final_opclass, final_channel);

		if (!(ndc->slot & final_bitmap)) {
			ERR("NAN2 : Select other than default NDC slot");
			generate_ndc_bitmap(final_bitmap, &ndc->slot);
		}
		/*In case of counter proposal clear the exisiting schedule
		  entries and Set final map in schedule entry*/
		memset(schedule, 0,
		       (MAX_SCHEDULE_ENTRIES * sizeof(struct nan_schedule)));
		num_entries = entry_index + 1;
		schedule[entry_index].availability_map[0] = final_bitmap;
		schedule[entry_index].op_chan = final_channel;
		schedule[entry_index].op_class = final_opclass;
		schedule[entry_index].time_bitmap_count = num_entries;
		schedule[entry_index].start_offset[0] = 0;
		schedule[entry_index].period = NDP_TIME_BM_PERIOD_512;
		final_chan_set = TRUE;
		ERR("NAN2 : Bitmap 0x%x and channel %u set at index %u",
		    final_bitmap, final_channel, entry_index);
	}

	/*Set NDP NDL Status*/

	if (!final_bitmap || !final_chan_set) {
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_INVALID_AVAILABILITY;
	} else if (final_bitmap != peer_bitmap) {
		if (cur_if->pnan_info->counter_proposal_needed) {
			/* initial proposal explicitly countered*/
			*ndp_status = (cur_if->pnan_info->confirm_required ||
				       cur_if->pnan_info->security_required) ?
					      NDP_NDL_STATUS_CONTINUED :
					      NDP_NDL_STATUS_ACCEPTED;

			*ndl_status = NDP_NDL_STATUS_CONTINUED;
		} else if (!combined_immutable_bitmap ||
			   ((combined_immutable_bitmap & final_bitmap) ==
			    combined_immutable_bitmap)) {
			/* Final bitmap is subset of initial proposal &
			   Immutable schedule, if present was accepted */
			*ndp_status = (cur_if->pnan_info->confirm_required ||
				       cur_if->pnan_info->security_required) ?
					      NDP_NDL_STATUS_CONTINUED :
					      NDP_NDL_STATUS_ACCEPTED;

			*ndl_status = NDP_NDL_STATUS_ACCEPTED;
		} else {
			/* Reject if intersection of our and peer's
			   bitmap doesn't include immutable slots */
			*ndp_status = NDP_NDL_STATUS_REJECTED;
			*ndl_status = NDP_NDL_STATUS_REJECTED;
			*reason = NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE;
		}
	} else {
		*ndp_status = (cur_if->pnan_info->confirm_required ||
			       cur_if->pnan_info->security_required) ?
				      NDP_NDL_STATUS_CONTINUED :
				      NDP_NDL_STATUS_ACCEPTED;

		*ndl_status = NDP_NDL_STATUS_ACCEPTED;
	}
	ERR("NAN2 : Final ndp status %u and ndl status %u", *ndp_status,
	    *ndl_status);

	/*Copy final bitmap and channel to committed entries */
	if (ndp)
		ndp->ndp_slots = final_bitmap;
	if (*ndl_status == NDP_NDL_STATUS_ACCEPTED) {
		int i = 0, j = 0, k = 0;
		for (i = 0; i < num_entries; i++) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				if (!(cur_if->pnan_info->self_avail_info
					      .committed_valid &
				      (1 << j))) {
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.time_bitmap_count =
						schedule[i].time_bitmap_count;
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.period = schedule[i].period;
					for (k = 0;
					     k < schedule[i].time_bitmap_count;
					     k++) {
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[j]
							.time_bitmap[k] =
							schedule[i]
								.availability_map
									[k];
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[j]
							.start_offset[k] =
							schedule[i]
								.start_offset[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    schedule[i]
							    .availability_map[k],
						    schedule[i].start_offset[k]);
					}
					cur_if->pnan_info->self_avail_info
						.committed_valid |= (1 << j);
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.op_class =
						schedule[i].op_class;
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.channels[0] =
						schedule[i].op_chan;
					ERR("NAN2 : Committed entry set at index %u , channel %u",
					    j, schedule[i].op_chan);
					break;
				}
			}
		}
		/*
		    cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap
		   = final_bitmap;
		    cur_if->pnan_info->self_avail_info.committed_valid |=
		   ENTRY0_VALID;
		    cur_if->pnan_info->self_avail_info.entry_committed[0].op_class
		   = final_opclass;
		    cur_if->pnan_info->self_avail_info.entry_committed[0].channels[0]
		   = final_channel;

		    cur_if->pnan_info->self_avail_info.seq_id++;
		    cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap
			&=
		   ~(cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap);
		*/
	} else if (*ndl_status == NDP_NDL_STATUS_CONTINUED) {
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.time_bitmap[0] = schedule[0].availability_map[0];
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.time_bitmap_count = schedule[0].time_bitmap_count;
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.start_offset[0] = schedule[0].start_offset[0];
		cur_if->pnan_info->self_avail_info.entry_conditional[0].period =
			schedule[0].period;
		cur_if->pnan_info->self_avail_info.conditional_valid |=
			ENTRY0_VALID;
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.op_class = schedule[0].op_class;
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.channels[0] = schedule[0].op_chan;
		ERR("NAN2 : Conditional entry bitmap added 0x%x , at index 0",
		    cur_if->pnan_info->self_avail_info.entry_conditional[0]
			    .time_bitmap[0]);
	}

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_schedule_resp(struct mwu_iface_info *cur_if)
{
	peer_availability_info *peer_info = NULL, *self_info = NULL;
	nan_availability_attr *avail_attr = NULL;
	struct nan_generic_buf *ndp_resp_buf = NULL;
	int i = 0, j = 0, k = 0;
	u32 combined_committed_map = 0;

	ndp_resp_buf = cur_if->pnan_info->rx_ndp_resp;
	avail_attr = (nan_availability_attr *)nan_get_nan_attr(
		NAN_AVAILABILITY_ATTR, ndp_resp_buf->buf, ndp_resp_buf->size);
	peer_info = &cur_if->pnan_info->peer_avail_info;
	self_info = &cur_if->pnan_info->self_avail_info;

	nan_parse_availability(avail_attr, peer_info);

	/*Copy multiple parsed entries to committed entries*/
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if ((peer_info->committed_valid & (1 << i))) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				if (!(self_info->committed_valid & (1 << j))) {
					self_info->entry_committed[j]
						.time_bitmap_count =
						peer_info->entry_committed[i]
							.time_bitmap_count;
					self_info->entry_committed[j].period =
						peer_info->entry_committed[i]
							.period;
					for (k = 0;
					     k < peer_info->entry_committed[i]
							 .time_bitmap_count;
					     k++) {
						self_info->entry_committed[j]
							.time_bitmap[k] =
							peer_info
								->entry_committed
									[i]
								.time_bitmap[k] &
							NAN_POTENTIAL_BITMAP;
						self_info->entry_committed[j]
							.start_offset[k] =
							peer_info
								->entry_committed
									[i]
								.start_offset[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    peer_info
							    ->entry_committed[i]
							    .time_bitmap[k],
						    peer_info
							    ->entry_committed[i]
							    .start_offset[k]);
						combined_committed_map |=
							peer_info
								->entry_committed
									[i]
								.time_bitmap[k];
					}
					self_info->committed_valid |= (1 << j);
					self_info->entry_committed[j].op_class =
						peer_info->entry_committed[i]
							.op_class;
					self_info->entry_committed[j]
						.channels[0] =
						peer_info->entry_committed[i]
							.channels[0];
					ERR("NAN2 : Committed entry set at index %u with  channel %u",
					    j,
					    self_info->entry_committed[j]
						    .channels[0]);
					break;
				}
			}
		}
		/*We accept the counter proposal by default*/
		if ((peer_info->conditional_valid & (1 << i))) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				if (!(self_info->committed_valid & (1 << j))) {
					self_info->entry_committed[j]
						.time_bitmap_count =
						peer_info->entry_conditional[i]
							.time_bitmap_count;
					self_info->entry_committed[j].period =
						peer_info->entry_conditional[i]
							.period;
					for (k = 0;
					     k < peer_info->entry_conditional[i]
							 .time_bitmap_count;
					     k++) {
						self_info->entry_committed[j]
							.time_bitmap[k] =
							peer_info
								->entry_conditional
									[i]
								.time_bitmap[k] &
							NAN_POTENTIAL_BITMAP;
						self_info->entry_committed[j]
							.start_offset[k] =
							peer_info
								->entry_conditional
									[i]
								.start_offset[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    peer_info
							    ->entry_conditional[i]
							    .time_bitmap[k],
						    peer_info
							    ->entry_conditional[i]
							    .start_offset[k]);
						combined_committed_map |=
							peer_info
								->entry_conditional
									[i]
								.time_bitmap[k];
					}
					self_info->committed_valid |= (1 << j);
					self_info->entry_committed[j].op_class =
						peer_info->entry_conditional[i]
							.op_class;
					self_info->entry_committed[j]
						.channels[0] =
						peer_info->entry_conditional[i]
							.channels[0];
					ERR("NAN2 : Committed entry set at index %u with  channel %u",
					    j,
					    self_info->entry_committed[j]
						    .channels[0]);
					break;
				}
			}
		}
	}
	/*
	if(peer_info->committed_valid & ENTRY0_VALID){
	    parsed_entry = &peer_info->entry_committed[0];
	}
	if(peer_info->conditional_valid & ENTRY0_VALID){
	    parsed_entry = &peer_info->entry_conditional[0];
	}

	self_info->entry_committed[0].time_bitmap = parsed_entry->time_bitmap;
	self_info->entry_committed[0].op_class = parsed_entry->op_class;
	self_info->entry_committed[0].channels[0] = parsed_entry->channels[0];
	ERR("NAN2 : Final bit map set(%x) ",
	self_info->entry_committed[0].time_bitmap); INFO("NAN2 : final op class
	(%x),final op chan(%u) ", self_info->entry_committed[0].op_class,
	self_info->entry_committed[0].channels[0]); */

	cur_if->pnan_info->self_avail_info.seq_id++;
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] &=
		~(combined_committed_map);
	cur_if->pnan_info->self_avail_info.time_bitmap_present_potential = 1;
	cur_if->pnan_info->self_avail_info.committed_changed = 1;
	cur_if->pnan_info->self_avail_info.potential_changed = 1;
	cur_if->pnan_info->avail_attr.len = 0;
	ERR("NAN2 : entried updated");

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_schedule_confirm(struct mwu_iface_info *cur_if)
{
	peer_availability_info *peer_info = NULL, *self_info = NULL;
	nan_availability_attr *avail_attr = NULL;
	// struct nan_generic_buf *ndp_conf_buf = NULL;
	// avail_entry_t *parsed_entry = NULL;
	int i = 0, j = 0, k = 0;

	// ndp_conf_buf =  cur_if->pnan_info->rx_ndp_conf;

	/*return if availability attribute absent*/
	if (!avail_attr)
		return NAN_ERR_INVAL;

	peer_info = &cur_if->pnan_info->peer_avail_info;
	self_info = &cur_if->pnan_info->self_avail_info;

	/*We accept the counter proposal by default*/
	/*
	if(peer_info->committed_valid & ENTRY0_VALID){
	    parsed_entry = &peer_info->entry_committed[0];
	}
	*/

	/*Copy multiple parsed entries to committed entries*/
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if ((peer_info->committed_valid & (1 << i))) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				if (!(self_info->committed_valid & (1 << j))) {
					self_info->entry_committed[j]
						.time_bitmap_count =
						peer_info->entry_committed[i]
							.time_bitmap_count;
					self_info->entry_committed[j].period =
						peer_info->entry_committed[i]
							.period;
					for (k = 0;
					     k < peer_info->entry_committed[i]
							 .time_bitmap_count;
					     k++) {
						self_info->entry_committed[j]
							.time_bitmap[k] =
							peer_info
								->entry_committed
									[i]
								.time_bitmap[k] &
							NAN_POTENTIAL_BITMAP;
						self_info->entry_committed[j]
							.start_offset[k] =
							peer_info
								->entry_committed
									[i]
								.start_offset[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    peer_info
							    ->entry_committed[i]
							    .time_bitmap[k],
						    peer_info
							    ->entry_committed[i]
							    .start_offset[k]);
					}
					self_info->committed_valid |= (1 << j);
					self_info->entry_committed[j].op_class =
						peer_info->entry_committed[i]
							.op_class;
					self_info->entry_committed[j]
						.channels[0] =
						peer_info->entry_committed[i]
							.channels[0];
					ERR("NAN2 : Committed entry set at index %u with  channel %u",
					    j,
					    self_info->entry_committed[j]
						    .channels[0]);
					break;
				}
			}
		}
	}
	/*
	    self_info->entry_committed[0].time_bitmap =
	   parsed_entry->time_bitmap; self_info->entry_committed[0].op_class =
	   parsed_entry->op_class; self_info->entry_committed[0].channels[0] =
	   parsed_entry->channels[0]; ERR("NAN2 : Final bit map set(%x) ",
	   self_info->entry_committed[0].time_bitmap); INFO("NAN2 : final op
	   class (%x),final op chan(%u) ",
	   self_info->entry_committed[0].op_class,
	   self_info->entry_committed[0].channels[0]); */
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_ndp_req(struct mwu_iface_info *cur_if, u8 *ndl_status,
				 u8 *ndp_status, u8 *reason)
{
	nan_availability_attr *avail_attr = NULL, *avail_attr2 = NULL;
	nan_ndl_attr *ndl_attr = NULL;
	nan_ndl_qos_attr *ndl_qos = NULL;
	nan_ndp_attr *ndp_attr = NULL;
	nan_ndc_attr *ndc_attr = NULL;
	u8 ndc_present = FALSE, peer_ndc = FALSE, i = 0, j = 0;
	struct nan_generic_buf *ndp_req_buf;
	peer_availability_info *peer_info = &cur_if->pnan_info->peer_avail_info;
	u32 final_bitmap = 0, peer_bitmap = 0,
	    ndc_time_bitmap[MAX_SCHEDULE_ENTRIES] = {0},
	    immutable_bitmap[MAX_SCHEDULE_ENTRIES] = {0};
	u32 combined_ndc_bitmap = 0, combined_immutable_bitmap = 0;
	struct nan_schedule schedule[MAX_SCHEDULE_ENTRIES];
	u8 final_chan_set = 0, num_entries = 0, immutable_bitmap_count = 0,
	   ndc_time_bitmap_count = 0, attr_id = 0;
	NDC_INFO *ndc = NULL;
	NDP_INFO *ndp = NULL;
	NDL_INFO *ndl = NULL;
	u8 *buff = NULL;

	memset(schedule, 0, sizeof(schedule));

	*reason = NAN_REASON_CODE_DEFAULT;

	ndp_req_buf = cur_if->pnan_info->rx_ndp_req;

	a_band_flag = cur_if->pnan_info->a_band ? TRUE : FALSE;

	/* Process NDL attribute */
	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(
		NAN_NDL_ATTR, ndp_req_buf->buf, ndp_req_buf->size);

	if (ndl_attr->type_and_status.type == NDP_NDL_TYPE_REQUEST) {
		if (ndl_attr->type_and_status.status !=
		    NDP_NDL_STATUS_CONTINUED) {
			ERR("NAN2 : NDL Attr status(%d) should be Continued(0) in NDL Request",
			    ndl_attr->type_and_status.status);
			return NAN_ERR_INVAL;
		}
		if (ndl_attr->ndl_ctrl.immutable_sched_present) {
			immutable_schedule *imm_sched;
			u8 i, j;

			ERR("NAN2 : Immutable entry present");
			j = 0;

			if (ndl_attr->ndl_ctrl.ndl_peer_id_present)
				j++;
			if (ndl_attr->ndl_ctrl.max_idle_period_present)
				j += 2;

			imm_sched =
				(immutable_schedule *)&ndl_attr->optional[j];
			// for (i = 0; i < imm_sched->no_of_entries; i++)
			//  For now assume only 1 immutable entry
			for (i = 0; i < 1; i++) {
				immutable_bitmap_count =
					nan_parse_generic_time_bitmap(
						&imm_sched->entry[i]
							 .time_bitmap_ctrl,
						imm_sched->entry[i]
							.time_bitmap_len,
						imm_sched->entry[i].bitmap,
						immutable_bitmap);
			}
			for (i = 0; i < immutable_bitmap_count; i++) {
				combined_immutable_bitmap |=
					immutable_bitmap[i];
			}
		}
		if (ndl_attr->ndl_ctrl.ndc_attr_present) {
			ERR("NAN2 : NDC for this NDL is present");
			ndc_present = TRUE;
			// set flag to consider the NDC if propsed
		} else {
			cur_if->pnan_info->ndc_proposed = 1;
		}
	}

	/* Process NDL QoS attribute*/
	ndl_qos = (nan_ndl_qos_attr *)nan_get_nan_attr(
		NAN_NDL_QOS_ATTR, ndp_req_buf->buf, ndp_req_buf->size);
	if (ndl_qos) {
		peer_info->qos.min_slots = ndl_qos->min_slots;
		memcpy(&peer_info->qos.max_latency, ndl_qos->max_latency,
		       sizeof(peer_info->qos.max_latency));
	} else {
		peer_info->qos.min_slots = NAN_QOS_MIN_SLOTS;
		peer_info->qos.max_latency = NAN_QOS_MAX_LATENCY;
	}

	/* Process NDC attribute*/
	if (ndc_present) {
		ndc_attr = (nan_ndc_attr *)nan_get_nan_attr(
			NAN_NDC_ATTR, ndp_req_buf->buf, ndp_req_buf->size);

		if (ndc_attr->sched_ctrl.proposed_ndc) {
			ndc_time_bitmap_count = nan_parse_generic_time_bitmap(
				&ndc_attr->time_bitmap_ctrl,
				ndc_attr->time_bitmap_len, ndc_attr->bitmap,
				ndc_time_bitmap);
			for (i = 0; i < ndc_time_bitmap_count; i++) {
				combined_ndc_bitmap |= ndc_time_bitmap[i];
			}

			ERR("NAN2 : Proposed NDC with slots - %x",
			    combined_ndc_bitmap);
			// Check whether NDC slot conflicts with our own
			// availability
			// if (ndc_time_bitmap == 0x02 || ndc_time_bitmap ==
			// 0x0200)
			if (combined_ndc_bitmap) {
				peer_ndc = TRUE;
			} else {
				ERR("NAN2 : Proposed NDC(%x) abnormal! Propose preferred/existing NDC slot",
				    combined_ndc_bitmap);
			}
		}
	}

	/*Extract NDP attribute*/
	if ((!cur_if->pnan_info->ndpe_not_present) &&
	    (cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(
			NAN_NDP_EXT_ATTR, ndp_req_buf->buf, ndp_req_buf->size);
	else
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(
			NAN_NDP_ATTR, ndp_req_buf->buf, ndp_req_buf->size);

	/*Update Peer_mac, peer/initiator ndi, dialog token, ndp_id*/
	ndc = &cur_if->pnan_info->ndc_info[0];
	ndl = &ndc->ndl_info[0];
	memcpy(ndl->peer_mac, ndp_req_buf->buf, ETH_ALEN);
	if (ndl_attr->ndl_ctrl.ndl_peer_id_present)
		ndl->peer_id = ndl_attr->optional[0];

	ndp = &ndl->ndp_info[0]; // NDP Identifier, 0 is reserved
	if (ndp_attr) {
		memcpy(ndp->initiator_ndi, ndp_attr->initiator_ndi, ETH_ALEN);
		memcpy(ndp->peer_ndi, ndp_attr->initiator_ndi, ETH_ALEN);
		memcpy(ndp->responder_ndi, cur_if->device_mac_addr, ETH_ALEN);
		ndp->ndp_id = ndp_attr->ndp_id;
		ndp->dialogue_token = ndp_attr->dialogue_token;
	}
	/*Extract and parse availability attribute*/
	avail_attr = (nan_availability_attr *)nan_get_nan_attr(
		NAN_AVAILABILITY_ATTR, ndp_req_buf->buf, ndp_req_buf->size);
	peer_info = &cur_if->pnan_info->peer_avail_info;
	nan_parse_availability(avail_attr, peer_info);

	ERR("NAN2 : parsed entries - committed %d, conditional %d, potential %d",
	    cur_if->pnan_info->peer_avail_info.committed_valid,
	    cur_if->pnan_info->peer_avail_info.conditional_valid,
	    cur_if->pnan_info->peer_avail_info.potential_valid);

	/* parse second avail_attr if exists */
	buff = (u8 *)avail_attr;
	buff += (avail_attr->len + NAN_ATTR_HDR_LEN);

	avail_attr2 = (nan_availability_attr *)buff;
	attr_id = *(buff);
	if (attr_id == NAN_AVAILABILITY_ATTR)
		nan_parse_availability(avail_attr2, peer_info);

	ERR("NAN2 : parsed entries - committed %d, conditional %d, potential %d",
	    cur_if->pnan_info->peer_avail_info.committed_valid,
	    cur_if->pnan_info->peer_avail_info.conditional_valid,
	    cur_if->pnan_info->peer_avail_info.potential_valid);

	/*Get the schedule proposal into single combined bitmap*/
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if (cur_if->pnan_info->peer_avail_info.conditional_valid &
		    (1 << i)) {
			for (j = 0; j < cur_if->pnan_info->peer_avail_info
						.entry_conditional[i]
						.time_bitmap_count;
			     j++) {
				peer_bitmap |=
					cur_if->pnan_info->peer_avail_info
						.entry_conditional[i]
						.time_bitmap[j];
			}
		}
		if (cur_if->pnan_info->peer_avail_info.committed_valid &
		    (1 << i)) {
			for (j = 0; j < cur_if->pnan_info->peer_avail_info
						.entry_committed[i]
						.time_bitmap_count;
			     j++) {
				peer_bitmap |=
					cur_if->pnan_info->peer_avail_info
						.entry_committed[i]
						.time_bitmap[j];
			}
		}
	}
	ERR("NAN2 : peer's combined bitmap (0x%x)!", peer_bitmap);

	/* Sanity on NAN2 parameters of NDP Req */
	if (combined_immutable_bitmap &&
	    ((combined_immutable_bitmap & peer_bitmap) !=
	     combined_immutable_bitmap)) {
		/* Sanity on immutable */
		ERR("NAN2 : Immutable(0x%x) is not a subset of peer's bitmap (0x%x)!",
		    combined_immutable_bitmap, peer_bitmap);
		INFO("NAN2 : NDP request rejected");
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE;
		/*Clear off peers entries*/
		peer_info->committed_valid = 0;
		peer_info->conditional_valid = 0;
		peer_info->potential_valid = 0;

		return NAN_ERR_SUCCESS;
	}

	/* Sanity on NDC */
	if (peer_ndc &&
	    ((combined_ndc_bitmap & peer_bitmap) != combined_ndc_bitmap)) {
		ERR("NAN2 : NDC(0x%x) is not a subset of NA(0x%x). Reject.",
		    combined_ndc_bitmap, peer_bitmap);
		INFO("NAN2 : NDP request rejected");
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_INVALID_AVAILABILITY;
		/*Clear off peers entries*/
		peer_info->committed_valid = 0;
		peer_info->conditional_valid = 0;
		peer_info->potential_valid = 0;

		return NAN_ERR_SUCCESS;
	}
	/* Sanity on NDPE attribute */
	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported) &&
	    (cur_if->pnan_info->ndpe_not_present)) {
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_NDP_REJECTED;
		INFO("NDP Response:Rejected, NDPE not present in NDP request");
		/*Clear off peers entries*/
		peer_info->committed_valid = 0;
		peer_info->conditional_valid = 0;
		peer_info->potential_valid = 0;
	}

	/*Validate avail entries & detect overlap*/
	if (nan_validate_avail_entries(cur_if)) {
		INFO("NAN2 : NDP request rejected");
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_INVALID_AVAILABILITY;
		/*Clear off peers entries*/
		peer_info->committed_valid = 0;
		peer_info->conditional_valid = 0;
		peer_info->potential_valid = 0;

		return NAN_ERR_SUCCESS;
	}
	/*Sanity on NAN2 parameters of NDP Req Over*/

	/*Process peers proposal and prepare response*/
	nan_process_peer_proposal(cur_if, schedule, &num_entries, &final_bitmap,
				  &final_chan_set);
	ERR("NAN2 : Number of final entries(%u) ", num_entries);
	ERR("NAN2 : Combined Final bit map set(%x) ", final_bitmap);

	/*Join or select new NDC*/
	if (peer_ndc &&
	    ((combined_ndc_bitmap & peer_bitmap) == combined_ndc_bitmap)) {
		// Joining the Peer NDC
		ndc = &cur_if->pnan_info->ndc_info[0];
		ndc->slot = combined_ndc_bitmap;
		memcpy(ndc->ndc_id, ndc_attr->ndc_id, ETH_ALEN);
		*ndl_status = NDP_NDL_STATUS_ACCEPTED;
		ERR("NAN2 : Accepted proposed NDC with slot - %x",
		    combined_ndc_bitmap);
	} else {
		u8 wfa_ndc_id[6] = {0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00};
		// TODO Use existing NDC if available or create a new one
		ndc = &cur_if->pnan_info->ndc_info[0];
		generate_ndc_bitmap(final_bitmap, &ndc->slot);
		memcpy(ndc->ndc_id, wfa_ndc_id, ETH_ALEN);
		*ndl_status = NDP_NDL_STATUS_CONTINUED;
	}

#if 0
    /*Match channel*/
    if(cur_if->pnan_info->self_avail_info.band_entry_potential)
    {
        /*Band entry so accept proposed channel*/
        final_channel = entry->channels[0];
        final_opclass = entry->op_class;
    }
    else
    {
        final_channel = nan_scan_chan_list(cur_if->pnan_info->self_avail_info.entry_potential[0].channels,
                                            entry->channels[0]);
        final_opclass = entry->op_class;
    }
    INFO("NAN2 : final op class (%x),final op chan(%u) ", final_opclass, final_channel);
#endif

	/*If Explicit counter required*/
	if (cur_if->pnan_info->counter_proposal_needed) {
		u8 final_channel = 0, final_opclass = 0, entry_index = 0,
		   rc = 0;
		int k;

		for (k = 0; k < MAX_SCHEDULE_ENTRIES; k++) {
			/*Pick up opclass and channel in a band*/
			if ((cur_if->pnan_info->a_band) &&
			    (schedule[k].op_class > 86)) {
				final_opclass = schedule[k].op_class;
				final_channel = schedule[k].op_chan;
				break;
			}

			/*Pick up opclass and channel in 2.4 GHZ band*/
			if ((!cur_if->pnan_info->a_band) &&
			    (schedule[k].op_class < 86)) {
				final_opclass = schedule[k].op_class;
				final_channel = schedule[k].op_chan;
				break;
			}
		}
		/*If no a_band entry(5GHZ) is published by
		 peer pickup the 1st available entry*/
		if (cur_if->pnan_info->a_band && !final_channel) {
			final_opclass = schedule[0].op_class;
			final_channel = schedule[0].op_chan;
		}

		INFO("NAN2 : final op class (%x),final op chan(%u) ",
		     final_opclass, final_channel);
		/*Counter the time bitmap*/
		if (cur_if->pnan_info->counter_proposal.availability_map[0]) {
			final_bitmap = cur_if->pnan_info->counter_proposal
					       .availability_map[0];
		} else {
			/*if (final_bitmap & 0xffff)
			    final_bitmap &=  0xfefe;
			else
			    final_bitmap &=  0xfefe0000;*/

			final_bitmap = 0;

			// Add the immutable schedule
			final_bitmap |= combined_immutable_bitmap;

			// Add the ndc schedule
			final_bitmap |= ndc->slot;

			/*Generate counter proposal bitmap*/
			if (NAN_ERR_SUCCESS !=
			    nan_generate_counter_proposal(cur_if, &final_bitmap,
							  &rc)) {
				/* Reject the proposal as counter proposal
				 * generation failed */
				*ndp_status = NDP_NDL_STATUS_REJECTED;
				*ndl_status = NDP_NDL_STATUS_REJECTED;
				*reason = rc;
				return NAN_ERR_SUCCESS;
			}
		}

		INFO("NAN2 : Countered bit map set(%x) ", final_bitmap);

		/*Counter the channel*/
		if (cur_if->pnan_info->counter_proposal.op_class &&
		    cur_if->pnan_info->counter_proposal.op_chan) {
			final_channel =
				cur_if->pnan_info->counter_proposal.op_chan;
			final_opclass =
				cur_if->pnan_info->counter_proposal.op_class;
		}
		INFO("NAN2 : Countered op class (%x),final op chan(%u) ",
		     final_opclass, final_channel);

		if (!(ndc->slot & final_bitmap)) {
			ERR("NAN2 : Select other than default NDC slot");
			generate_ndc_bitmap(final_bitmap, &ndc->slot);
		}
		/*In case of counter proposal clear the exisiting schedule
		  entries and Set final map in schedule entry*/
		memset(schedule, 0,
		       (MAX_SCHEDULE_ENTRIES * sizeof(struct nan_schedule)));
		num_entries = entry_index + 1;
		schedule[entry_index].availability_map[0] = final_bitmap;
		schedule[entry_index].op_chan = final_channel;
		schedule[entry_index].op_class = final_opclass;
		schedule[entry_index].time_bitmap_count = num_entries;
		schedule[entry_index].start_offset[0] = 0;
		schedule[entry_index].period = NDP_TIME_BM_PERIOD_512;
		final_chan_set = TRUE;
		ERR("NAN2 : Bitmap 0x%x and channel %u set at index %u",
		    final_bitmap, final_channel, entry_index);
	}

	/*Set NDP NDL Status*/

	if (!final_bitmap || !final_chan_set) {
		*ndp_status = NDP_NDL_STATUS_REJECTED;
		*ndl_status = NDP_NDL_STATUS_REJECTED;
		*reason = NAN_REASON_CODE_INVALID_AVAILABILITY;
	} else if (final_bitmap != peer_bitmap) {
		if (cur_if->pnan_info->counter_proposal_needed) {
			/* initial proposal explicitly countered*/
			*ndp_status = (cur_if->pnan_info->confirm_required ||
				       cur_if->pnan_info->security_required) ?
					      NDP_NDL_STATUS_CONTINUED :
					      NDP_NDL_STATUS_ACCEPTED;

			*ndl_status = NDP_NDL_STATUS_CONTINUED;
		} else if (!combined_immutable_bitmap ||
			   ((combined_immutable_bitmap & final_bitmap) ==
			    combined_immutable_bitmap)) {
			/* Final bitmap is subset of initial proposal &
			   Immutable schedule, if present was accepted */
			*ndp_status = (cur_if->pnan_info->confirm_required ||
				       cur_if->pnan_info->security_required) ?
					      NDP_NDL_STATUS_CONTINUED :
					      NDP_NDL_STATUS_ACCEPTED;

			*ndl_status = NDP_NDL_STATUS_ACCEPTED;
		} else {
			/* Reject if intersection of our and peer's
			   bitmap doesn't include immutable slots */
			*ndp_status = NDP_NDL_STATUS_REJECTED;
			*ndl_status = NDP_NDL_STATUS_REJECTED;
			*reason = NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE;
		}
	} else {
		*ndp_status = (cur_if->pnan_info->confirm_required ||
			       cur_if->pnan_info->security_required) ?
				      NDP_NDL_STATUS_CONTINUED :
				      NDP_NDL_STATUS_ACCEPTED;

		*ndl_status = NDP_NDL_STATUS_ACCEPTED;
	}
	ERR("NAN2 : Final ndp status %u and ndl status %u", *ndp_status,
	    *ndl_status);

	/*Clear off peers entries*/
	peer_info->committed_valid = 0;
	peer_info->conditional_valid = 0;
	peer_info->potential_valid = 0;

	/*Copy final bitmap and channel to committed entries */
	ndp->ndp_slots = final_bitmap;
	if (*ndl_status == NDP_NDL_STATUS_ACCEPTED) {
		int i = 0, j = 0, k = 0;
		u32 combined_committed_map = 0;
		for (i = 0; i < num_entries; i++) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				combined_committed_map = 0;
				if (!(cur_if->pnan_info->self_avail_info
					      .committed_valid &
				      (1 << j))) {
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.time_bitmap_count =
						schedule[i].time_bitmap_count;
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.period = schedule[i].period;
					for (k = 0;
					     k < schedule[i].time_bitmap_count;
					     k++) {
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[j]
							.time_bitmap[k] =
							schedule[i]
								.availability_map
									[k];
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[j]
							.start_offset[k] =
							schedule[i]
								.start_offset[k];
						combined_committed_map |=
							schedule[i]
								.availability_map
									[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    schedule[i]
							    .availability_map[k],
						    schedule[i].start_offset[k]);
					}
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.combined_time_bitmap =
						combined_committed_map;
					cur_if->pnan_info->self_avail_info
						.committed_valid |= (1 << j);
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.op_class =
						schedule[i].op_class;
					cur_if->pnan_info->self_avail_info
						.entry_committed[j]
						.channels[0] =
						schedule[i].op_chan;
					ERR("NAN2 : Committed entry set at index %u with bitmap 0x%x, channel %u",
					    j,
					    cur_if->pnan_info->self_avail_info
						    .entry_committed[j]
						    .combined_time_bitmap,
					    schedule[i].op_chan);
					break;
				}
			}
		}
		/*
		    cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap
		   = final_bitmap;
		    cur_if->pnan_info->self_avail_info.committed_valid |=
		   ENTRY0_VALID;
		    cur_if->pnan_info->self_avail_info.entry_committed[0].op_class
		   = final_opclass;
		    cur_if->pnan_info->self_avail_info.entry_committed[0].channels[0]
		   = final_channel;
		*/
		cur_if->pnan_info->self_avail_info.seq_id++;
		cur_if->pnan_info->self_avail_info.entry_potential[0]
			.time_bitmap[0] &= ~(combined_committed_map);
		cur_if->pnan_info->self_avail_info
			.time_bitmap_present_potential = 1;
		cur_if->pnan_info->self_avail_info.committed_changed = 1;
		cur_if->pnan_info->self_avail_info.potential_changed = 1;
		cur_if->pnan_info->avail_attr.len = 0;
		ERR("NAN2 : entried updated");

	} else if (*ndl_status == NDP_NDL_STATUS_CONTINUED) {
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.time_bitmap[0] = schedule[0].availability_map[0];
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.time_bitmap_count = schedule[0].time_bitmap_count;
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.start_offset[0] = schedule[0].start_offset[0];
		cur_if->pnan_info->self_avail_info.entry_conditional[0].period =
			schedule[0].period;
		cur_if->pnan_info->self_avail_info.conditional_valid |=
			ENTRY0_VALID;
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.op_class = schedule[0].op_class;
		cur_if->pnan_info->self_avail_info.entry_conditional[0]
			.channels[0] = schedule[0].op_chan;
		ERR("NAN2 : Conditional entry bitmap added 0x%x , at index 0",
		    cur_if->pnan_info->self_avail_info.entry_conditional[0]
			    .time_bitmap[0]);

		// cur_if->pnan_info->self_avail_info.seq_id++;

		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.time_bitmap[0] = schedule[0].availability_map[0];
		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.time_bitmap_count = schedule[0].time_bitmap_count;
		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.start_offset[0] = schedule[0].start_offset[0];
		cur_if->pnan_info->self_avail_info.entry_committed[0].period =
			schedule[0].period;
		cur_if->pnan_info->self_avail_info.committed_valid |=
			ENTRY0_VALID;
		cur_if->pnan_info->self_avail_info.entry_committed[0].op_class =
			schedule[0].op_class;
		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.channels[0] = schedule[0].op_chan;
		ERR("NAN2 : Conditional entry bitmap added 0x%x , at index 0",
		    cur_if->pnan_info->self_avail_info.entry_committed[0]
			    .time_bitmap[0]);
	}

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_ndp_resp(struct mwu_iface_info *cur_if)
{
	peer_availability_info *peer_info = NULL, *self_info = NULL;
	nan_availability_attr *avail_attr = NULL, *avail_attr2 = NULL;
	struct nan_generic_buf *ndp_resp_buf = NULL;
	nan_ndl_qos_attr *ndl_qos;
	int i = 0, j = 0, k = 0;
	u8 attr_id = 0;
	u32 combined_committed_map = 0;
	u8 *buff;

	ndp_resp_buf = cur_if->pnan_info->rx_ndp_resp;
	avail_attr = (nan_availability_attr *)nan_get_nan_attr(
		NAN_AVAILABILITY_ATTR, ndp_resp_buf->buf, ndp_resp_buf->size);
	peer_info = &cur_if->pnan_info->peer_avail_info;
	self_info = &cur_if->pnan_info->self_avail_info;

	nan_parse_availability(avail_attr, peer_info);

	/* parse the 2nd avail_attr if preset */
	buff = (u8 *)avail_attr;
	buff += (avail_attr->len + NAN_ATTR_HDR_LEN);

	avail_attr2 = (nan_availability_attr *)buff;
	attr_id = *(buff);
	if (attr_id == NAN_AVAILABILITY_ATTR)
		nan_parse_availability(avail_attr2, peer_info);

	/*Copy multiple parsed entries to committed entries*/
	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if ((peer_info->committed_valid & (1 << i))) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				if (!(self_info->committed_valid & (1 << j))) {
					combined_committed_map = 0;
					self_info->entry_committed[j]
						.time_bitmap_count =
						peer_info->entry_committed[i]
							.time_bitmap_count;
					self_info->entry_committed[j].period =
						peer_info->entry_committed[i]
							.period;
					for (k = 0;
					     k < peer_info->entry_committed[i]
							 .time_bitmap_count;
					     k++) {
						self_info->entry_committed[j]
							.time_bitmap[k] =
							peer_info
								->entry_committed
									[i]
								.time_bitmap[k] &
							NAN_POTENTIAL_BITMAP;
						self_info->entry_committed[j]
							.start_offset[k] =
							peer_info
								->entry_committed
									[i]
								.start_offset[k];
						combined_committed_map |=
							peer_info
								->entry_committed
									[i]
								.time_bitmap[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    peer_info
							    ->entry_committed[i]
							    .time_bitmap[k],
						    peer_info
							    ->entry_committed[i]
							    .start_offset[k]);
					}
					self_info->entry_committed[j]
						.combined_time_bitmap =
						combined_committed_map &
						NAN_POTENTIAL_BITMAP;
					self_info->committed_valid |= (1 << j);
					self_info->entry_committed[j].op_class =
						peer_info->entry_committed[i]
							.op_class;
					self_info->entry_committed[j]
						.channels[0] =
						peer_info->entry_committed[i]
							.channels[0];
					ERR("NAN2 : Committed entry set at index %u with channel %u",
					    j,
					    self_info->entry_committed[j]
						    .channels[0]);
					break;
				}
			}
		}
		/*We accept the counter proposal by default*/
		if ((peer_info->conditional_valid & (1 << i))) {
			for (j = 0; j < MAX_SCHEDULE_ENTRIES; j++) {
				if (!(self_info->committed_valid & (1 << j))) {
					combined_committed_map = 0;
					self_info->entry_committed[j]
						.time_bitmap_count =
						peer_info->entry_conditional[i]
							.time_bitmap_count;
					self_info->entry_committed[j].period =
						peer_info->entry_conditional[i]
							.period;
					for (k = 0;
					     k < peer_info->entry_conditional[i]
							 .time_bitmap_count;
					     k++) {
						self_info->entry_committed[j]
							.time_bitmap[k] =
							peer_info
								->entry_conditional
									[i]
								.time_bitmap[k] &
							NAN_POTENTIAL_BITMAP;
						self_info->entry_committed[j]
							.start_offset[k] =
							peer_info
								->entry_conditional
									[i]
								.start_offset[k];
						combined_committed_map |=
							peer_info
								->entry_conditional
									[i]
								.time_bitmap[k];
						ERR("NAN2 : Committed entry bitmap added 0x%x , with offset %u ",
						    peer_info
							    ->entry_conditional[i]
							    .time_bitmap[k],
						    peer_info
							    ->entry_conditional[i]
							    .start_offset[k]);
					}
					self_info->entry_committed[j]
						.combined_time_bitmap =
						combined_committed_map &
						NAN_POTENTIAL_BITMAP;
					self_info->committed_valid |= (1 << j);
					self_info->entry_committed[j].op_class =
						peer_info->entry_conditional[i]
							.op_class;
					self_info->entry_committed[j]
						.channels[0] =
						peer_info->entry_conditional[i]
							.channels[0];
					ERR("NAN2 : Committed entry set at index %u with channel %u",
					    j,
					    self_info->entry_committed[j]
						    .channels[0]);
					break;
				}
			}
		}
	}

	cur_if->pnan_info->self_avail_info.seq_id++;
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] &=
		~(combined_committed_map);
	cur_if->pnan_info->self_avail_info.time_bitmap_present_potential = 1;
	cur_if->pnan_info->self_avail_info.committed_changed = 1;
	cur_if->pnan_info->self_avail_info.potential_changed = 1;
	cur_if->pnan_info->avail_attr.len = 0;
	ERR("NAN2 : entried updated");

	/*Clear off peers entries*/
	peer_info->committed_valid = 0;
	peer_info->conditional_valid = 0;
	/*
	if(peer_info->committed_valid & ENTRY0_VALID){
	    parsed_entry = &peer_info->entry_committed[0];
	}
	if(peer_info->conditional_valid & ENTRY0_VALID){
	    parsed_entry = &peer_info->entry_conditional[0];
	}

	self_info->entry_committed[0].time_bitmap = parsed_entry->time_bitmap;
	self_info->entry_committed[0].op_class = parsed_entry->op_class;
	self_info->entry_committed[0].channels[0] = parsed_entry->channels[0];
	ERR("NAN2 : Final bit map set(%x) ",
	self_info->entry_committed[0].time_bitmap); INFO("NAN2 : final op class
	(%x),final op chan(%u) ", self_info->entry_committed[0].op_class,
	self_info->entry_committed[0].channels[0]); */

	/* Process NDL QoS attribute in case countered*/
	ndl_qos = (nan_ndl_qos_attr *)nan_get_nan_attr(
		NAN_NDL_QOS_ATTR, ndp_resp_buf->buf, ndp_resp_buf->size);
	if (ndl_qos) {
		peer_info->qos.min_slots = ndl_qos->min_slots;
		memcpy(&peer_info->qos.max_latency, ndl_qos->max_latency,
		       sizeof(peer_info->qos.max_latency));
	} else {
		peer_info->qos.min_slots = NAN_QOS_MIN_SLOTS;
		peer_info->qos.max_latency = NAN_QOS_MAX_LATENCY;
	}

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_ndp_confirm(struct mwu_iface_info *cur_if)
{
	peer_availability_info *peer_info = NULL;
	// peer_availability_info *self_info = NULL;
	nan_availability_attr *avail_attr = NULL;
	struct nan_generic_buf *ndp_conf_buf = NULL;

	ndp_conf_buf = cur_if->pnan_info->rx_ndp_conf;
	avail_attr = (nan_availability_attr *)nan_get_nan_attr(
		NAN_AVAILABILITY_ATTR, ndp_conf_buf->buf, ndp_conf_buf->size);

	/*return if availability attribute absent*/
	if (!avail_attr)
		return NAN_ERR_INVAL;

	peer_info = &cur_if->pnan_info->peer_avail_info;
	// self_info = &cur_if->pnan_info->self_avail_info;

	nan_parse_availability(avail_attr, peer_info);

	/*
	    if(peer_info->committed_valid & ENTRY0_VALID){
		parsed_entry = &peer_info->entry_committed[0];
	    }

	    for(i=0; i<MAX_SCHEDULE_ENTRIES; i++)
	    {
		if((peer_info->committed_valid & (1<<i)))
		{
		    for(j=0; j<MAX_SCHEDULE_ENTRIES; j++)
		    {
			if(!(self_info->committed_valid & (1<<j)))
			{
			    combined_committed_map = 0;
			    self_info->entry_committed[j].time_bitmap_count =
	   peer_info->entry_committed[i].time_bitmap_count;
			    self_info->entry_committed[j].period =
	   peer_info->entry_committed[i].period; for(k = 0; k <
	   peer_info->entry_committed[i].time_bitmap_count; k++)
			    {
				self_info->entry_committed[j].time_bitmap[k] =
	   peer_info->entry_committed[i].time_bitmap[k] & NAN_POTENTIAL_BITMAP;
				self_info->entry_committed[j].start_offset[k] =
	   peer_info->entry_committed[i].start_offset[k]; combined_committed_map
	   |= peer_info->entry_committed[i].time_bitmap[k];
			    }
			    self_info->entry_committed[j].combined_time_bitmap =
	   combined_committed_map & NAN_POTENTIAL_BITMAP;
			    self_info->committed_valid |= (1<<j);
			    self_info->entry_committed[j].op_class =
	   peer_info->entry_committed[i].op_class;
			    self_info->entry_committed[j].channels[0] =
	   peer_info->entry_committed[i].channels[0]; ERR("NAN2 : Committed
	   entry set at index %u with channel %u", j,
	   self_info->entry_committed[j].channels[0]); break;
			}
		    }
		}
	    }
	*/
	cur_if->pnan_info->self_avail_info.seq_id++;
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] &=
		~(cur_if->pnan_info->self_avail_info.entry_committed[0]
			  .time_bitmap[0]);
	cur_if->pnan_info->self_avail_info.time_bitmap_present_potential = 1;
	cur_if->pnan_info->self_avail_info.committed_changed = 1;
	cur_if->pnan_info->self_avail_info.potential_changed = 1;
	cur_if->pnan_info->avail_attr.len = 0;
	ERR("NAN2 : entried updated");

	/*Clear off peers entries*/
	peer_info->committed_valid = 0;
	peer_info->conditional_valid = 0;
	peer_info->potential_valid = 0;
	/*
	    self_info->entry_committed[0].time_bitmap =
	   parsed_entry->time_bitmap; self_info->entry_committed[0].op_class =
	   parsed_entry->op_class; self_info->entry_committed[0].channels[0] =
	   parsed_entry->channels[0]; ERR("NAN2 : Final bit map set(%x) ",
	   self_info->entry_committed[0].time_bitmap); INFO("NAN2 : final op
	   class (%x),final op chan(%u) ",
	   self_info->entry_committed[0].op_class,
	   self_info->entry_committed[0].channels[0]); */
	return NAN_ERR_SUCCESS;
}

static u32 nan_calculate_ndp_frame_size(u8 *buffer, int size)
{
	/*
	u8 mandatory_attr[]={
			     NAN_ELEMENT_CONTAINER_ATTR,
			     NAN_NDL_ATTR,
			     NAN_NDC_ATTR,
			     NAN_DEVICE_CAPABILITY_ATTR,
			     NAN_NDL_QOS_ATTR,
			     NAN_UNALIGNED_SCHEDULE_ATTR,
			     NAN_AVAILABILITY_ATTR,
			     NAN_NDP_ATTR,
			     NAN_CIPHER_SUITE_INFO_ATTR,
			     NAN_SEC_CONTEXT_INFO_ATTR,
			     NAN_SHARED_KEY_DESC_ATTR,
			    };
			    */
	u8 *tmp_ptr = NULL;
	u32 bytes_left = size;
	u32 len = 0;
	tlv_header *tlv = NULL;
	/*
	for(i = 0; i < sizeof(mandatory_attr); i++)
	{
	    if(attr = nan_get_nan_attr(mandatory_attr[i], buffer, size))
	    {
	       len += *(u16 *)&attr[1] + 3;
	       INFO("DBG ATTR LENGTH %d",len);
	    }
	}*/

	tmp_ptr = buffer + NDP_ATTR_START_OFFSET;
	bytes_left -= NDP_ATTR_START_OFFSET;

	while (bytes_left > NAN_ATTR_HDR_LEN) {
		tlv = (tlv_header *)tmp_ptr;
		INFO("DBG ATTR type %d LENGTH %d", tlv->type, tlv->length);

		if ((tlv->length - NAN_ATTR_HDR_LEN) > bytes_left)
			return len;

		len += tlv->length + NAN_ATTR_HDR_LEN;
		bytes_left -= (tlv->length + NAN_ATTR_HDR_LEN);
		INFO("BYTES left %d", bytes_left);
		tmp_ptr += tlv->length + NAN_ATTR_HDR_LEN;
	}
	return len;
}

/*Function to validate the received NDP request*/
enum nan_error nan_validate_ndp_request(struct mwu_iface_info *cur_if,
					u8 *buffer, int size)
{
	u8 mandatory_attr[] = {NAN_NDP_ATTR, NAN_ELEMENT_CONTAINER_ATTR,
			       NAN_NDL_ATTR, NAN_DEVICE_CAPABILITY_ATTR,
			       NAN_AVAILABILITY_ATTR};
	u8 security_attr[] = {
		NAN_CIPHER_SUITE_INFO_ATTR,
		NAN_SEC_CONTEXT_INFO_ATTR,
		NAN_SHARED_KEY_DESC_ATTR,
	};
	u8 i = 0;

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported)) {
		if (NULL == nan_get_nan_attr(NAN_NDP_EXT_ATTR, buffer, size)) {
			cur_if->pnan_info->ndpe_not_present = TRUE;
		}
		/** skip/ignore ndp_attr in mandatory_attr[] */
		i++;
	}

	for (; i < sizeof(mandatory_attr); i++) {
		if (NULL == nan_get_nan_attr(mandatory_attr[i], buffer, size)) {
			return NAN_ERR_INVAL;
		}
	}
	/*If security is enabled validate security attrs*/
	if (cur_if->pnan_info->security_required) {
		for (i = 0; i < sizeof(security_attr); i++) {
			if (NULL ==
			    nan_get_nan_attr(security_attr[i], buffer, size)) {
				return NAN_ERR_INVAL;
			}
		}
	}
	return NAN_ERR_SUCCESS;
}

/*Function to validate the received NDP response*/
enum nan_error nan_validate_ndp_response(struct mwu_iface_info *cur_if,
					 u8 *buffer, int size)
{
	u8 mandatory_attr[] = {NAN_NDP_ATTR,
			       NAN_NDL_ATTR,
			       NAN_NDC_ATTR,
			       NAN_ELEMENT_CONTAINER_ATTR,
			       NAN_DEVICE_CAPABILITY_ATTR,
			       NAN_AVAILABILITY_ATTR};
	u8 security_attr[] = {
		NAN_CIPHER_SUITE_INFO_ATTR,
		NAN_SHARED_KEY_DESC_ATTR,
	};
	u8 i = 0, trunc = 0;
	nan_ndl_attr *ndl_attr;
	nan_ndp_attr *ndp_attr;

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported)) {
		if (NULL == nan_get_nan_attr(NAN_NDP_EXT_ATTR, buffer, size)) {
			cur_if->pnan_info->ndpe_not_present = TRUE;
		}
		/** skip/ignore ndp_attr in mandatory_attr[] */
		i++;
	}
	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(NAN_NDL_ATTR, buffer, size);

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_EXT_ATTR,
							    buffer, size);
	else
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_ATTR,
							    buffer, size);

	if (ndl_attr &&
	    ndl_attr->type_and_status.status == NDP_NDL_STATUS_REJECTED) {
		/*For status rejected truncate the mandatory_attr array*/
		trunc = 4;
	}
	for (; i < (sizeof(mandatory_attr) - trunc); i++) {
		if (NULL == nan_get_nan_attr(mandatory_attr[i], buffer, size)) {
			return NAN_ERR_INVAL;
		}
	}
	/*If security is enabled validate security attrs*/
	if (cur_if->pnan_info->security_required) {
		if (ndp_attr && ndp_attr->type_and_status.status ==
					NDP_NDL_STATUS_REJECTED) {
			return NAN_ERR_SUCCESS;
		} else if (ndp_attr && ndp_attr->type_and_status.status ==
					       NDP_NDL_STATUS_ACCEPTED) {
			ERR("NAN2:NDP Status = Accepted, is invalid for security enabled ndp response");
			return NAN_ERR_INVAL;
		}
		for (i = 0; i < sizeof(security_attr); i++) {
			if (NULL ==
			    nan_get_nan_attr(security_attr[i], buffer, size)) {
				return NAN_ERR_INVAL;
			}
		}
	}
	return NAN_ERR_SUCCESS;
}

/*Function to validate the received NDP confirm*/
enum nan_error nan_validate_ndp_confirm(struct mwu_iface_info *cur_if,
					u8 *buffer, int size)
{
	u8 mandatory_attr[] = {NAN_NDL_ATTR, NAN_NDC_ATTR,
			       NAN_AVAILABILITY_ATTR};
	u8 i, trunc = 0;
	nan_ndl_attr *ndl_attr =
		(nan_ndl_attr *)nan_get_nan_attr(NAN_NDL_ATTR, buffer, size);
	tlv_header *ptmp_ndp_attr = NULL;

	if (cur_if->pnan_info->confirm_required ||
	    cur_if->pnan_info->security_required) {
		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported))
			ptmp_ndp_attr = nan_get_nan_attr(NAN_NDP_EXT_ATTR,
							 buffer, size);
		else
			ptmp_ndp_attr =
				nan_get_nan_attr(NAN_NDP_ATTR, buffer, size);

		if (NULL == ptmp_ndp_attr) {
			return NAN_ERR_INVAL;
		}
		if (cur_if->pnan_info->security_required) {
			if (NULL == nan_get_nan_attr(NAN_SHARED_KEY_DESC_ATTR,
						     buffer, size)) {
				return NAN_ERR_INVAL;
			}
		}
	} else {
		if (ndl_attr && ndl_attr->type_and_status.status ==
					NDP_NDL_STATUS_REJECTED) {
			/*For status rejected truncate the mandatory_attr
			 * array*/
			trunc = 2;
		}
		for (i = 0; i < (sizeof(mandatory_attr) - trunc); i++) {
			if (NULL ==
			    nan_get_nan_attr(mandatory_attr[i], buffer, size)) {
				return NAN_ERR_INVAL;
			}
		}
	}
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_handle_ndp_req(struct mwu_iface_info *cur_if,
				  unsigned char *buffer, int size)
{
	struct ndp_data_indication *data_ind = NULL;
	nan_ndp_attr *ndp_attr = NULL;
	u8 publish_id = 0, *responder_ndi = NULL, *service_info = NULL;
	int service_len = 0, data_ind_len = 0;

	nan_device_capability_attr *dev_cap_attr = NULL;
	// struct ndp_ndpe_data *ndpe_data_event = NULL;
	// int ndpe_data_event_len = 0;

	if (cur_if->pnan_info->ndpe_attr_supported) {
		dev_cap_attr = (nan_device_capability_attr *)nan_get_nan_attr(
			NAN_DEVICE_CAPABILITY_ATTR, buffer, size);
		if (dev_cap_attr) {
			if (dev_cap_attr->capabilities.ndpe_attr_supported) {
				cur_if->pnan_info->peer_avail_info_published
					.ndpe_attr_supported = TRUE;
				mwu_hexdump(MSG_INFO, "PeerMAC", buffer,
					    ETH_ALEN);
				INFO("Peer supports NDPE attribute (from ndp request)");
			}
		}
	}

	if (NAN_ERR_SUCCESS != nan_validate_ndp_request(cur_if, buffer, size)) {
		ERR("NDP frame validation failed");
		return NAN_ERR_INVAL;
	}
	ERR("NDP request validation successful");

	/*Extract Peer MAC and maintain a copy of ndp request buffer for
	  later use. Free this buffer after transmitting ndp response*/
	memcpy(cur_if->pnan_info->peer_mac, buffer, ETH_ALEN);
	cur_if->pnan_info->rx_ndp_req =
		(struct nan_generic_buf *)malloc(size + NAN_BUF_HDR_SIZE);
	memcpy(cur_if->pnan_info->rx_ndp_req->buf, buffer, size);
	cur_if->pnan_info->rx_ndp_req->size = size + NAN_BUF_HDR_SIZE;

	if (cur_if->pnan_info->security_required) {
		cur_if->pnan_info->nan_security.m1 =
			(struct nan_sec_buf *)malloc(size + NAN_BUF_HDR_SIZE);
		memcpy(cur_if->pnan_info->nan_security.m1->buf,
		       cur_if->pnan_info->rx_ndp_req->buf, size);
		cur_if->pnan_info->nan_security.m1->size =
			nan_calculate_ndp_frame_size(buffer, size) +
			7 /*Category ,Action, OUI, subtype, NAF type*/
			+ NAN_BUF_HDR_SIZE;
		ERR("DBG RCVD M1 SIZE %lu",
		    cur_if->pnan_info->nan_security.m1->size -
			    NAN_BUF_HDR_SIZE);

		/*Extract M1 EAPOL and copy nonce value in cur_if*/
		eapol_key_frame *rcvd_m1_eapol;
		nan_shared_key_desc_attr *temp_shared_key_desc;
		temp_shared_key_desc =
			(nan_shared_key_desc_attr *)nan_get_nan_attr(
				NAN_SHARED_KEY_DESC_ATTR,
				cur_if->pnan_info->nan_security.m1->buf,
				cur_if->pnan_info->nan_security.m1->size);

		if (temp_shared_key_desc) {
			cur_if->pnan_info->instance_id =
				temp_shared_key_desc->instance_id;
			rcvd_m1_eapol =
				(eapol_key_frame *)
					temp_shared_key_desc->rsna_key_desc;
			memcpy(cur_if->pnan_info->nan_security.peer_key_nonce,
			       rcvd_m1_eapol->key_nonce, NONCE_LEN);
		} else {
			ERR("NDP request does not contain NAN_SHARED_KEY_DESC_ATTR attribute");
		}
	}

	/*Change state to INIT*/
	change_ndp_state(cur_if, NDP_INIT);

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported) &&
	    (cur_if->pnan_info->ndpe_not_present)) {
		INFO("Peer supports NDPE attr, NDP req does not contain NDPE attr");
		// return NAN_ERR_INVAL;
	}
	/*Extract NDP attribute*/
	if (!cur_if->pnan_info->ndpe_not_present &&
	    (cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported)) {
		INFO("****here: 1");
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_EXT_ATTR,
							    buffer, size);
		memcpy((cur_if->pnan_info->peer_ndp_attr), ndp_attr,
		       (ndp_attr->len + NAN_ATTR_HDR_LEN));
	} else {
		INFO("****here: 2");
		/*Extract NDP attribute*/
		ndp_attr = (struct _nan_ndp_attr *)nan_get_nan_attr(
			NAN_NDP_ATTR, buffer, size);
	}
	cur_if->pnan_info->confirm_required = ndp_attr->ndp_ctrl.confirm_req;

	nan_get_optional_ndp_attr(ndp_attr, &publish_id, &responder_ndi,
				  &service_info, &service_len);
	INFO("Optional attributes extracted");

	/*Populate Data Indication fields to be sent in Event Buffer*/
	data_ind_len = sizeof(struct ndp_data_indication) + service_len;
	data_ind = (struct ndp_data_indication *)malloc(data_ind_len);
	memset(data_ind, 0, data_ind_len);
	data_ind->type = NDP_TYPE_UNICAST;
	data_ind->publish_id = publish_id;
	data_ind->ndp_id = ndp_attr->ndp_id;
	memcpy(data_ind->initiator_addr, ndp_attr->initiator_ndi, ETH_ALEN);
	if (responder_ndi)
		memcpy(data_ind->responder_addr, responder_ndi, ETH_ALEN);
	data_ind->service_info.len = service_len;
	if (service_info && service_len)
		memcpy(data_ind->service_info.data, service_info, service_len);
	INFO("Data Indication Event buffer populated");

	/*Send event to app*/
	nan_send_ndp_event(cur_if, NAN_EVENT_DATA_INDICATION, (u8 *)data_ind,
			   data_ind_len);
	FREE(data_ind);
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_handle_ndp_resp(struct mwu_iface_info *cur_if,
				   unsigned char *buffer, int size)
{
	nan_ndp_attr *ndp_attr = NULL;
	nan_ndl_attr *ndl_attr = NULL;
	nan_ndc_attr *ndc_attr = NULL;
	NDP_INFO *ndp = &cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0];
	u8 publish_id = 0, *responder_ndi = NULL, *service_info = NULL;
	u8 *tlv_list = NULL;
	u16 total_tlv_len = 0;
	tlv_header *tlv = NULL;
	ipv6_link_local_tlv *ipv6_tlv = NULL;
	service_info_tlv *pserv_info = NULL;
	transport_port_sub_attr *tport_sub_attr = NULL;
	transport_protocol_sub_attr *tprotocol_sub_attr = NULL;
	u16 processed_tlv_len = 0, i = 0;
	int service_len = 0;
	enum nan_error ret = NAN_ERR_SUCCESS;

	if (NAN_ERR_SUCCESS !=
	    nan_validate_ndp_response(cur_if, buffer, size)) {
		ERR("NDP frame validation failed");
		change_ndp_state(cur_if, NDP_IDLE);
		return NAN_ERR_INVAL;
	}
	ERR("NDP response validation successful");

	/* Maintain a copy of ndp response buffer for later use.
	   Free this buffer after transmitting ndp response */
	cur_if->pnan_info->rx_ndp_resp =
		(struct nan_generic_buf *)malloc(size + NAN_BUF_HDR_SIZE);
	memcpy(cur_if->pnan_info->rx_ndp_resp->buf, buffer, size);
	cur_if->pnan_info->rx_ndp_resp->size = size + NAN_BUF_HDR_SIZE;

	/*Extract NDP and NDL Attribute*/
	if ((!cur_if->pnan_info->ndpe_not_present) &&
	    (cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_EXT_ATTR,
							    buffer, size);
	else
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_ATTR,
							    buffer, size);

	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(NAN_NDL_ATTR, buffer, size);
	ndc_attr = (nan_ndc_attr *)nan_get_nan_attr(NAN_NDC_ATTR, buffer, size);

	/** Included to handle negative test cases in WFA testplan */
	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported) &&
	    (cur_if->pnan_info->ndpe_not_present)) {
		ndp_attr->type_and_status.status = NDP_NDL_STATUS_REJECTED;
		ndl_attr->type_and_status.status = NDP_NDL_STATUS_REJECTED;
		INFO("Peer supports NDPE attr, NDP resp does not contain NDPE attr");
	}

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported) &&
	    (!cur_if->pnan_info->ndpe_not_present)) {
		if (NULL != nan_get_nan_attr(NAN_NDP_ATTR, buffer, size)) {
			ndp_attr->type_and_status.status =
				NDP_NDL_STATUS_REJECTED;
			ndl_attr->type_and_status.status =
				NDP_NDL_STATUS_REJECTED;
			INFO("NDPE support present, but response contains NDP attr as well");
		}
	}
	cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0].dialogue_token =
		ndp_attr->dialogue_token;
	nan_get_optional_ndp_attr(ndp_attr, &publish_id, &responder_ndi,
				  &service_info, &service_len);
	/*Copy responder NDI from NDP resp*/
	if (responder_ndi) {
		memcpy(ndp->peer_ndi, responder_ndi, ETH_ALEN);
		memcpy(ndp->responder_ndi, responder_ndi, ETH_ALEN);
	}

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported)) {
		if (!cur_if->pnan_info->ndpe_not_present) {
			nan_get_optional_ndp_ext_attr(ndp_attr, &tlv_list,
						      &total_tlv_len);
			INFO("Optional(ndpe) attributes extracted");
			while (processed_tlv_len < total_tlv_len) {
				tlv = (tlv_header *)(tlv_list +
						     processed_tlv_len);
				switch (tlv->type) {
				case TLV_TYPE_IPV6_LINK_LOCAL:
					ipv6_tlv = (ipv6_link_local_tlv *)tlv;
					INFO("ipv6 identifier(resp):");
					for (i = 0;
					     i < IPV6_IFACE_IDENTIFIER_LEN; i++)
						INFO("%x",
						     ipv6_tlv->iface_id[i]);
					break;
				case TLV_TYPE_SERVICE_INFO:
					pserv_info = (service_info_tlv *)tlv;
					INFO("serv info (resp):111");
					INFO("oui[0]: 0x%x",
					     pserv_info->oui[0]);
					INFO("oui[1]: 0x%x",
					     pserv_info->oui[1]);
					INFO("oui[2]: 0x%x",
					     pserv_info->oui[2]);
					INFO("is_wfa_oui(): 0x%x",
					     is_wfa_oui(pserv_info->oui));
					INFO("serv_prot_type 0x%x",
					     pserv_info->service_protocol_type);
					if ((is_wfa_oui(pserv_info->oui)) &&
					    (pserv_info->service_protocol_type ==
					     SERVICE_PROTOCOL_GENERIC)) {
						INFO("serv info (resp):222");
						nan_process_service_spc_info(
							pserv_info
								->service_spc_info,
							&tport_sub_attr,
							&tprotocol_sub_attr,
							(pserv_info->header
								 .length -
							 WFA_OUI_LEN -
							 sizeof(u8)));
						if (tport_sub_attr)
							INFO("transport port: %d",
							     tport_sub_attr
								     ->transport_port);
						if (tprotocol_sub_attr)
							INFO("transport portcol: %d",
							     tprotocol_sub_attr
								     ->transport_portocol);
					}
					break;
				default:
					break;
				}
				processed_tlv_len +=
					(tlv->length + sizeof(tlv_header));
			}
		}
	}

	if (cur_if->pnan_info->security_required) {
		nan_shared_key_desc_attr *temp_shared_key_desc;
		eapol_key_frame *rcvd_m2_eapol;

		cur_if->pnan_info->nan_security.m2 =
			(struct nan_sec_buf *)malloc(size + NAN_BUF_HDR_SIZE);
		memcpy(cur_if->pnan_info->nan_security.m2->buf,
		       cur_if->pnan_info->rx_ndp_resp->buf, size);
		cur_if->pnan_info->nan_security.m2->size =
			nan_calculate_ndp_frame_size(buffer, size) +
			7 /*Category ,Action, OUI, subtype, NAF type*/
			+ NAN_BUF_HDR_SIZE;
		ERR("DBG RCVD M2 SIZE %lu",
		    cur_if->pnan_info->nan_security.m2->size -
			    NAN_BUF_HDR_SIZE);

		temp_shared_key_desc =
			(nan_shared_key_desc_attr *)nan_get_nan_attr(
				NAN_SHARED_KEY_DESC_ATTR,
				cur_if->pnan_info->nan_security.m2->buf,
				cur_if->pnan_info->nan_security.m2->size);

		if (temp_shared_key_desc) {
			int akmp = BIT(16);
			rcvd_m2_eapol =
				(eapol_key_frame *)
					temp_shared_key_desc->rsna_key_desc;

			if (check_replay_counter_values(rcvd_m2_eapol) == 0) {
				u8 rcvd_mic[MIC_LEN] = {0};
				u8 temp_mic[MIC_LEN] = {0};

				nan_generate_ptk(
					cur_if->pnan_info->nan_security.pmk,
					(size_t)cur_if->pnan_info->nan_security
						.pmk_len,
					cur_if->pnan_info->ndc_info[0]
						.ndl_info[0]
						.ndp_info[0]
						.initiator_ndi,
					cur_if->pnan_info->ndc_info[0]
						.ndl_info[0]
						.ndp_info[0]
						.responder_ndi,
					cur_if->pnan_info->nan_security
						.local_key_nonce,
					rcvd_m2_eapol->key_nonce,
					(u8 *)(&(cur_if->pnan_info->nan_security
							 .ptk_buf)),
					akmp);

				mwu_hexdump(MSG_INFO,
					    "rcvd_m2_eapol->key_nonce",
					    rcvd_m2_eapol->key_nonce,
					    NONCE_LEN);
				mwu_hexdump(MSG_INFO, "rcvd_m2_eapol",
					    (u8 *)rcvd_m2_eapol,
					    sizeof(eapol_key_frame));
				memcpy(cur_if->pnan_info->nan_security
					       .peer_key_nonce,
				       rcvd_m2_eapol->key_nonce, NONCE_LEN);

				memcpy(rcvd_mic, rcvd_m2_eapol->key_mic,
				       MIC_LEN);
				mwu_hexdump(MSG_INFO,
					    "rcvd_m2_eapol->key_mic \n",
					    rcvd_m2_eapol->key_mic, MIC_LEN);
				mwu_hexdump(MSG_INFO, "recvd mic \n", rcvd_mic,
					    MIC_LEN);
				memset(rcvd_m2_eapol->key_mic, 0x0, MIC_LEN);

				eapol_key_mic(
					cur_if->pnan_info->nan_security.ptk_buf
						.kck,
					16,
					&cur_if->pnan_info->nan_security.m2
						 ->buf[10], /*Exclude initial 10
							       bytes mac rssi*/
					cur_if->pnan_info->nan_security.m2->size -
						NAN_BUF_HDR_SIZE, /*Exclude
								     buffer
								     header size
								     + mac
								     rssi*/
					temp_mic);
				mwu_hexdump(
					MSG_INFO,
					"NAN2 DBG MIC generated after M2 is rcvd \n",
					temp_mic, MIC_LEN);
				mwu_hexdump(MSG_INFO,
					    "NAN2 Recvd MIC in frame \n",
					    rcvd_mic, MIC_LEN);
				if (memcmp(rcvd_mic, temp_mic, MIC_LEN) != 0) {
					ERR("Invalid EAPOL-Key MIC");
					INFO("MIC validation has failed for rcvd M2\n");

					/*Change state to IDLE*/
					change_ndp_state(cur_if, NDP_IDLE);
					ret = NAN_ERR_INVAL;
					ERR("Failed to send NDP response");
					return ret;
				}
			} else {
				INFO("M1tx M2rx not successful\n");
				INFO("Replay counter values of M1 and M2 do not match, changing state to NDP_IDLE\n");

				/*Change state to IDLE*/
				change_ndp_state(cur_if, NDP_IDLE);

				ret = NAN_ERR_INVAL;
				ERR("Failed to send NDP response");
				return ret;
			}
		} else {
			ERR("NDP response does not contain NAN_SHARED_KEY_DESC_ATTR attribute");
			INFO("Changing state to NDP_IDLE\n");

			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);

			ret = NAN_ERR_INVAL;
			ERR("Failed to send NDP response");
			return ret;
		}
	}

	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(NAN_NDL_ATTR, buffer, size);
	ndc_attr = (nan_ndc_attr *)nan_get_nan_attr(NAN_NDC_ATTR, buffer, size);

	/*extract NDC bitmap if proposed*/
	if (ndc_attr && ndc_attr->sched_ctrl.proposed_ndc) {
		NDC_INFO *ndc = NULL;
		u32 ndc_time_bitmap[MAX_SCHEDULE_ENTRIES],
			combined_ndc_bitmap = 0;
		u8 ndc_time_bitmap_count = 0;
		u8 i = 0;

		mwu_hexdump(MSG_INFO, "ndc_attr", (u8 *)ndc_attr, 25);

		memset(ndc_time_bitmap, 0,
		       (sizeof(u32) * MAX_SCHEDULE_ENTRIES));
		ndc_time_bitmap_count = nan_parse_generic_time_bitmap(
			&ndc_attr->time_bitmap_ctrl, ndc_attr->time_bitmap_len,
			ndc_attr->bitmap, ndc_time_bitmap);
		for (i = 0; i < ndc_time_bitmap_count; i++) {
			combined_ndc_bitmap |= ndc_time_bitmap[i];
		}
		ERR("NAN2 : Received proposed NDC with slots - %x",
		    combined_ndc_bitmap);
		ndc = &cur_if->pnan_info->ndc_info[0];
		ndc->slot = combined_ndc_bitmap;
	}

	/*Take further actions based on NDP & NDL status*/
	if (cur_if->pnan_info->confirm_required ||
	    cur_if->pnan_info->security_required) {
		u8 tx_ndl_status = NDP_NDL_STATUS_ACCEPTED;
		u8 tx_ndp_status = NDP_NDL_STATUS_ACCEPTED;
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_CONTINUED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_ACCEPTED)) {
			/*Parse recvd ndp resp*/
			nan_parse_ndp_resp(cur_if);

			/*Change state to CONFIRM_PENDING*/
			change_ndp_state(cur_if, NDP_CONFIRM_PENDING);

			if (cur_if->pnan_info->security_required) {
				tx_ndp_status = NDP_NDL_STATUS_CONTINUED;
			}
			/*Generate and send NDP confirm*/
			ret = nan_send_ndp_confirm(cur_if, ndl_attr, ndp_attr,
						   tx_ndl_status,
						   tx_ndp_status);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP response");
				return ret;
			}

			if (!cur_if->pnan_info->security_required) {
				/*Change state to CONFIRM_PENDING*/
				change_ndp_state(cur_if, NDP_CONNECTED);

				/*Populate Data Confirmation fields to be sent
				 * in Event Buffer*/
				nan_send_data_confirm_event(cur_if, buffer,
							    size);

				/** Send NDPE event to app */
				INFO("NDPE data event#2");
				if ((cur_if->pnan_info->ndpe_attr_supported) &&
				    (cur_if->pnan_info->peer_avail_info_published
					     .ndpe_attr_supported))
					nan_send_ndpe_data_event(
						cur_if, ipv6_tlv,
						tport_sub_attr,
						tprotocol_sub_attr);
				/*Send the bitmap to firmware*/
				nancmd_set_final_bitmap(cur_if, NAN_NDL);

				/*Open nan interface*/
				nan_iface_open();

				/*Clear peers published entries*/
				nan_clear_peer_avail_published_entries(cur_if);
			}
		}
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_CONTINUED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_CONTINUED)) {
			/*Change state to CONFIRM_PENDING*/
			change_ndp_state(cur_if, NDP_CONFIRM_PENDING);

			/*This is a case of counter proposal*/
			nan_parse_ndp_resp(cur_if);

			if (cur_if->pnan_info->security_required) {
				tx_ndp_status = NDP_NDL_STATUS_CONTINUED;
			}
			/*Generate and send NDP confirm*/
			ret = nan_send_ndp_confirm(cur_if, ndl_attr, ndp_attr,
						   tx_ndl_status,
						   tx_ndp_status);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP response");
				return ret;
			}

			if (!cur_if->pnan_info->security_required) {
				/*Change state to CONFIRM_PENDING*/
				change_ndp_state(cur_if, NDP_CONNECTED);

				/*Populate Data Confirmation fields to be sent
				 * in Event Buffer*/
				nan_send_data_confirm_event(cur_if, buffer,
							    size);

				/** Send NDPE event to app */
				INFO("NDPE data event#3");
				if ((cur_if->pnan_info->ndpe_attr_supported) &&
				    (cur_if->pnan_info->peer_avail_info_published
					     .ndpe_attr_supported))
					nan_send_ndpe_data_event(
						cur_if, ipv6_tlv,
						tport_sub_attr,
						tprotocol_sub_attr);
				/*Send the bitmap to firmware*/
				nancmd_set_final_bitmap(cur_if, NAN_NDL);

				/*Open nan interface*/
				nan_iface_open();

				/*Clear peers published entries*/
				nan_clear_peer_avail_published_entries(cur_if);
			}
		}
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_REJECTED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_REJECTED)) {
			if ((cur_if->pnan_info->ndpe_attr_supported) &&
			    (cur_if->pnan_info->peer_avail_info_published
				     .ndpe_attr_supported) &&
			    (cur_if->pnan_info->ndpe_not_present)) {
				INFO("Sending the Confirm:Rejected, NDPE validation failed for NDP response");
				ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
				/*Generate and send NDP confirm*/
				ret = nan_send_ndp_confirm(cur_if, ndl_attr,
							   ndp_attr,
							   tx_ndl_status,
							   tx_ndp_status);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to send NDP response");
					return ret;
				}
			}
			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);

			/* for the special case of negative test case */
			cur_if->pnan_info->self_avail_info.entry_conditional[0]
				.time_bitmap[0] = NAN_DEFAULT_BITMAP;
			cur_if->pnan_info->self_avail_info.conditional_valid |=
				ENTRY0_VALID;
			cur_if->pnan_info->self_avail_info.seq_id++;
		}
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_ACCEPTED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_ACCEPTED)) {
			/*Need to revisit this condition*/
			if (!cur_if->pnan_info->security_required) {
				/*Parse recvd ndp resp*/
				nan_parse_ndp_resp(cur_if);

				/*Change state to CONNECTED*/
				change_ndp_state(cur_if, NDP_CONNECTED);

				/*Populate Data Confirmation fields to be sent
				 * in Event Buffer*/
				nan_send_data_confirm_event(cur_if, buffer,
							    size);

				/** Send NDPE event to app */
				INFO("NDPE data event#4");
				if ((cur_if->pnan_info->ndpe_attr_supported) &&
				    (cur_if->pnan_info->peer_avail_info_published
					     .ndpe_attr_supported))
					nan_send_ndpe_data_event(
						cur_if, ipv6_tlv,
						tport_sub_attr,
						tprotocol_sub_attr);
				/*Send the bitmap to firmware*/
				nancmd_set_final_bitmap(cur_if, NAN_NDL);

				/* notify peers of the updated committed
				 * schedule */
				nan_send_bcast_schedule_update(cur_if);

				/*Open nan interface*/
				nan_iface_open();

				/*Clear peers published entries*/
				nan_clear_peer_avail_published_entries(cur_if);
			}
		}
	} else {
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_ACCEPTED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_ACCEPTED)) {
			/*Parse recvd ndp resp*/
			nan_parse_ndp_resp(cur_if);

			/*Change state to CONNECTED*/
			change_ndp_state(cur_if, NDP_CONNECTED);

			/*Populate Data Confirmation fields to be sent in Event
			 * Buffer*/
			nan_send_data_confirm_event(cur_if, buffer, size);

			/** Send NDPE event to app */
			INFO("NDPE data event#5");
			if ((cur_if->pnan_info->ndpe_attr_supported) &&
			    (cur_if->pnan_info->peer_avail_info_published
				     .ndpe_attr_supported))
				nan_send_ndpe_data_event(cur_if, ipv6_tlv,
							 tport_sub_attr,
							 tprotocol_sub_attr);
			/*Send the bitmap to firmware*/
			nancmd_set_final_bitmap(cur_if, NAN_NDL);

			/* notify peers of the updated committed schedule */
			nan_send_bcast_schedule_update(cur_if);

			/*Open nan interface*/
			nan_iface_open();

			/*Clear peers published entries*/
			nan_clear_peer_avail_published_entries(cur_if);
		}
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_ACCEPTED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_CONTINUED)) {
			/*This is a case of counter proposal*/

			/*Parse recvd ndp resp*/
			nan_parse_ndp_resp(cur_if);

			/*Generate and send NDP confirm*/
			ret = nan_send_ndp_confirm(cur_if, ndl_attr, ndp_attr,
						   NDP_NDL_STATUS_ACCEPTED,
						   NDP_NDL_STATUS_ACCEPTED);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send NDP response");
				return ret;
			}

			/*Change state to CONNECTED*/
			change_ndp_state(cur_if, NDP_CONNECTED);

			/*Populate Data Confirmation fields to be sent in Event
			 * Buffer*/
			nan_send_data_confirm_event(cur_if, buffer, size);

			/** Send NDPE event to app */
			INFO("NDPE data event#6");
			if ((cur_if->pnan_info->ndpe_attr_supported) &&
			    (cur_if->pnan_info->peer_avail_info_published
				     .ndpe_attr_supported))
				nan_send_ndpe_data_event(cur_if, ipv6_tlv,
							 tport_sub_attr,
							 tprotocol_sub_attr);
			/*Send the bitmap to firmware*/
			nancmd_set_final_bitmap(cur_if, NAN_NDL);

			/*Open nan interface*/
			nan_iface_open();

			/*Clear peers published entries*/
			nan_clear_peer_avail_published_entries(cur_if);
		}
		if ((ndp_attr->type_and_status.status ==
		     NDP_NDL_STATUS_REJECTED) &&
		    (ndl_attr->type_and_status.status ==
		     NDP_NDL_STATUS_REJECTED)) {
			if ((cur_if->pnan_info->ndpe_attr_supported) &&
			    (cur_if->pnan_info->peer_avail_info_published
				     .ndpe_attr_supported) &&
			    (cur_if->pnan_info->ndpe_not_present)) {
				INFO("Sending the Confirm:Rejected, NDPE validation failed for NDP response");
				ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
				/*Generate and send NDP confirm*/
				ret = nan_send_ndp_confirm(
					cur_if, ndl_attr, ndp_attr,
					NDP_NDL_STATUS_REJECTED,
					NDP_NDL_STATUS_REJECTED);
				if (ret != NAN_ERR_SUCCESS) {
					ERR("Failed to send NDP response");
					return ret;
				}
			}
			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);
			/* for the special case of negative test case */
			cur_if->pnan_info->self_avail_info.conditional_valid =
				0;
			cur_if->pnan_info->self_avail_info.entry_conditional[0]
				.time_bitmap[0] = NAN_DEFAULT_BITMAP;
			cur_if->pnan_info->self_avail_info.entry_conditional[0]
				.combined_time_bitmap = NAN_DEFAULT_BITMAP;
			cur_if->pnan_info->self_avail_info.conditional_valid |=
				ENTRY0_VALID;
			cur_if->pnan_info->self_avail_info.entry_conditional[0]
				.op_class = DEFAULT_2G_OP_CLASS;
			cur_if->pnan_info->self_avail_info.entry_conditional[0]
				.channels[0] = DEFAULT_2G_OP_CHAN;
			cur_if->pnan_info->self_avail_info.seq_id++;
		}
	}
	FREE(cur_if->pnan_info->rx_ndp_resp);
	return NAN_ERR_SUCCESS;
}

#if 0
u32 nan_parse_time_bitmap(time_bitmap_control *ctrl, u8 bitmap_len, u8 *bitmap)
{
    u32 time_bitmap = 0, byte, bit, temp_word = 0;
    u8 time_byte, bit_pos = 0, dur_factor;

    if (((bitmap_len * 8) * (16 * (1 << ctrl->bit_duration))) > 512)
    {
        ERR("NAN2 : Time bitmap length cannot be more than 4!");
        bitmap_len = 512/((16 * (1 <<ctrl->bit_duration)) * 8); /*Bitmap length in octets*/
        ERR("NAN2 : Time bitmap length set to %d for bit duration %d",bitmap_len,(16 * (1<<ctrl->bit_duration)));
    }
    if (ctrl->start_offset > 24)
    {
        ERR("NAN2 : Time bitmap offset should not be more than 24!");
        return 0;
    }

    bit_pos = ctrl->start_offset;
    dur_factor = ctrl->bit_duration;

    for (byte = 0; byte < bitmap_len; byte++)
    {
        time_byte = bitmap[byte];

        for (bit = 0; bit < 8; bit++)
        {
            if (time_byte & (1<<bit))
            {
                temp_word = (1 << (1 << dur_factor)) - 1;
                time_bitmap |= (temp_word << bit_pos);
                temp_word = 0;
            }
            bit_pos += (1 << dur_factor);
        }
    }

    if (ctrl->bit_period == 1)
    {
        temp_word = time_bitmap & 0xff;
        time_bitmap = temp_word;
        time_bitmap |= temp_word << 8;
        time_bitmap |= temp_word << 16;
        time_bitmap |= temp_word << 24;
    }
    if (ctrl->bit_period == 2)
    {
        temp_word = time_bitmap & 0xffff;
        time_bitmap = temp_word;
        time_bitmap |= temp_word << 16;
    }

    return time_bitmap;
}
#endif

int nan_parse_generic_time_bitmap(time_bitmap_control *ctrl, u8 bitmap_len,
				  u8 *bitmap,
				  u32 time_bitmap[MAX_SCHEDULE_ENTRIES])
{
	u32 byte, bit, temp_word = 0, temp_word1 = 0, bit1 = 0;
	u8 time_byte, bit_pos = 0, dur_factor, bitmap_count = 0, spill_over = 0;

	bit_pos = ctrl->start_offset;
	dur_factor = ctrl->bit_duration;

	for (byte = 0; byte < bitmap_len; byte++) {
		time_byte = bitmap[byte];

		/*
			if( (byte != 0) && (((byte * 8 * dur_factor) % 512) ==
		   0) )
			{
			    bitmap_count++;
			    bit_pos = 0;
			}
		*/

		temp_word = 0;
		spill_over = 0;
		for (bit = 0; bit < 8; bit++) {
			if (bit_pos != DEFAULT_BITMAP_LEN) {
				if (time_byte & (1 << bit)) {
					temp_word =
						(1 << (1 << dur_factor)) - 1;
					time_bitmap[bitmap_count] |=
						(temp_word << bit_pos);
					temp_word = 0;
					spill_over = 1;
				} else
					spill_over = 0;

				bit_pos += (1 << dur_factor);
			}

			if (bit_pos >= DEFAULT_BITMAP_LEN) { // set parameters
							     // for the next
							     // bitmap
				bit_pos -= DEFAULT_BITMAP_LEN;
				bitmap_count++;

				// set the spill-over bits if any, from the last
				// bitmap
				if ((bit_pos != 0) && (spill_over == 1)) {
					temp_word1 = 0;
					for (bit1 = 0; bit1 < bit_pos; bit1++) {
						temp_word1 |= (1 << bit1);
					}
					time_bitmap[bitmap_count] |= temp_word1;
				}
			}
		}
	}

	if (ctrl->bit_period == 1) {
		temp_word = time_bitmap[0] & 0xff;
		time_bitmap[0] = temp_word;
		time_bitmap[0] |= temp_word << 8;
		time_bitmap[0] |= temp_word << 16;
		time_bitmap[0] |= temp_word << 24;
	}
	if (ctrl->bit_period == 2) {
		temp_word = time_bitmap[0] & 0xffff;
		time_bitmap[0] = temp_word;
		time_bitmap[0] |= temp_word << 16;
	}

	if (time_bitmap[bitmap_count] != 0)
		return (bitmap_count + 1);
	else
		return bitmap_count;
}

enum nan_error nan_parse_avail_entries(nan_availability_list *avail,
				       avail_entry_t *avail_entry)
{
	nan_channel_entry_list *chan_list;
	u32 num_entries, chan_entry_type;
	u8 *band_id;
	chan_entry_list_t *chan_entry;
	u8 i = 0, j = 0, ind = 0;
	u8 szof_time_bitmap_ctrl = 0;
	u8 szof_time_bitmap_len = 0;
	u8 szof_time_bitmap = 0;

	if (avail->entry_ctrl.usage_preference < avail_entry->usage_pref) {
		INFO("NAN2 : Ignoring the lower usage preference entry");
		//    return NAN_ERR_SUCCESS;
	}
	avail_entry->usage_pref = avail->entry_ctrl.usage_preference;

	if (avail->entry_ctrl.time_bitmap_present) {
		time_bitmap_control *time_bitmap_ctrl =
			(time_bitmap_control *)&avail->optional[0];
		u8 time_bitmap_len = avail->optional[2];
		u8 *time_bitmap = &avail->optional[3];
		avail_entry->time_bitmap_count = nan_parse_generic_time_bitmap(
			time_bitmap_ctrl, time_bitmap_len, time_bitmap,
			avail_entry->time_bitmap);
		avail_entry->combined_time_bitmap = 0;
		for (i = 0; i < avail_entry->time_bitmap_count; i++) {
			avail_entry->start_offset[i] = DEFAULT_WINDOW_SIZE * i;
			avail_entry->combined_time_bitmap |=
				avail_entry->time_bitmap[i];
		}

		/*
			if (time_bitmap_ctrl->bit_period == 0)
			    avail_entry->period  = 0;

			if (time_bitmap_ctrl->bit_period > 2)
			    avail_entry->period  = time_bitmap_ctrl->bit_period
		   - 2;
		*/

		ind = time_bitmap_ctrl->bit_period;
		avail_entry->period = ind;
		ERR("NAN2 : repeat_interval assigned %d", repeat_interval[ind]);

		if (!avail_entry->time_bitmap) {
			ERR("NAN2 : Parsed time bitmap is 0!");
		}
		szof_time_bitmap_ctrl = 2;
		szof_time_bitmap_len = 1;
		szof_time_bitmap = avail->optional[2]; /*time_bitmap_len*/
		// ERR("NAN2 : ##DBG Set bitmap 0x%x",
		// avail_entry->time_bitmap);
	} else {
		if (avail->entry_ctrl.avail_type != NDP_AVAIL_TYPE_POTENTIAL) {
			ERR("NAN2 : Time bitmap must be present for non-potential entries!");
			return NAN_ERR_INVAL;
		}

		avail_entry->time_bitmap[0] = NAN_POTENTIAL_BITMAP;
		avail_entry->start_offset[0] = 0;
		avail_entry->combined_time_bitmap = NAN_POTENTIAL_BITMAP;
		avail_entry->period = 1;
		avail_entry->time_bitmap_count = 1;
		ERR("NAN2 : ##DBG Set bitmap 0x%x",
		    avail_entry->combined_time_bitmap);
	}

	/*Get the channel list by hopping over (time_bitmap_ctrl +
	 * time_bitmap_len + time_bitmap) bytes*/
	chan_list = (nan_channel_entry_list *)&(
		avail->optional[szof_time_bitmap_ctrl + szof_time_bitmap_len +
				szof_time_bitmap]);
	num_entries = chan_list->entry_ctrl.num_entries;
	chan_entry_type = chan_list->entry_ctrl.entry_type;

	avail_entry->channels[0] = DEFAULT_2G_OP_CHAN;
	avail_entry->op_class = DEFAULT_2G_OP_CLASS;

	if (chan_entry_type == 0) // band entry
	{
		band_id = (u8 *)(&chan_list->chan_band_entry.band_id);
		for (i = 0; i < num_entries; i++) {
			if (*band_id == 2 && a_band_flag == FALSE) // 2.4 GHz
			{
				avail_entry->channels[0] = DEFAULT_2G_OP_CHAN;
				avail_entry->op_class = DEFAULT_2G_OP_CLASS;
				break;
			}
			if (*band_id == 4 && a_band_flag == TRUE) // 5 GHz
			{
				avail_entry->channels[0] = DEFAULT_5G_OP_CHAN;
				avail_entry->op_class = DEFAULT_5G_OP_CLASS;
				break;
			}
			band_id++;
		}

	} else // op class, chan map entry
	{
		chan_entry = &chan_list->chan_band_entry.chan_entry;

		/*if (avail_entry->op_class > 86 && a_band_flag == FALSE)
			    return NAN_ERR_UNSUPPORTED; */

		/*From multiple channel entries pickout the suitable one
		 * considering a_band flag*/
		for (i = 0; i < num_entries; i++) {
			if ((a_band_flag == FALSE &&
			     chan_entry->op_class < 86) ||
			    (a_band_flag == TRUE &&
			     chan_entry->op_class > 86)) {
				j += nan_get_channels_from_bitmap(
					chan_entry->op_class,
					chan_entry->chan_bitmap,
					chan_entry->primary_chan_bitmap,
					&avail_entry->channels[j]);
				avail_entry->op_class = chan_entry->op_class;
				if (chan_entry->op_class >= 128)
					avail_entry->op_class =
						DEFAULT_5G_OP_CLASS;
				break;
			}
			chan_entry++;
		}

		/*If there is no channel entry in 5ghz or a_band take the 1st
		 * available channel entry*/
		if (j == 0 && a_band_flag == TRUE) {
			chan_entry = &chan_list->chan_band_entry.chan_entry;
			j += nan_get_channels_from_bitmap(
				chan_entry->op_class, chan_entry->chan_bitmap,
				chan_entry->primary_chan_bitmap,
				&avail_entry->channels[j]);
			avail_entry->op_class = chan_entry->op_class;
			if (chan_entry->op_class >= 128)
				avail_entry->op_class = DEFAULT_5G_OP_CLASS;
		}
		/*If still no channel found return error*/
		if (j == 0)
			return NAN_ERR_UNSUPPORTED;

		for (i = 0; i < j; i++)
			ERR("NAN2 : ##DBG Found channels : %d",
			    avail_entry->channels[i]);
	}

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_parse_availability(nan_availability_attr *attr,
				      peer_availability_info *peer_info)
{
	nan_availability_list *entry;
	int entries_len, i;
	enum nan_error ret = NAN_ERR_SUCCESS;
	avail_entry_t peer_entry;

	peer_info->seq_id = attr->seq_id;
	peer_info->map_id = attr->attr_ctrl.map_id;
	entry = attr->entry;
	entries_len =
		attr->len - sizeof(nan_availability_attr) + NAN_ATTR_HDR_LEN;
	ERR("NAN2 : total entries %u", entries_len);
	while (entries_len > 0) {
		memset(&peer_entry, 0, sizeof(avail_entry_t));
		ret = nan_parse_avail_entries(entry, &peer_entry);
		if ((peer_entry.op_class == DEFAULT_5G_OP_CLASS) &&
		    (peer_info->single_band == TRUE)) {
			ERR("NAN2 : ##Peer is signle band, ignoring the 5G entry ");
			ret = NAN_ERR_INVAL;
		}
		peer_entry.map_id = attr->attr_ctrl.map_id;
		if (ret == NAN_ERR_SUCCESS) {
			if (entry->entry_ctrl.avail_type ==
			    NDP_AVAIL_TYPE_POTENTIAL) {
				if (!(entry->entry_ctrl.time_bitmap_present)) {
					peer_info->band_entry_potential = TRUE;
				}
				for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
					if (!(peer_info->potential_valid &
					      (1 << i))) {
						memcpy(&peer_info->entry_potential
								[i],
						       &peer_entry,
						       sizeof(avail_entry_t));
						peer_info->potential_valid |=
							(1 << i);
						ERR("NAN2 : parsed potential entry#%u 0x%x",
						    i,
						    peer_info
							    ->entry_potential[i]
							    .combined_time_bitmap);
						break;
					}
				}
			} else if (entry->entry_ctrl.avail_type ==
				   NDP_AVAIL_TYPE_COMMITTED) {
				for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
					if (!(peer_info->committed_valid &
					      (1 << i))) {
						memcpy(&peer_info->entry_committed
								[i],
						       &peer_entry,
						       sizeof(avail_entry_t));
						peer_info->committed_valid |=
							(1 << i);
						ERR("NAN2 : parsed committed entry#%u 0x%x",
						    i,
						    peer_info
							    ->entry_committed[i]
							    .combined_time_bitmap);
						break;
					}
				}
			} else if (entry->entry_ctrl.avail_type ==
					   NDP_AVAIL_TYPE_CONDITIONAL ||
				   entry->entry_ctrl.avail_type ==
					   NDP_AVAIL_TYPE_POTENTIAL_CONDITIONAL) {
				for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
					if (!(peer_info->conditional_valid &
					      (1 << i))) {
						memcpy(&peer_info->entry_conditional
								[i],
						       &peer_entry,
						       sizeof(avail_entry_t));
						peer_info->conditional_valid |=
							(1 << i);
						ERR("NAN2 : parsed conditional entry#%u 0x%x",
						    i,
						    peer_info
							    ->entry_conditional[i]
							    .combined_time_bitmap);
						break;
					}
				}
			}
		} else {
			ERR("NAN2 : Ignoring entry type %d due to non zero return",
			    entry->entry_ctrl.avail_type);
			memset(&peer_entry, 0, sizeof(avail_entry_t));
		}
		entries_len -= (entry->len + 2);
		entry = (nan_availability_list *)((u8 *)entry + entry->len + 2);
	}
	return ret;
}

enum nan_error nan_handle_ndp_confirm(struct mwu_iface_info *cur_if,
				      unsigned char *buffer, int size)
{
	nan_ndp_attr *ndp_attr;
	nan_ndl_attr *ndl_attr;
	u8 *publish_id = NULL, *responder_ndi = NULL, *service_info = NULL;
	int service_len = 0;

	if (NAN_ERR_SUCCESS != nan_validate_ndp_confirm(cur_if, buffer, size)) {
		ERR("NDP frame validation failed");
		change_ndp_state(cur_if, NDP_IDLE);
		return NAN_ERR_INVAL;
	}
	ERR("NDP confirm validation successful");

	/* Maintain a copy of ndp response buffer for later use.
	   Free this buffer after transmitting ndp response */
	cur_if->pnan_info->rx_ndp_conf =
		(struct nan_generic_buf *)malloc(size + NAN_BUF_HDR_SIZE);
	memcpy(cur_if->pnan_info->rx_ndp_conf->buf, buffer, size);
	cur_if->pnan_info->rx_ndp_conf->size = size + NAN_BUF_HDR_SIZE;

	if (cur_if->pnan_info->security_required) {
		cur_if->pnan_info->nan_security.m3 =
			(struct nan_sec_buf *)malloc(size + NAN_BUF_HDR_SIZE);
		memcpy(cur_if->pnan_info->nan_security.m3->buf, buffer, size);
		cur_if->pnan_info->nan_security.m3->size =
			nan_calculate_ndp_frame_size(buffer, size) +
			7 /*Category ,Action, OUI, subtype, NAF type*/
			+ NAN_BUF_HDR_SIZE;
		ERR("DBG RCVD M3 SIZE %lu",
		    cur_if->pnan_info->nan_security.m3->size -
			    NAN_BUF_HDR_SIZE);

		nan_shared_key_desc_attr *temp_shared_key_desc;
		temp_shared_key_desc =
			(nan_shared_key_desc_attr *)nan_get_nan_attr(
				NAN_SHARED_KEY_DESC_ATTR,
				cur_if->pnan_info->nan_security.m3->buf,
				cur_if->pnan_info->nan_security.m3->size);

		/* If M3 does not contain valid NAN_SHARED_KEY_DESC_ATTR, do not
		 * check MIC validation*/
		if (temp_shared_key_desc) {
			eapol_key_frame *rcvd_m3_eapol;

			rcvd_m3_eapol =
				(eapol_key_frame *)
					temp_shared_key_desc->rsna_key_desc;

			u8 temp_mic[MIC_LEN] = {0};
			u8 rcvd_mic[MIC_LEN] = {0};

			memcpy(rcvd_mic, rcvd_m3_eapol->key_mic, MIC_LEN);
			memset(rcvd_m3_eapol->key_mic, 0x0, MIC_LEN);

			nan_validate_m3_mic(
				temp_mic,
				&cur_if->pnan_info->nan_security.m1->buf[10],
				cur_if->pnan_info->nan_security.m1->size -
					NAN_BUF_HDR_SIZE,
				&cur_if->pnan_info->nan_security.m3->buf[10],
				cur_if->pnan_info->nan_security.m3->size -
					NAN_BUF_HDR_SIZE,
				cur_if->pnan_info->nan_security.ptk_buf.kck);

			INFO("rcvd M3 body length : %lu \n",
			     cur_if->pnan_info->nan_security.m3->size -
				     NAN_BUF_HDR_SIZE);
			if (memcmp(rcvd_mic, temp_mic, MIC_LEN)) {
				ERR("Invalid EAPOL-Key MIC");
				INFO("MIC validation has failed for rcvd M3\n");

				free(cur_if->pnan_info->nan_security.m1);
				free(cur_if->pnan_info->nan_security.m2);

				/*Change state to IDLE*/
				change_ndp_state(cur_if, NDP_IDLE);
				ERR("Failed to send NDP install\n");
				return NAN_ERR_INVAL;
			}
		}
		free(cur_if->pnan_info->nan_security.m1);
		free(cur_if->pnan_info->nan_security.m2);
	}

	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(NAN_NDL_ATTR, buffer, size);
	if (cur_if->pnan_info->confirm_required ||
	    cur_if->pnan_info->security_required) {
		if ((!cur_if->pnan_info->ndpe_not_present) &&
		    (cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported))
			ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(
				NAN_NDP_EXT_ATTR, buffer, size);
		else
			ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(
				NAN_NDP_ATTR, buffer, size);
		nan_get_optional_ndp_attr(ndp_attr, publish_id, &responder_ndi,
					  &service_info, &service_len);

		if (ndp_attr->type_and_status.status ==
			    NDP_NDL_STATUS_ACCEPTED ||
		    ndp_attr->type_and_status.status ==
			    NDP_NDL_STATUS_CONTINUED) /*In case of NAN
							 security*/
		{
			/*Parse recvd ndp confirm*/
			nan_parse_ndp_confirm(cur_if);

			/*Send Key install frame and install keys to FW*/
			if (cur_if->pnan_info->security_required) {
				if (NAN_ERR_SUCCESS !=
				    nan_send_security_install(cur_if)) {
					/*Change state to IDLE*/
					change_ndp_state(cur_if, NDP_IDLE);
					FREE(cur_if->pnan_info->rx_ndp_conf);
					return NAN_ERR_SUCCESS;
				}
			}

			/*Change state to CONNECTED*/
			change_ndp_state(cur_if, NDP_CONNECTED);

			/*Populate Data Confirmation fields to be sent in Event
			 * Buffer*/
			nan_send_data_confirm_event(cur_if, buffer, size);

			/** Might not require to send NDPE event from here,
			 * since we already have the ipv6 identifier from the
			 * data indication event Also, till now, ndp_req does
			 * not contain service specific info (port + protocol)
			 * Details (ipv6 + port + protocol) not specified for
			 * the ndp_confirm frame as well (hence added the event
			 * here just for consistency)
			 */
			/** Send NDPE event to app */
			INFO("NDPE data event#7");
			if ((cur_if->pnan_info->ndpe_attr_supported) &&
			    (cur_if->pnan_info->peer_avail_info_published
				     .ndpe_attr_supported))
				nan_send_ndpe_data_event(cur_if, NULL, NULL,
							 NULL);

			/*Set the schedule bitmap*/
			nancmd_set_final_bitmap(cur_if, NAN_NDL);

			/*Open nan interface*/
			nan_iface_open();

			/*cur_if->pnan_info->self_avail_info.seq_id++;
			 cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap
			     &=
			 ~(cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap);*/

			/*Clear peers published entries*/
			nan_clear_peer_avail_published_entries(cur_if);
		} else {
			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);
		}
	} else {
		if (ndl_attr->type_and_status.status ==
		    NDP_NDL_STATUS_ACCEPTED) {
			/*Parse recvd ndp conf*/
			nan_parse_ndp_confirm(cur_if);

			/*Change state to CONNECTED*/
			change_ndp_state(cur_if, NDP_CONNECTED);

			/*Populate Data Confirmation fields to be sent in Event
			 * Buffer*/
			nan_send_data_confirm_event(cur_if, buffer, size);

			/*Set the schedule bitmap*/
			nancmd_set_final_bitmap(cur_if, NAN_NDL);

			/*Open nan interface*/
			nan_iface_open();

			/*cur_if->pnan_info->self_avail_info.seq_id++;
			cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap
			    &=
			~(cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap);*/

			/*Clear peers published entries*/
			nan_clear_peer_avail_published_entries(cur_if);
		} else {
			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);
		}
	}
	FREE(cur_if->pnan_info->rx_ndp_conf);
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_handle_schedule_req(struct mwu_iface_info *cur_if,
				       unsigned char *buffer, int size)
{
	/*Extract Peer MAC and maintain a copy of schedule request buffer for
	  later use. Free this buffer after transmitting schedule response*/
	cur_if->pnan_info->self_avail_info.committed_valid = 0;
	cur_if->pnan_info->peer_avail_info.committed_valid = 0;
	cur_if->pnan_info->self_avail_info.conditional_valid = 0;
	cur_if->pnan_info->peer_avail_info.conditional_valid = 0;
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] =
		NAN_POTENTIAL_BITMAP;
	cur_if->pnan_info->self_avail_info.entry_potential[0]
		.combined_time_bitmap = NAN_POTENTIAL_BITMAP;

	memcpy(cur_if->pnan_info->peer_mac, buffer, ETH_ALEN);
	cur_if->pnan_info->rx_ndp_req =
		(struct nan_generic_buf *)malloc(size + NAN_BUF_HDR_SIZE);
	memcpy(cur_if->pnan_info->rx_ndp_req->buf, buffer, size);
	cur_if->pnan_info->rx_ndp_req->size = size + NAN_BUF_HDR_SIZE;

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_send_schedule_req(struct mwu_iface_info *cur_if,
				     char responder_mac[ETH_ALEN])
{
	int ret;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;
	u32 sched_req_bitmap = 0, peer_bitmap = 0;
	u8 sched_op_class = 0, sched_op_chan = 0;

	NDC_INFO *ndc;
	NDL_INFO *ndl;

	ndc = &cur_if->pnan_info->ndc_info[0]; // current NDC_INFO
	ndl = &ndc->ndl_info[0]; // current NDL_INFO

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr, responder_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;
	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = SCHEDULE_REQUEST;

	{ // Availability attr
		nan_availability_attr *availability_attr;
		nan_availability_list *entry;
		nan_channel_entry_list *chan_list;
		u8 opt_fields_len = 7, i = 0;
		u8 sz_of_chan_entry_list = sizeof(chan_entry_list_t);
		u8 sz_of_band_id = sizeof(u8);
		u8 sz_of_entry_len = sizeof(u16);

		availability_attr = (nan_availability_attr *)ndp_var_attr_ptr;
		availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
		availability_attr->attr_ctrl.map_id =
			cur_if->pnan_info->self_avail_info.map_id;
		availability_attr->attr_ctrl.committed_changed = 1;
		availability_attr->seq_id =
			++cur_if->pnan_info->self_avail_info.seq_id;

		availability_attr->len =
			sizeof(nan_availability_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_availability_attr);
		var_sd_attr_len += sizeof(nan_availability_attr);

		if (ndc->ndc_setup && ndl->ndl_setup) { // schedule negotiation
							// case

			if (cur_if->pnan_info->schedule_update_needed) {
				sched_op_class =
					cur_if->pnan_info->ndl_sched.op_class;
				sched_op_chan =
					cur_if->pnan_info->ndl_sched.op_chan;
				sched_req_bitmap = cur_if->pnan_info->ndl_sched
							   .availability_map[0];
			} else {
				/* Compute the bitmap and specify default
				 * channel parameters*/
				peer_availability_info *peer_info = NULL,
						       *self_info = NULL;
				peer_info = &cur_if->pnan_info->peer_avail_info;
				self_info = &cur_if->pnan_info->self_avail_info;

				sched_op_class = DEFAULT_2G_OP_CLASS;
				sched_op_chan = DEFAULT_2G_OP_CHAN;

				// include the existing NDC schedule
				sched_req_bitmap = ndc->slot;

				// include the existing immutable schedule if
				// any
				if (ndl->immutable_sched_present) {
					sched_req_bitmap |=
						ndl->immutable_sched_bitmap;
				}

				// select random slots from peer's committed
				// availability
				peer_bitmap = peer_info->entry_committed[0]
						      .combined_time_bitmap;
				if ((peer_bitmap & PREFERRED_BITMAP1) == 0)
					peer_bitmap &= PREFERRED_BITMAP2;
				else
					peer_bitmap &= PREFERRED_BITMAP1;

				sched_req_bitmap |= peer_bitmap;

				// Add more random slots from the ones not
				// covered by above cases
				/*
				if((sched_req_bitmap | RANDOM_BITMAP2) ==
				sched_req_bitmap) sched_req_bitmap |=
				RANDOM_BITMAP1; else sched_req_bitmap |=
				RANDOM_BITMAP2;
				*/
				sched_req_bitmap |= NAN_DEFAULT_BITMAP;

				// consider peer's potential bitmap sent
				// previously
				peer_bitmap = peer_info->entry_potential[0]
						      .combined_time_bitmap;

				if (peer_bitmap) {
					sched_req_bitmap &= peer_bitmap;
				}

				/*Check if bitmap is qos compliant; if not use
				 * default map*/
				if ((cur_if->pnan_info->qos_enabled) &&
				    (NAN_ERR_SUCCESS !=
				     nan_validate_bitmap_for_qos(
					     sched_req_bitmap,
					     &self_info->qos))) {
					sched_req_bitmap = DEFAULT_BITMAP1;
				}

				sched_req_bitmap &= NAN_POTENTIAL_BITMAP;

				// Just in case ndc and immutable go outside
				sched_req_bitmap |= ndl->immutable_sched_bitmap;
				sched_req_bitmap = ndc->slot;
			}

			INFO("Adding the schedule-update committed entry");
			entry = (nan_availability_list *)ndp_var_attr_ptr;
			entry->entry_ctrl.avail_type = NDP_AVAIL_TYPE_COMMITTED;
			entry->entry_ctrl.usage_preference = 1;
			entry->entry_ctrl.utilization = 0;
			entry->entry_ctrl.rx_nss = 1;
			entry->entry_ctrl.time_bitmap_present = 1;
			/*Populate optional fields*/
			{
				time_bitmap_control *time_bitmap_ctrl =
					(time_bitmap_control *)&entry
						->optional[0];
				u8 *time_bitmap_len = &entry->optional[2];
				u8 *time_bitmap = &entry->optional[3];

				time_bitmap_ctrl->bit_duration =
					NDP_TIME_BM_DUR_16;
				time_bitmap_ctrl->bit_period =
					NDP_TIME_BM_PERIOD_512;
				time_bitmap_ctrl->start_offset = 0;
				*time_bitmap_len = 4; /*time_bitmap_len*/

				memcpy(time_bitmap, &sched_req_bitmap,
				       4); /*time bitmap computed above*/
			}
			/*Hop over by (avail list + optional)bytes to get chan
			 * list */
			chan_list = (nan_channel_entry_list
					     *)(ndp_var_attr_ptr +
						sizeof(nan_availability_list) +
						opt_fields_len);
			chan_list->entry_ctrl.entry_type = 1;
			chan_list->entry_ctrl.band_type = 0;
			chan_list->entry_ctrl.num_entries = 1;
			chan_list->chan_band_entry.chan_entry.op_class =
				sched_op_class;
			chan_list->chan_band_entry.chan_entry.chan_bitmap =
				ndp_get_chan_bitmap(sched_op_class,
						    sched_op_chan);
			chan_list->chan_band_entry.chan_entry
				.primary_chan_bitmap = 0x0;
			// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
			// = 0x0;

			entry->len = sizeof(nan_channel_entry_list) +
				     sizeof(nan_availability_list) +
				     opt_fields_len - sz_of_entry_len;
			availability_attr->len += entry->len + sz_of_entry_len;
			ndp_var_attr_ptr += entry->len + sz_of_entry_len;
			var_sd_attr_len += entry->len + sz_of_entry_len;

		} else { // Fresh schedule request

			for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
				/*Send all the valid conditional entries in
				 * proposal*/
				if ((cur_if->pnan_info->self_avail_info
					     .conditional_valid &
				     (1 << i))) {
					INFO("Adding conditional entry#%u", i);
					entry = (nan_availability_list *)
						ndp_var_attr_ptr;
					entry->entry_ctrl.avail_type =
						NDP_AVAIL_TYPE_CONDITIONAL;
					entry->entry_ctrl.usage_preference = 1;
					entry->entry_ctrl.utilization = 0;
					entry->entry_ctrl.rx_nss = 1;
					entry->entry_ctrl.time_bitmap_present =
						1;
					/*Populate optional fields*/
					{
						time_bitmap_control *time_bitmap_ctrl =
							(time_bitmap_control
								 *)&entry
								->optional[0];
						u8 *time_bitmap_len =
							&entry->optional[2];
						u8 *time_bitmap =
							&entry->optional[3];
						u32 ndp_req_bitmap,
							peer_bitmap =
								0xFFFFFFFF;
						peer_availability_info
							*peer_info;

						peer_info =
							&cur_if->pnan_info
								 ->peer_avail_info;

						if (peer_info->potential_valid &
						    ENTRY0_VALID)
							peer_bitmap =
								peer_info
									->entry_potential
										[0]
									.combined_time_bitmap;
						else if (peer_info->committed_valid &
							 ENTRY0_VALID)
							peer_bitmap =
								peer_info
									->entry_committed
										[0]
									.combined_time_bitmap; // in case some device sends committed entry in the publish frame

						time_bitmap_ctrl->bit_duration =
							NDP_TIME_BM_DUR_16;
						time_bitmap_ctrl->bit_period =
							NDP_TIME_BM_PERIOD_512;
						time_bitmap_ctrl->start_offset =
							0;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						// Request the ndl schedule
						// taking into consideration the
						// peer's bitmap
						ndp_req_bitmap =
							(cur_if->pnan_info
								 ->self_avail_info
								 .entry_conditional
									 [i]
								 .time_bitmap[0]) &
							peer_bitmap;

						memcpy(time_bitmap,
						       &ndp_req_bitmap,
						       4); /*time bitmap*/
					}
					/*Hop over by (avail list +
					 * optional)bytes to get chan list */
					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 1;
					chan_list->entry_ctrl.band_type = 0;
					chan_list->entry_ctrl.num_entries = 1;
					chan_list->chan_band_entry.chan_entry
						.op_class =
						cur_if->pnan_info
							->self_avail_info
							.entry_conditional[i]
							.op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap = ndp_get_chan_bitmap(
						cur_if->pnan_info
							->self_avail_info
							.entry_conditional[i]
							.op_class,
						cur_if->pnan_info
							->self_avail_info
							.entry_conditional[i]
							.channels[0]);
					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len -
						sz_of_entry_len;
					availability_attr->len +=
						entry->len + sz_of_entry_len;
					ndp_var_attr_ptr +=
						entry->len + sz_of_entry_len;
					var_sd_attr_len +=
						entry->len + sz_of_entry_len;
				}
			}
		}

		// Add the potential entry
		{
			nan_availability_list *potential_entry;
			potential_entry =
				(nan_availability_list *)ndp_var_attr_ptr;

			potential_entry->entry_ctrl.avail_type =
				NDP_AVAIL_TYPE_POTENTIAL;
			potential_entry->entry_ctrl.usage_preference = 1;
			potential_entry->entry_ctrl.utilization = 0;
			potential_entry->entry_ctrl.rx_nss = 1;

			if (cur_if->pnan_info->self_avail_info
				    .band_entry_potential) {
				/*Device available on all slots on the given
				 * band*/
				potential_entry->entry_ctrl.time_bitmap_present =
					0;
				chan_list =
					(nan_channel_entry_list
						 *)(ndp_var_attr_ptr +
						    sizeof(nan_availability_list));
				chan_list->entry_ctrl.entry_type = 0x0;
				chan_list->entry_ctrl.band_type = 0x0;
				chan_list->entry_ctrl.num_entries = 0x1;
				if (cur_if->pnan_info->a_band)
					chan_list->chan_band_entry.band_id =
						0x04; /*5 GHz*/
				else
					chan_list->chan_band_entry.band_id =
						0x02; /*2.4 GHz*/

				potential_entry->len =
					sizeof(nan_channel_entry_list) +
					sizeof(nan_availability_list) -
					sz_of_entry_len -
					sz_of_chan_entry_list +
					sz_of_band_id; // set the potential
						       // entry's length
				// add the potetial entry's length in the main
				// avail_attr and other pointers
				availability_attr->len +=
					potential_entry->len + sz_of_entry_len;
				ndp_var_attr_ptr +=
					potential_entry->len + sz_of_entry_len;
				var_sd_attr_len +=
					potential_entry->len + sz_of_entry_len;

			} else { // non-band entry

				potential_entry->entry_ctrl.time_bitmap_present =
					1;
				/*Populate optional fields*/
				{
					time_bitmap_control *time_bitmap_ctrl =
						(time_bitmap_control
							 *)&potential_entry
							->optional[0];
					u8 *time_bitmap_len =
						&potential_entry->optional[2];
					u8 *time_bitmap =
						&potential_entry->optional[3];

					time_bitmap_ctrl->bit_duration =
						NDP_TIME_BM_DUR_16;
					time_bitmap_ctrl->bit_period =
						NDP_TIME_BM_PERIOD_512;
					time_bitmap_ctrl->start_offset = 0;
					*time_bitmap_len =
						4; /*time_bitmap_len*/

					memcpy(time_bitmap,
					       &cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.time_bitmap[0],
					       4); /*time bitmap*/
				}
				/*Hop over by 5 bytes(time_bitmap_len +
				 * time_bitmap) to get chan list */
				chan_list =
					(nan_channel_entry_list
						 *)(ndp_var_attr_ptr +
						    sizeof(nan_availability_list) +
						    opt_fields_len);
				chan_list->entry_ctrl.entry_type = 1;
				chan_list->entry_ctrl.band_type = 0;
				chan_list->entry_ctrl.num_entries = 1;

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

				// set the potential entry's length
				potential_entry->len =
					sizeof(nan_channel_entry_list) +
					sizeof(nan_availability_list) +
					opt_fields_len - sz_of_entry_len;
				// add the potetial entry's length in the main
				// avail_attr and other pointers
				availability_attr->len +=
					potential_entry->len + sz_of_entry_len;
				ndp_var_attr_ptr +=
					potential_entry->len + sz_of_entry_len;
				var_sd_attr_len +=
					potential_entry->len + sz_of_entry_len;
			}
		} // end of the block adding potential entry
	}

	if (ndc->ndc_setup && ndl->ndl_setup) { // case of schedule update
						// negotiation, NDC attr :
						// include the currentNDC
						// schedule in the schedule
						// request
		nan_ndc_attr *ndc_attr;
		ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		ndc_attr->attribute_id = NAN_NDC_ATTR;
		memcpy(ndc_attr->ndc_id, ndc->ndc_id, ETH_ALEN);
		ndc_attr->map_id =
			cur_if->pnan_info->self_avail_info.map_id; // TODO Put
								   // map id of
								   // Avail attr
								   // here
		ndc_attr->sched_ctrl.proposed_ndc = 0x1;
		ndc_attr->time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
		ndc_attr->time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
		ndc_attr->time_bitmap_ctrl.start_offset = 0;
		ndc_attr->time_bitmap_len = 4;
		memcpy(ndc_attr->bitmap, &ndc->slot, sizeof(u32));

		ndc_attr->len = sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
		var_sd_attr_len += sizeof(nan_ndc_attr) + 4;

	} else { // Fresh schedule request

		if (cur_if->pnan_info->ndc_attr.len > 0) {
			nan_ndc_attr *ndc_attr =
				(nan_ndc_attr *)ndp_var_attr_ptr;
			memcpy(ndc_attr, cur_if->pnan_info->ndc_attr.data,
			       cur_if->pnan_info->ndc_attr.len);
			ndp_var_attr_ptr += cur_if->pnan_info->ndc_attr.len;
			var_sd_attr_len += cur_if->pnan_info->ndc_attr.len;
		} else {
			nan_ndc_attr *ndc_attr;
			/*Setting prefered bitmap for 2.4 or 5 g bands*/
			u32 bitmap =
				cur_if->pnan_info->a_band ? 0x0200 : 0x0002;
			u8 wfa_ndc_id[6] = {0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00};
			ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
			ndc_attr->attribute_id = NAN_NDC_ATTR;
			memcpy(ndc_attr->ndc_id, wfa_ndc_id, ETH_ALEN);
			ndc_attr->map_id = cur_if->pnan_info->self_avail_info
						   .map_id; // TODO Put map id
							    // of Avail attr
							    // here
			ndc_attr->sched_ctrl.proposed_ndc = 0x1;
			ndc_attr->time_bitmap_ctrl.bit_duration =
				NDP_TIME_BM_DUR_16;
			ndc_attr->time_bitmap_ctrl.bit_period =
				NDP_TIME_BM_PERIOD_512;
			ndc_attr->time_bitmap_ctrl.start_offset = 0;
			ndc_attr->time_bitmap_len = 4;
			memcpy(ndc_attr->bitmap, &bitmap, sizeof(u32)); // Hardcoded
									// as
									// 2nd
									// slot,
									// just
									// after
									// DW
			/*Save NDC related info in NDC_INFO*/
			cur_if->pnan_info->ndc_info[0].slot = bitmap;
			memcpy(cur_if->pnan_info->ndc_info[0].ndc_id,
			       wfa_ndc_id, ETH_ALEN);

			ndc_attr->len =
				sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
			var_sd_attr_len += sizeof(nan_ndc_attr) + 4;
		}
	}

	if (ndc->ndc_setup && ndl->ndl_setup) { // case of schedule update
						// negotiation , NDL attr: add
						// the current immutable schdule
		nan_ndl_attr *ndl_attr;
		ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		ndl_attr->attribute_id = NAN_NDL_ATTR;
		u8 sz_of_immutable_sched = 0;
		ndl_attr->dialogue_token = ++cur_if->pnan_info->dialog_token;
		ndl_attr->type_and_status.type = NDP_NDL_TYPE_REQUEST;
		ndl_attr->type_and_status.status = NDP_NDL_STATUS_CONTINUED;
		ndl_attr->reson_code = NAN_REASON_CODE_DEFAULT; // gbhat@HC,
								// reserved
		ndl_attr->ndl_ctrl.ndl_peer_id_present = 0x0; // Only NDC
							      // attribute
							      // present bit set
							      // and peer id
							      // present bit set
		if (ndl->immutable_sched_present) {
			ndl_attr->ndl_ctrl.immutable_sched_present = 0x1;
		}
		ndl_attr->ndl_ctrl.ndc_attr_present = 0x1; // Only NDC attribute
							   // present bit set
							   // and peer id
							   // present bit set
		ndl_attr->ndl_ctrl.ndl_qos_attr_present = 0x1; // NDL QoS is
							       // present

		if (ndl->immutable_sched_present) {
			immutable_schedule *imm_sched =
				(immutable_schedule *)&ndl_attr->optional[0];
			// imm_sched->no_of_entries = 1;
			imm_sched->entry[0].map_id =
				cur_if->pnan_info->self_avail_info.map_id;
			imm_sched->entry[0].time_bitmap_ctrl.bit_duration =
				NDP_TIME_BM_DUR_16;
			imm_sched->entry[0].time_bitmap_ctrl.bit_period =
				NDP_TIME_BM_PERIOD_512;
			imm_sched->entry[0].time_bitmap_ctrl.start_offset = 0;
			imm_sched->entry[0].time_bitmap_len = 4;
			memcpy(&imm_sched->entry[0].bitmap,
			       &ndl->immutable_sched_bitmap, 4);
			sz_of_immutable_sched = sizeof(immutable_schedule) +
						sizeof(immutable_sched_entry) +
						4;
		}
		ndl_attr->len = sizeof(nan_ndl_attr) + sz_of_immutable_sched -
				NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr +=
			sizeof(nan_ndl_attr) + sz_of_immutable_sched;
		var_sd_attr_len += sizeof(nan_ndl_attr) + sz_of_immutable_sched;

	} else { // Fresh schedule request
		if (cur_if->pnan_info->ndl_attr.len > 0) {
			nan_ndl_attr *ndl_attr =
				(nan_ndl_attr *)ndp_var_attr_ptr;
			memcpy(ndl_attr, cur_if->pnan_info->ndl_attr.data,
			       cur_if->pnan_info->ndl_attr.len);
			ndp_var_attr_ptr += cur_if->pnan_info->ndl_attr.len;
			var_sd_attr_len += cur_if->pnan_info->ndl_attr.len;
		} else {
			nan_ndl_attr *ndl_attr;
			ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
			ndl_attr->attribute_id = NAN_NDL_ATTR;
			u8 sz_of_immutable_sched = 0;
			ndl_attr->dialogue_token =
				++cur_if->pnan_info->dialog_token;
			ndl_attr->type_and_status.type = NDP_NDL_TYPE_REQUEST;
			ndl_attr->type_and_status.status =
				NDP_NDL_STATUS_CONTINUED;
			ndl_attr->reson_code =
				NAN_REASON_CODE_DEFAULT; // gbhat@HC, reserved
			ndl_attr->ndl_ctrl.ndl_peer_id_present =
				0x0; // Only NDC attribute present bit set and
				     // peer id present bit set
			if (cur_if->pnan_info->immutable_sched_required)
				ndl_attr->ndl_ctrl.immutable_sched_present =
					0x1;
			else
				ndl_attr->ndl_ctrl.immutable_sched_present =
					0x0;
			ndl_attr->ndl_ctrl.ndc_attr_present =
				0x1; // Only NDC attribute present bit set and
				     // peer id present bit set
			ndl_attr->ndl_ctrl.ndl_qos_attr_present =
				0x1; // Only NDC attribute present bit set and
				     // peer id present bit set
			if (cur_if->pnan_info->immutable_sched_required) {
				immutable_schedule *imm_sched =
					(immutable_schedule *)&ndl_attr
						->optional[0];
				u32 bitmap = 0x02;
				// imm_sched->no_of_entries = 1;
				imm_sched->entry[0].map_id =
					cur_if->pnan_info->self_avail_info
						.map_id;
				imm_sched->entry[0]
					.time_bitmap_ctrl.bit_duration =
					NDP_TIME_BM_DUR_16;
				imm_sched->entry[0].time_bitmap_ctrl.bit_period =
					NDP_TIME_BM_PERIOD_512;
				imm_sched->entry[0]
					.time_bitmap_ctrl.start_offset = 0;
				imm_sched->entry[0].time_bitmap_len = 4;
				memcpy(&imm_sched->entry[0].bitmap, &bitmap, 4);
				sz_of_immutable_sched =
					sizeof(immutable_schedule) +
					sizeof(immutable_sched_entry) + 4;
			}

			ndl_attr->len = sizeof(nan_ndl_attr) +
					sz_of_immutable_sched -
					NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr +=
				sizeof(nan_ndl_attr) + sz_of_immutable_sched;
			var_sd_attr_len +=
				sizeof(nan_ndl_attr) + sz_of_immutable_sched;
		}
	}
	/*NDL QoS Attribute*/
	{
		nan_ndl_qos_attr *ndl_qos_attr =
			(nan_ndl_qos_attr *)ndp_var_attr_ptr;
		ndl_qos_attr->attribute_id = NAN_NDL_QOS_ATTR;
		ndl_qos_attr->min_slots =
			cur_if->pnan_info->self_avail_info.qos.min_slots;
		memcpy(ndl_qos_attr->max_latency,
		       &cur_if->pnan_info->self_avail_info.qos.max_latency,
		       sizeof(ndl_qos_attr->max_latency));
		ndl_qos_attr->len = sizeof(nan_ndl_qos_attr) - NAN_ATTR_HDR_LEN;

		ndp_var_attr_ptr += sizeof(nan_ndl_qos_attr);
		var_sd_attr_len += sizeof(nan_ndl_qos_attr);
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out Schedule request");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	{ // commit the schedule
		peer_availability_info *peer_info, *self_info;

		peer_info = &cur_if->pnan_info->peer_avail_info;
		self_info = &cur_if->pnan_info->self_avail_info;

		cur_if->pnan_info->self_avail_info.committed_valid = 1;
		cur_if->pnan_info->peer_avail_info.committed_valid = 1;
		cur_if->pnan_info->self_avail_info.conditional_valid = 0;
		cur_if->pnan_info->peer_avail_info.conditional_valid = 0;

		peer_info->entry_committed[0].time_bitmap[0] = sched_req_bitmap;
		peer_info->entry_committed[0].op_class = sched_op_class;
		peer_info->entry_committed[0].channels[0] = sched_op_chan;

		self_info->entry_committed[0].time_bitmap[0] = sched_req_bitmap;
		self_info->entry_committed[0].op_class = sched_op_class;
		self_info->entry_committed[0].channels[0] = sched_op_chan;

		nancmd_set_final_bitmap(cur_if, NAN_NDL);
	}

	if (ret != NAN_ERR_SUCCESS)
		ERR("nan_cmdbuf_send failed");

	return NAN_ERR_SUCCESS;
}

int in_prev_chans(int channels[5], int count, int cur_chan)
{
	int i;
	for (i = 0; i < count; i++) {
		if (cur_chan == channels[i])
			return 1;
	}
	return 0;
}

enum nan_error nan_handle_schedule_update(struct mwu_iface_info *cur_if,
					  unsigned char *buffer, int size)
{
	nan_unaligned_sched_attr *unaligned_sched = NULL;
	nan_ulw_param ulw = {0};
	u8 channels[16] = {0}, j = 0, i = 0, k = 0, count = 0;
	nan_availability_attr *avail_attr = NULL;
	peer_availability_info *peer_info = NULL, *self_info = NULL;
	int prev_chan_count = 0, prev_chans[5];
	unsigned char *temp_buffer = NULL;

	/*Unaligned schedule attribute*/
	unaligned_sched = (nan_unaligned_sched_attr *)nan_get_nan_attr(
		NAN_UNALIGNED_SCHEDULE_ATTR, buffer, size);
	if (unaligned_sched) {
		ulw.start_time = unaligned_sched->start_time;
		ulw.period = unaligned_sched->period;
		ERR("NAN2 : ULW period : %d", ulw.period);
		ulw.duration = unaligned_sched->duration;
		ulw.count_down = unaligned_sched->count_down;
		ERR("NAN2 : ULW attr_len : %d", unaligned_sched->len);

		if (unaligned_sched->len >
		    (sizeof(nan_unaligned_sched_attr) - NAN_ATTR_HDR_LEN)) {
			nan_ulw_control *ulw_control =
				(nan_ulw_control *)&unaligned_sched->optional[0];
			ulw.avail_type = ulw_control->channel_avail;
			if (ulw_control->type != 0) {
				chan_entry_list_t *chan_entry =
					(chan_entry_list_t *)&unaligned_sched
						->optional[1];
				j = nan_get_channels_from_bitmap(
					chan_entry->op_class,
					chan_entry->chan_bitmap,
					chan_entry->primary_chan_bitmap,
					&channels[0]);

				for (i = 0; i < j; i++) {
					ERR("NAN2 : ##DBG Found channels : %d",
					    channels[i]);
				}
				ulw.channel = channels[0];
			} else {
				ERR("NAN2: Support for band entry in ULW not present");
			}
		}
		nancmd_set_unaligned_sched(cur_if, &ulw);
	}

	/*Extract and parse the first availability attribute sent by the peer*/
	temp_buffer = (unsigned char *)nan_get_nan_attr(NAN_AVAILABILITY_ATTR,
							buffer, size);
	avail_attr = (nan_availability_attr *)temp_buffer;
	peer_info = &cur_if->pnan_info->peer_avail_info;
	self_info = &cur_if->pnan_info->self_avail_info;

	ERR("NAN2 : parsed entries - committed %d, conditional %d, potential %d",
	    cur_if->pnan_info->peer_avail_info.committed_valid,
	    cur_if->pnan_info->peer_avail_info.conditional_valid,
	    cur_if->pnan_info->peer_avail_info.potential_valid);

	for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
		if ((self_info->committed_valid & (1 << i))) {
			prev_chans[prev_chan_count++] =
				self_info->entry_committed[i].channels[0];
		}
	}

	count = 0;
	while (count < 2) {
		if (count == 1) // count = 1, means the second iteration
		{
			/*Extract and parse the second availability attribute
			 * sent by the peer if any*/
			temp_buffer = (unsigned char *)nan_get_nan_attr_second(
				NAN_AVAILABILITY_ATTR, buffer, size);
			avail_attr = (nan_availability_attr *)temp_buffer;
			ERR("NAN2 : Looking for second availability attribute");
		}
		if (avail_attr) {
			cur_if->pnan_info->self_avail_info.committed_valid = 0;
			cur_if->pnan_info->peer_avail_info.committed_valid = 0;
			cur_if->pnan_info->self_avail_info.conditional_valid =
				0;
			cur_if->pnan_info->peer_avail_info.conditional_valid =
				0;
			cur_if->pnan_info->self_avail_info.entry_potential[0]
				.time_bitmap[0] = NAN_POTENTIAL_BITMAP;
			cur_if->pnan_info->self_avail_info.entry_potential[0]
				.combined_time_bitmap = NAN_POTENTIAL_BITMAP;

			nan_parse_availability(avail_attr, peer_info);

			for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
				if ((peer_info->committed_valid & (1 << i)) &&
				    (in_prev_chans(prev_chans, prev_chan_count,
						   peer_info->entry_committed[i]
							   .channels[0]) ==
				     1)) {
					for (j = 0; j < MAX_SCHEDULE_ENTRIES;
					     j++) {
						if (!(self_info->committed_valid &
						      (1 << j))) {
							self_info
								->entry_committed
									[j]
								.time_bitmap_count =
								peer_info
									->entry_committed
										[i]
									.time_bitmap_count;
							for (k = 0;
							     k <
							     peer_info
								     ->entry_committed
									     [i]
								     .time_bitmap_count;
							     k++) {
								self_info
									->entry_committed
										[j]
									.time_bitmap
										[k] =
									peer_info
										->entry_committed
											[i]
										.time_bitmap
											[k] &
									NAN_POTENTIAL_BITMAP;
								self_info
									->entry_committed
										[j]
									.start_offset
										[k] =
									peer_info
										->entry_committed
											[i]
										.start_offset
											[k];
							}
							self_info
								->committed_valid |=
								(1 << j);
							self_info
								->entry_committed
									[j]
								.op_class =
								peer_info
									->entry_committed
										[i]
									.op_class;
							self_info
								->entry_committed
									[j]
								.channels[0] =
								peer_info
									->entry_committed
										[i]
									.channels[0];
							ERR("NAN2 : Committed entry set at index %u with  channel %u",
							    j,
							    self_info
								    ->entry_committed
									    [j]
								    .channels[0]);
							break;
						}
					}
				}
			}
			nancmd_set_final_bitmap(cur_if, NAN_NDL);
			nan_iface_open();
			if (self_info->committed_valid > 0)
				break;
		}
		count++;
	}

	return NAN_ERR_SUCCESS;
}

enum nan_error nan_send_ndp_req(struct mwu_iface_info *cur_if, nan_ndp_req *req,
				int *ndp_req_id)
{
	int ret = NAN_ERR_SUCCESS;
	u32 ndp_req_bitmap = 0;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;
	int avail_attr_count = 0, cur_avail_index = 0, max_entry_count = 0;
	int ndpe_attr_included = 0;
	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr, req->responder_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = NDP_REQ;
	/* device capability */
	nan_device_capability_attr *device_capa_attr;
	device_capa_attr = (nan_device_capability_attr *)ndp_var_attr_ptr;
	device_capa_attr->attribute_id = NAN_DEVICE_CAPA_ATTR;
	device_capa_attr->len =
		sizeof(nan_device_capability_attr) - NAN_ATTR_HDR_LEN;
	device_capa_attr->committed_dw_info._2g_dw =
		cur_if->pnan_info->awake_dw_interval; // gbhat@HC:available on
						      // all slots on 2.4GHz
	if (cur_if->pnan_info->a_band)
		device_capa_attr->committed_dw_info._5g_dw = 1;
	device_capa_attr->supported_bands =
		cur_if->pnan_info->a_band ? 0x14 : 0x04; // gbhat@HC:only 2.4GHz

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		device_capa_attr->capabilities.ndpe_attr_supported = TRUE;

	if (cur_if->pnan_info->ndpe_attr_negative)
		device_capa_attr->capabilities.ndpe_attr_supported = TRUE;
	// Keep below fields zero
	// device_capa_attr->operation_mode = 0x00;
	// device_capa_attr->no_of_antennas = 0x0;
	device_capa_attr->max_chan_sw_time = 5000;

	ndp_var_attr_ptr += sizeof(nan_device_capability_attr);
	var_sd_attr_len += sizeof(nan_device_capability_attr);

	/* NDP attribute*/
	{
		nan_ndp_attr *ndp_attr;
		NDP_INFO *ndp;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;

		ipv6_link_local_tlv *pipv6_link_local = NULL;
		u16 ipv6_tlv_len = 0;

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported)) {
			if (!cur_if->pnan_info->ndpe_attr_negative) {
				ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
				ndpe_attr_included = 1;
			} else {
				ndp_attr->attribute_id = NAN_NDP_ATTR;
			}
		} else {
			ndp_attr->attribute_id = NAN_NDP_ATTR;
		}
		ndp_attr->dialogue_token =
			++cur_if->pnan_info->dialog_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_REQUEST;
		ndp_attr->type_and_status.status = NDP_NDL_STATUS_CONTINUED;
		ndp_attr->reson_code = NAN_REASON_CODE_DEFAULT;
		memcpy(ndp_attr->initiator_ndi, cur_if->device_mac_addr,
		       ETH_ALEN);
		ndp_attr->ndp_id = 0x1; // TODO
		ndp_attr->ndp_ctrl.confirm_req = req->confirm_required;

		ndp_attr->ndp_ctrl.security_present = req->security;

		ndp_attr->ndp_ctrl.explicit_conf = FALSE;
		// ndp_attr->ndp_ctrl.security_present = FALSE;
		ndp_attr->ndp_ctrl.publish_id_present = TRUE;
		ndp_attr->ndp_ctrl.responder_ndi_present = FALSE;
		ndp_attr->optional[0] = (u8)cur_if->pnan_info->instance_id;

		/*Save NDP related info in NDP_INFO*/
		ndp = &cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0];
		ndp->ndp_id = ndp_attr->ndp_id;
		memcpy(ndp->initiator_ndi, ndp_attr->initiator_ndi, ETH_ALEN);

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported)) {
			if (!cur_if->pnan_info->ndpe_attr_negative) {
				/** Optional IPv6 TLV */
				if (cur_if->pnan_info
					    ->ndpe_attr_iface_identifier) {
					/** publish_id/ndp_id: 1 byte */
					pipv6_link_local =
						(ipv6_link_local_tlv
							 *)(ndp_attr->optional +
							    sizeof(u8));
					pipv6_link_local->header.type =
						TLV_TYPE_IPV6_LINK_LOCAL;
					pipv6_link_local->header.length =
						IPV6_IFACE_IDENTIFIER_LEN;
					nan_get_ipv6_iface_identifier(
						pipv6_link_local->iface_id,
						cur_if->device_mac_addr);
					ipv6_tlv_len +=
						sizeof(ipv6_link_local_tlv);
				}
			}
		}

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported) &&
		    (!cur_if->pnan_info->ndpe_attr_negative)) {
			ndp_attr->len = sizeof(nan_ndp_attr) -
					NAN_ATTR_HDR_LEN + sizeof(u8) +
					ipv6_tlv_len;
			ndp_var_attr_ptr +=
				sizeof(nan_ndp_attr) + 1 + ipv6_tlv_len;
			var_sd_attr_len +=
				sizeof(nan_ndp_attr) + 1 + ipv6_tlv_len;
		} else {
			ndp_attr->len =
				sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN + 1;
			ndp_var_attr_ptr += sizeof(nan_ndp_attr) + 1;
			var_sd_attr_len += sizeof(nan_ndp_attr) + 1;
		}
	}

	if ((cur_if->pnan_info->ndp_attr_present) &&
	    (ndpe_attr_included == 1)) // include ndp attribute if not included
				       // and asked by the WFA CAPI
	{
		nan_ndp_attr *ndp_attr;
		NDP_INFO *ndp;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;

		ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token =
			++cur_if->pnan_info->dialog_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_REQUEST;
		ndp_attr->type_and_status.status = NDP_NDL_STATUS_CONTINUED;
		ndp_attr->reson_code = NAN_REASON_CODE_DEFAULT;
		memcpy(ndp_attr->initiator_ndi, cur_if->device_mac_addr,
		       ETH_ALEN);
		ndp_attr->ndp_id = 0x1; // TODO
		ndp_attr->ndp_ctrl.confirm_req = req->confirm_required;
		ndp_attr->ndp_ctrl.security_present = req->security;

		ndp_attr->ndp_ctrl.explicit_conf = FALSE;
		// ndp_attr->ndp_ctrl.security_present = FALSE;
		ndp_attr->ndp_ctrl.publish_id_present = TRUE;
		ndp_attr->ndp_ctrl.responder_ndi_present = FALSE;
		ndp_attr->optional[0] = (u8)cur_if->pnan_info->instance_id;

		/*Save NDP related info in NDP_INFO*/
		ndp = &cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0];
		ndp->ndp_id = ndp_attr->ndp_id;
		memcpy(ndp->initiator_ndi, ndp_attr->initiator_ndi, ETH_ALEN);

		ndp_attr->len = sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN + 1;
		ndp_var_attr_ptr += sizeof(nan_ndp_attr) + 1;
		var_sd_attr_len += sizeof(nan_ndp_attr) + 1;
	}

	if ((cur_if->pnan_info->set_apple_dut_test) &&
	    (cur_if->pnan_info->avail_attr.len > 0)) { // Just for testing
		nan_availability_attr *availability_attr =
			(nan_availability_attr *)ndp_var_attr_ptr;
		memcpy(availability_attr, cur_if->pnan_info->avail_attr.data,
		       cur_if->pnan_info->avail_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->avail_attr.len;
		var_sd_attr_len += cur_if->pnan_info->avail_attr.len;
		cur_if->pnan_info->avail_attr.len = 0;
	} else {
		cur_avail_index = 0;

		if (cur_if->pnan_info->dual_map) {
			avail_attr_count = 2;
			max_entry_count = 2; // avail entries per avail_attr is
					     // 2
		} else {
			avail_attr_count = 1;
			max_entry_count = MAX_SCHEDULE_ENTRIES;
		}

		while (cur_avail_index < avail_attr_count) {
			nan_availability_attr *availability_attr;
			nan_availability_list *entry;
			nan_channel_entry_list *chan_list;
			u8 opt_fields_len = 7, i = 0;
			u8 sz_of_chan_entry_list = sizeof(chan_entry_list_t);
			u8 sz_of_band_id = sizeof(u8);
			u8 sz_of_entry_len = sizeof(u16);
			u8 op_class, op_chan;
			u8 peer_chan = 0, peer_class = 0;

			if (cur_if->pnan_info->dual_map)
				max_entry_count +=
					(2 * cur_avail_index); // For 1st
							       // avail_attr
							       // max_entry_count
							       // will be 2 and
							       // for second it
							       // will be 4
							       // avail entries
							       // per avail_attr
							       // is 2

			availability_attr =
				(nan_availability_attr *)ndp_var_attr_ptr;
			availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
			availability_attr->attr_ctrl.map_id =
				cur_if->pnan_info->self_avail_info.map_id +
				cur_avail_index; // +cur_avail_index part for
						 // the dual map test
			// availability_attr->seq_id =
			// cur_if->pnan_info->self_avail_info.seq_id+1;
			availability_attr->seq_id =
				cur_if->pnan_info->self_avail_info.seq_id;

			availability_attr->len = sizeof(nan_availability_attr) -
						 NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_availability_attr);
			var_sd_attr_len += sizeof(nan_availability_attr);

			for (i = 2 * cur_avail_index; i < max_entry_count;
			     i++) // avail entries per avail_attr is 2
			{
				/*Send all the valid conditional entries in
				 * proposal*/
				if ((cur_if->pnan_info->self_avail_info
					     .conditional_valid &
				     (1 << i))) {
					INFO("Adding conditional entry#%u", i);
					entry = (nan_availability_list *)
						ndp_var_attr_ptr;
					entry->entry_ctrl.avail_type =
						NDP_AVAIL_TYPE_CONDITIONAL;
					entry->entry_ctrl.usage_preference = 1;
					entry->entry_ctrl.utilization = 0;
					entry->entry_ctrl.rx_nss = 1;
					entry->entry_ctrl.time_bitmap_present =
						1;
					/*Populate optional fields*/
					{
						time_bitmap_control *time_bitmap_ctrl =
							(time_bitmap_control
								 *)&entry
								->optional[0];
						u8 *time_bitmap_len =
							&entry->optional[2];
						u8 *time_bitmap =
							&entry->optional[3];
						u8 valid = 0;
						u32 peer_bitmap = 0;
						peer_availability_info
							*peer_info;
						avail_entry_t *peer_entry =
							NULL;

						peer_info =
							&cur_if->pnan_info
								 ->peer_avail_info_published;
						if (peer_info->committed_valid) {
							peer_entry =
								peer_info
									->entry_committed;
							valid = peer_info
									->committed_valid;
							ERR("committed entry chosen from peer's published avail entries");
						} else if (peer_info
								   ->potential_valid) {
							peer_entry =
								peer_info
									->entry_potential;
							valid = peer_info
									->potential_valid;
							ERR("potential entry chosen from peer's published avail entries");
						}

						/* If both potential and
						   committed entries are present
						   in the publish, take the one
						   which has a bigger bitmap */
						if ((peer_info->committed_valid !=
						     0) &&
						    (peer_info->potential_valid !=
						     0)) {
							if (count_bits(
								    peer_info
									    ->entry_committed
										    [0]
									    .combined_time_bitmap) >
							    count_bits(
								    peer_info
									    ->entry_potential
										    [0]
									    .combined_time_bitmap)) {
								peer_entry =
									peer_info
										->entry_committed;
								valid = peer_info
										->committed_valid;
								ERR("committed entry chosen from peer's published avail entries");
							} else {
								peer_entry =
									peer_info
										->entry_potential;
								valid = peer_info
										->potential_valid;
								ERR("potential entry chosen from peer's published avail entries");
							}
						}

						if (peer_entry && valid) {
							int i;
							for (i = 0;
							     i <
							     MAX_SCHEDULE_ENTRIES;
							     i++) {
								if ((valid) &
								    (1 << i)) {
									{
										if (peer_entry[i]
											    .op_class <
										    86) {
											peer_bitmap =
												peer_entry[i]
													.combined_time_bitmap;
											// peer_chan = peer_entry[i].channels[0];
											// peer_class = peer_entry[i].op_class;
											peer_chan =
												DEFAULT_2G_OP_CHAN;
											peer_class =
												DEFAULT_2G_OP_CLASS;
											ERR("peer bit map found in 2.4 band (%x) ",
											    peer_bitmap);
											// break;
										}
									}

									if (cur_if->pnan_info
										    ->a_band) {
										/*Find 5G entry from peers publish*/
										if ((peer_entry[i]
											     .op_class >
										     86)) {
											peer_bitmap =
												peer_entry[i]
													.combined_time_bitmap;
											// peer_chan = peer_entry[i].channels[0];
											// peer_class = peer_entry[i].op_class;
											peer_chan =
												DEFAULT_5G_OP_CHAN;
											peer_class =
												DEFAULT_5G_OP_CLASS;
											ERR("peer bit map found in a band (%x) ",
											    peer_bitmap);
											break;
										}
									}
									/*
													else
													{
													    if(peer_entry[i].op_class < 86)
													    {
														peer_bitmap = peer_entry[i].combined_time_bitmap;
														//peer_chan = peer_entry[i].channels[0];
														//peer_class = peer_entry[i].op_class;
														peer_chan = DEFAULT_2G_OP_CHAN;
														peer_class = DEFAULT_2G_OP_CLASS;
														ERR("peer bit map found in 2.4 band (%x) ", peer_bitmap);
														break;
													    }
													}
									*/
								}
							}
							/*If no a_band
							 entry(5GHZ) is
							 published by peer
							 pickup the 1st
							 available entry*/
							if (cur_if->pnan_info
								    ->a_band &&
							    !(peer_chan &&
							      peer_bitmap)) {
								peer_bitmap =
									peer_entry[0]
										.combined_time_bitmap;
								peer_chan =
									peer_entry[0]
										.channels[0];
								peer_class =
									peer_entry[0]
										.op_class;
							}
							/*If no entry is
							 * published by peer
							 * assume defaults*/
							if (!(peer_chan &&
							      peer_bitmap)) {
								peer_bitmap =
									0xffffffff;
								peer_chan =
									cur_if->pnan_info
											->a_band ?
										DEFAULT_2G_OP_CHAN :
										DEFAULT_5G_OP_CHAN;
								peer_class =
									cur_if->pnan_info
											->a_band ?
										DEFAULT_2G_OP_CLASS :
										DEFAULT_5G_OP_CLASS;
							}
						}

						time_bitmap_ctrl->bit_duration =
							NDP_TIME_BM_DUR_16;
						time_bitmap_ctrl->bit_period =
							NDP_TIME_BM_PERIOD_512;
						time_bitmap_ctrl->start_offset =
							0;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						ERR("peer bit map (%x) ",
						    peer_bitmap);
						// Request the ndl schedule
						// taking into consideration the
						// peer's bitmap
						ndp_req_bitmap =
							((cur_if->pnan_info
								  ->self_avail_info
								  .entry_conditional
									  [i]
								  .time_bitmap
									  [0]) &
							 peer_bitmap);

						if (cur_if->pnan_info
							    ->invalid_sched_required)
							ndp_req_bitmap =
								cur_if->pnan_info
									->self_avail_info
									.entry_conditional
										[i]
									.time_bitmap
										[0];
						ERR("ndp bit map (%x) ",
						    ndp_req_bitmap);

						memcpy(time_bitmap,
						       &ndp_req_bitmap,
						       4); /*time bitmap*/
					}
					/*Hop over by (avail list +
					 * optional)bytes to get chan list */
					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 1;
					chan_list->entry_ctrl.band_type = 0;
					chan_list->entry_ctrl.num_entries = 1;
					if (cur_if->pnan_info->self_avail_info
						    .entry_conditional[i]
						    .op_class == 0) {
						op_class =
							peer_class; // assign
								    // op_class
								    // from
								    // peer's
								    // entry
								    // received
					} else { // use the one set through
						 // set_ndl
						op_class =
							cur_if->pnan_info
								->self_avail_info
								.entry_conditional
									[i]
								.op_class;
					}
					if (cur_if->pnan_info->self_avail_info
						    .entry_conditional[i]
						    .channels[0] == 0) {
						op_chan = peer_chan; // assign
								     // channel
								     // from
								     // peer's
								     // entry
								     // received
					} else { // use the one set through
						 // set_ndl
						op_chan =
							cur_if->pnan_info
								->self_avail_info
								.entry_conditional
									[i]
								.channels[0];
					}

					chan_list->chan_band_entry.chan_entry
						.op_class = op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap =
						ndp_get_chan_bitmap(op_class,
								    op_chan);
					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len -
						sz_of_entry_len;
					availability_attr->len +=
						entry->len + sz_of_entry_len;
					ndp_var_attr_ptr +=
						entry->len + sz_of_entry_len;
					var_sd_attr_len +=
						entry->len + sz_of_entry_len;
				}
			}
			// Add the potential entry
			{
				nan_availability_list *potential_entry;
				potential_entry = (nan_availability_list *)
					ndp_var_attr_ptr;

				potential_entry->entry_ctrl.avail_type =
					NDP_AVAIL_TYPE_POTENTIAL;
				potential_entry->entry_ctrl.usage_preference =
					1;
				potential_entry->entry_ctrl.utilization = 0;
				potential_entry->entry_ctrl.rx_nss = 1;

				if (cur_if->pnan_info->self_avail_info
					    .band_entry_potential) {
					/*Device available on all slots on the
					 * given band*/
					potential_entry->entry_ctrl
						.time_bitmap_present = 1;
					cur_if->pnan_info->self_avail_info
						.time_bitmap_present_potential =
						1;
					/*Populate optional fields*/
					{
						time_bitmap_control *time_bitmap_ctrl =
							(time_bitmap_control
								 *)&potential_entry
								->optional[0];
						u8 *time_bitmap_len =
							&potential_entry
								 ->optional[2];
						u8 *time_bitmap =
							&potential_entry
								 ->optional[3];
						u32 potential_bitmap =
							NAN_POTENTIAL_BITMAP;

						time_bitmap_ctrl->bit_duration =
							NDP_TIME_BM_DUR_16;
						time_bitmap_ctrl->bit_period =
							NDP_TIME_BM_PERIOD_512;
						time_bitmap_ctrl->start_offset =
							0;
						*time_bitmap_len =
							4; /*time_bitmap_len*/

						memcpy(time_bitmap,
						       &potential_bitmap,
						       4); /*time bitmap*/
					}
					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 0x0;
					chan_list->entry_ctrl.band_type = 0x0;
					if (cur_if->pnan_info->a_band)
						chan_list->entry_ctrl
							.num_entries = 0x2;
					else
						chan_list->entry_ctrl
							.num_entries = 0x1;

					chan_list->chan_band_entry.band_id =
						0x02; /*2.4 GHz*/

					/*For dual band device add potential
					 * band entry on both the bands*/
					if (cur_if->pnan_info->a_band) {
						u8 *temp =
							(u8 *)(&chan_list
									->chan_band_entry
									.band_id);
						temp++;
						*temp = 0x04; /*5 GHz*/
						sz_of_band_id++;
					}

					potential_entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) -
						sz_of_entry_len -
						sz_of_chan_entry_list +
						sz_of_band_id +
						opt_fields_len; // set the
								// potential
								// entry's
								// length
					// add the potetial entry's length in
					// the main avail_attr and other
					// pointers
					availability_attr->len +=
						potential_entry->len +
						sz_of_entry_len;
					ndp_var_attr_ptr +=
						potential_entry->len +
						sz_of_entry_len;
					var_sd_attr_len +=
						potential_entry->len +
						sz_of_entry_len;

				} else { // non-band entry

					potential_entry->entry_ctrl
						.time_bitmap_present = 1;
					/*Populate optional fields*/
					{
						time_bitmap_control *time_bitmap_ctrl =
							(time_bitmap_control
								 *)&potential_entry
								->optional[0];
						u8 *time_bitmap_len =
							&potential_entry
								 ->optional[2];
						u8 *time_bitmap =
							&potential_entry
								 ->optional[3];

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
					/*Hop over by 5 bytes(time_bitmap_len +
					 * time_bitmap) to get chan list */
					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 1;
					chan_list->entry_ctrl.band_type = 0;
					chan_list->entry_ctrl.num_entries = 1;

					chan_list->chan_band_entry.chan_entry
						.op_class =
						cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap = ndp_get_chan_bitmap(
						cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.op_class,
						cur_if->pnan_info
							->self_avail_info
							.entry_potential[0]
							.channels[0]);
					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					// set the potential entry's length
					potential_entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len -
						sz_of_entry_len;
					// add the potetial entry's length in
					// the main avail_attr and other
					// pointers
					availability_attr->len +=
						potential_entry->len +
						sz_of_entry_len;
					ndp_var_attr_ptr +=
						potential_entry->len +
						sz_of_entry_len;
					var_sd_attr_len +=
						potential_entry->len +
						sz_of_entry_len;
				}
			} // end of the block adding potential entry

			cur_avail_index++;
			mwu_hexdump(MSG_INFO, "avail attr",
				    (u8 *)availability_attr,
				    availability_attr->len + 3);
		}
	} // for the else part of avail_attr_len>0

	/* NDC attribute */
	if (cur_if->pnan_info->ndc_attr.len > 0) {
		nan_ndc_attr *ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		memcpy(ndc_attr, cur_if->pnan_info->ndc_attr.data,
		       cur_if->pnan_info->ndc_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndc_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndc_attr.len;
	} else {
		nan_ndc_attr *ndc_attr;
		/*Setting prefered bitmap for 2.4 or 5 g bands*/
		u32 bitmap = cur_if->pnan_info->a_band ? 0x0200 : 0x0002;
		u8 wfa_ndc_id[6] = {0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00};
		ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		ndc_attr->attribute_id = NAN_NDC_ATTR;
		memcpy(ndc_attr->ndc_id, wfa_ndc_id, ETH_ALEN);
		ndc_attr->map_id =
			cur_if->pnan_info->self_avail_info.map_id; // TODO Put
								   // map id of
								   // Avail attr
								   // here
		ndc_attr->sched_ctrl.proposed_ndc = 0x1;
		ndc_attr->time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
		ndc_attr->time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
		ndc_attr->time_bitmap_ctrl.start_offset = 0;
		ndc_attr->time_bitmap_len = 4;

		if ((!(cur_if->pnan_info->invalid_sched_required)) &&
		    ((ndp_req_bitmap & bitmap) == 0)) {
			generate_ndc_bitmap(ndp_req_bitmap, &bitmap);
		}

		memcpy(ndc_attr->bitmap, &bitmap, sizeof(u32)); // Hardcoded as
								// 2nd slot,
								// just after DW
		/*Save NDC related info in NDC_INFO*/
		cur_if->pnan_info->ndc_info[0].slot = bitmap;
		memcpy(cur_if->pnan_info->ndc_info[0].ndc_id, wfa_ndc_id,
		       ETH_ALEN);

		ndc_attr->len = sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
		var_sd_attr_len += sizeof(nan_ndc_attr) + 4;
	}

	/* NDL attribute */
	if (cur_if->pnan_info->ndl_attr.len > 0) {
		nan_ndl_attr *ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		memcpy(ndl_attr, cur_if->pnan_info->ndl_attr.data,
		       cur_if->pnan_info->ndl_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndl_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndl_attr.len;
	} else {
		nan_ndl_attr *ndl_attr;
		ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		ndl_attr->attribute_id = NAN_NDL_ATTR;
		u8 sz_of_immutable_sched = 0;
		ndl_attr->dialogue_token = cur_if->pnan_info->dialog_token++;
		ndl_attr->type_and_status.type = NDP_NDL_TYPE_REQUEST;
		ndl_attr->type_and_status.status = NDP_NDL_STATUS_CONTINUED;
		ndl_attr->reson_code = NAN_REASON_CODE_DEFAULT; // gbhat@HC,
								// reserved
		ndl_attr->ndl_ctrl.ndl_peer_id_present = 0x0; // Only NDC
							      // attribute
							      // present bit set
							      // and peer id
							      // present bit set
		if (cur_if->pnan_info->immutable_sched_required)
			ndl_attr->ndl_ctrl.immutable_sched_present = 0x1;
		else
			ndl_attr->ndl_ctrl.immutable_sched_present = 0x0;
		ndl_attr->ndl_ctrl.ndc_attr_present = 0x1; // Only NDC attribute
							   // present bit set
							   // and peer id
							   // present bit set
		ndl_attr->ndl_ctrl.ndl_qos_attr_present = 0x1;
		if (cur_if->pnan_info->immutable_sched_required) {
			immutable_schedule *imm_sched =
				(immutable_schedule *)&ndl_attr->optional[0];
			u32 bitmap;
			/*
			if(cur_if->pnan_info->a_band)
			    bitmap = 0x0200;
			else
			    bitmap = 0x02;
			    */
			if (cur_if->pnan_info->immutable_bitmap)
				bitmap = cur_if->pnan_info->immutable_bitmap;
			else
				bitmap = 0x00010000; // Due to the negative test
						     // case, we can decide the
						     // general policy later

			if ((!(cur_if->pnan_info->invalid_sched_required)) &&
			    ((ndp_req_bitmap & bitmap) == 0)) {
				generate_imm_bitmap(ndp_req_bitmap, &bitmap);
			}

			INFO("NAN2: Immutable bitmap set %x", bitmap);
			// imm_sched->no_of_entries = 1;
			imm_sched->entry[0].map_id =
				cur_if->pnan_info->self_avail_info.map_id;
			imm_sched->entry[0].time_bitmap_ctrl.bit_duration =
				NDP_TIME_BM_DUR_16;
			imm_sched->entry[0].time_bitmap_ctrl.bit_period =
				NDP_TIME_BM_PERIOD_512;
			imm_sched->entry[0].time_bitmap_ctrl.start_offset = 0;
			imm_sched->entry[0].time_bitmap_len = 4;
			memcpy(&imm_sched->entry[0].bitmap, &bitmap, 4);
			sz_of_immutable_sched = sizeof(immutable_schedule) +
						sizeof(immutable_sched_entry) +
						4;
		}

		ndl_attr->len = sizeof(nan_ndl_attr) + sz_of_immutable_sched -
				NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr +=
			sizeof(nan_ndl_attr) + sz_of_immutable_sched;
		var_sd_attr_len += sizeof(nan_ndl_attr) + sz_of_immutable_sched;
	}

	/*NDL QoS Attribute*/
	{
		nan_ndl_qos_attr *ndl_qos_attr =
			(nan_ndl_qos_attr *)ndp_var_attr_ptr;
		ndl_qos_attr->attribute_id = NAN_NDL_QOS_ATTR;
		ndl_qos_attr->min_slots =
			cur_if->pnan_info->self_avail_info.qos.min_slots;
		memcpy(ndl_qos_attr->max_latency,
		       &cur_if->pnan_info->self_avail_info.qos.max_latency,
		       sizeof(ndl_qos_attr->max_latency));
		ndl_qos_attr->len = sizeof(nan_ndl_qos_attr) - NAN_ATTR_HDR_LEN;

		ndp_var_attr_ptr += sizeof(nan_ndl_qos_attr);
		var_sd_attr_len += sizeof(nan_ndl_qos_attr);
	}
	/* container attribute */
	{
		u8 supported_rates[6] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
		u8 ext_supported_rates[10] = {0x32, 0x08, 0x0c, 0x12, 0x18,
					      0x24, 0x30, 0x48, 0x60, 0x6c};
		u8 htcap[28] = {0x2d, 0x1a, 0xee, 0x01, 0x1f, 0xff, 0xff,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		u8 htinfo[24] = {0x3d, 0x16, 0x06, 0x05, 0x15, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		u8 *elem_ptr;
		u16 elem_len = 0;

		nan_element_container_attr *element_container_attr;
		element_container_attr =
			(nan_element_container_attr *)ndp_var_attr_ptr;
		elem_ptr = (u8 *)(&element_container_attr->elements[0]);
		element_container_attr->attribute_id =
			NAN_ELEMENT_CONTAINER_ATTR;
		element_container_attr->len =
			sizeof(supported_rates) + sizeof(ext_supported_rates) +
			sizeof(htcap) + sizeof(htinfo) + 1;

		memcpy(elem_ptr, supported_rates, sizeof(supported_rates));
		// mwu_hexdump(MSG_INFO, "elem", elem_ptr,
		// sizeof(supported_rates)); mwu_hexdump(MSG_INFO, "suppor
		// rates", supported_rates, sizeof(supported_rates));
		elem_ptr += sizeof(supported_rates);
		elem_len += sizeof(supported_rates);

		memcpy(elem_ptr, ext_supported_rates,
		       sizeof(ext_supported_rates));
		elem_ptr += sizeof(ext_supported_rates);
		elem_len += sizeof(ext_supported_rates);

		memcpy(elem_ptr, htcap, sizeof(htcap));
		elem_ptr += sizeof(htcap);
		elem_len += sizeof(htcap);

		memcpy(elem_ptr, htinfo, sizeof(htinfo));
		elem_ptr += sizeof(htinfo);
		elem_len += sizeof(htinfo);

		ndp_var_attr_ptr = ndp_var_attr_ptr +
				   sizeof(nan_element_container_attr) +
				   elem_len;
		var_sd_attr_len = var_sd_attr_len +
				  sizeof(nan_element_container_attr) + elem_len;
	}

	/*Security attributes*/
	if (cur_if->pnan_info->security_required) {
		{
			/*Cipher suite info attr*/
			nan_cipher_suite_info_attr *csinfo =
				(nan_cipher_suite_info_attr *)ndp_var_attr_ptr;
			csinfo->attribute_id = NAN_CIPHER_SUITE_INFO_ATTR;
			csinfo->capabilities = 0;
			csinfo->attr_info[0].cipher_suite_id = NCS_SK_CCM_128;
			csinfo->attr_info[0].instance_id =
				cur_if->pnan_info->instance_id; /*All services*/

			csinfo->len = sizeof(nan_cipher_suite_info_attr) +
				      sizeof(nan_cipher_suite_attr_info) -
				      NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_cipher_suite_info_attr) +
					    sizeof(nan_cipher_suite_attr_info);
			var_sd_attr_len += sizeof(nan_cipher_suite_info_attr) +
					   sizeof(nan_cipher_suite_attr_info);
		}
		{
			/*Security context info attr*/
			nan_security_context_info_attr *sec_ctx_info =
				(nan_security_context_info_attr *)
					ndp_var_attr_ptr;
#if 0
            u8 pmkid[PMKID_LEN] = {0}, svcid = 0;

            /*TODO : pass correct service name entered by user in lower case*/
            u8 *service_name = "test" ;
            size_t service_len = strlen((const char *)service_name);
            u8 service_id[SERVICE_ID_LEN] ;

			nan_generate_service_id(service_name, service_len, service_id);
#endif
			u8 pmkid[PMKID_LEN] = {0};

			nan_generate_pmkid(
				cur_if->pnan_info->nan_security.pmk,
				(size_t)cur_if->pnan_info->nan_security.pmk_len,
				(u8 *)cur_if->device_mac_addr,
				(u8 *)req->responder_mac,
				cur_if->pnan_info->s_service.service_hash,
				pmkid);

			mwu_hexdump(MSG_ERROR, "default service ID",
				    cur_if->pnan_info->s_service.service_hash,
				    SERVICE_ID_LEN);
			memcpy(cur_if->pnan_info->nan_security.nan_pmkid, pmkid,
			       PMKID_LEN); /*Copy PMKID*/

			sec_ctx_info->attribute_id = NAN_SEC_CONTEXT_INFO_ATTR;
			sec_ctx_info->identifier_list[0].identifier_len =
				PMKID_LEN;
			sec_ctx_info->identifier_list[0].identifier_type =
				1 /*PMKID*/;
			sec_ctx_info->identifier_list[0].instance_id =
				cur_if->pnan_info->instance_id /*All services*/;
			memcpy(&sec_ctx_info->identifier_list[0]
					.sec_context_identifier,
			       pmkid, PMKID_LEN); /*Copy PMKID*/

			sec_ctx_info->len =
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) + 16 -
				NAN_ATTR_HDR_LEN;

			ndp_var_attr_ptr +=
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) + 16;
			var_sd_attr_len +=
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) + 16;
		}
		/*Shared key desc attr*/
		{
			/* >>>> TBD <<<<<< Put Key desc for M1 */

			eapol_key_frame *m1_eapol;
			nan_shared_key_desc_attr *key_desc =
				(nan_shared_key_desc_attr *)ndp_var_attr_ptr;
			u8 sz_key_desc;
			u8 *temp;
			temp = (u8 *)sdf_buf;

			sz_key_desc = sizeof(eapol_key_frame);
			key_desc->attribute_id = NAN_SHARED_KEY_DESC_ATTR;
			key_desc->instance_id =
				cur_if->pnan_info->instance_id; /*all
								   instances*/

			nan_alloc_key_desc_buf(&m1_eapol);

			nan_generate_key_desc_m1(m1_eapol);

			memcpy(cur_if->pnan_info->nan_security.local_key_nonce,
			       m1_eapol->key_nonce, NONCE_LEN);
			memcpy(&key_desc->rsna_key_desc, m1_eapol, sz_key_desc);

			nan_free_key_desc_buf(&m1_eapol);

			ERR("sizeof(nan_shared_key_desc_attr) %lu",
			    sizeof(nan_shared_key_desc_attr));
			ERR("sz_key_desc %d", sz_key_desc);

			key_desc->len = sizeof(nan_shared_key_desc_attr) +
					sz_key_desc - NAN_ATTR_HDR_LEN;

			ERR("key_desc->len %d", key_desc->len);

			ndp_var_attr_ptr +=
				sizeof(nan_shared_key_desc_attr) + sz_key_desc;
			var_sd_attr_len +=
				sizeof(nan_shared_key_desc_attr) + sz_key_desc;

			ERR("DBG var sd attr len %d", var_sd_attr_len);
			/*Save body of M1 as it is required to calculate auth
			 * token for M3*/
			cur_if->pnan_info->nan_security.m1 =
				(struct nan_sec_buf *)malloc(sizeof(nan_ndp) +
							     var_sd_attr_len +
							     NAN_BUF_HDR_SIZE);

			memcpy(cur_if->pnan_info->nan_security.m1->buf, temp,
			       sizeof(nan_ndp) + var_sd_attr_len);
			cur_if->pnan_info->nan_security.m1->size =
				sizeof(nan_ndp) + var_sd_attr_len;
		}
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out NDP request");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	if (ret != NAN_ERR_SUCCESS)
		ERR("nan_cmdbuf_send failed");

	/*Generate NDP request*/
	/*Tx NDP response*/
	/*Change state*/
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_generate_validate_pmkid(const u8 *pmk, size_t pmk_len,
					   const u8 *IAddr, const u8 *RAddr,
					   u8 *nan_pmkid, u8 *service_id,
					   u8 *m1_buf, int m1_size)
{
	int ret = NAN_ERR_SUCCESS;
	nan_security_context_info_attr *sec_temp_desc;
	u8 pmkid_buf[PMKID_LEN];
	u8 pmkid[PMKID_LEN];
#if 0
    u8 *service_name = "test";
    u8 service_id[SERVICE_ID_LEN] ;
    size_t service_len = strlen((const char *)service_name);

    nan_generate_service_id(service_name, service_len, service_id);
#endif
	nan_generate_pmkid(pmk, pmk_len, IAddr, RAddr, service_id, pmkid);

	memcpy(nan_pmkid, pmkid, PMKID_LEN); /*Copy PMKID*/

	sec_temp_desc = (nan_security_context_info_attr *)nan_get_nan_attr(
		NAN_SEC_CONTEXT_INFO_ATTR, m1_buf, m1_size);

	memcpy(&pmkid_buf,
	       &sec_temp_desc->identifier_list[0].sec_context_identifier,
	       PMKID_LEN);

	mwu_hexdump(MSG_INFO, "M1 PMKID", (u8 *)&pmkid_buf, PMKID_LEN);
	mwu_hexdump(MSG_INFO, "M2 PMKID", nan_pmkid, PMKID_LEN);

	/*Check if PMKID is same for M1 and M2, send M2 only if it is same*/

	if (memcmp(&pmkid_buf, nan_pmkid, PMKID_LEN) == 0) {
		ret = NAN_ERR_SUCCESS;

	} else {
		ret = NAN_ERR_RANGE;
		ERR("PMKID mismatch, M2 key descriptor not updated\n");
	}
	return ret;
}

enum nan_error nan_send_schedule_resp(struct mwu_iface_info *cur_if,
				      nan_ndp_req *req, u8 ndl_status,
				      u8 ndp_status, u8 reason)
{
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr,
	       &cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = SCHEDULE_RESPONSE;

	{ // NAN Availability attr

		nan_availability_attr *availability_attr;
		nan_availability_list *entry;
		nan_channel_entry_list *chan_list;
		u8 opt_fields_len = 7, entry_valid = 0, i = 0, k = 0;
		avail_entry_t *self_entry = NULL;

		if (ndl_status == NDP_NDL_STATUS_CONTINUED) {
			self_entry = cur_if->pnan_info->self_avail_info
					     .entry_conditional;
			entry_valid = cur_if->pnan_info->self_avail_info
					      .conditional_valid;
		} else if (ndl_status == NDP_NDL_STATUS_ACCEPTED) {
			self_entry = cur_if->pnan_info->self_avail_info
					     .entry_committed;
			entry_valid = cur_if->pnan_info->self_avail_info
					      .committed_valid;
		}

		availability_attr = (nan_availability_attr *)ndp_var_attr_ptr;
		availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
		availability_attr->attr_ctrl.map_id =
			cur_if->pnan_info->self_avail_info.map_id;
		availability_attr->seq_id =
			++cur_if->pnan_info->self_avail_info.seq_id;

		availability_attr->len =
			sizeof(nan_availability_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_availability_attr);
		var_sd_attr_len += sizeof(nan_availability_attr);

		for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
			/*Send all the valid conditional/committed entries in
			 * resp*/
			if ((entry_valid & (1 << i))) {
				for (k = 0; k < self_entry[i].time_bitmap_count;
				     k++) {
					INFO("Adding conditional/committed entry#%u",
					     i);
					entry = (nan_availability_list *)
						ndp_var_attr_ptr;
					if (ndl_status ==
					    NDP_NDL_STATUS_CONTINUED) {
						entry->entry_ctrl.avail_type =
							NDP_AVAIL_TYPE_CONDITIONAL;
					} else {
						entry->entry_ctrl.avail_type =
							NDP_AVAIL_TYPE_COMMITTED;
					}

					entry->entry_ctrl.usage_preference =
						0x1;
					entry->entry_ctrl.utilization = 0x0;
					entry->entry_ctrl.rx_nss = 0x1;
					entry->entry_ctrl.time_bitmap_present =
						0x1;
					/*Populate optional attributes*/
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
							self_entry[i].period;
						time_bitmap_ctrl->start_offset =
							self_entry[i]
								.start_offset[k] /
							16;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						memcpy(time_bitmap,
						       &self_entry[i]
								.time_bitmap[k],
						       4); /*time bitmap*/
					}
					INFO("With time_bitmap 0x%x , offset %u , op_class %u and op_chan %u",
					     self_entry[i].time_bitmap[k],
					     self_entry[i].start_offset[k],
					     self_entry[i].op_class,
					     self_entry[i].channels[0]);

					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 0x1;
					chan_list->entry_ctrl.band_type = 0x0;
					chan_list->entry_ctrl.num_entries = 0x1;
					chan_list->chan_band_entry.chan_entry
						.op_class =
						self_entry[i].op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap =
						ndp_get_chan_bitmap(
							self_entry[i].op_class,
							self_entry[i]
								.channels[0]);
					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len - 2;
					availability_attr->len +=
						entry->len + 2;
					ndp_var_attr_ptr += entry->len + 2;
					var_sd_attr_len += entry->len + 2;
				}
			}
		}
	}

	/* NDC attribute */
	if (cur_if->pnan_info->ndc_attr.len > 0) {
		nan_ndc_attr *ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		memcpy(ndc_attr, cur_if->pnan_info->ndc_attr.data,
		       cur_if->pnan_info->ndc_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndc_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndc_attr.len;
	} else {
		nan_ndc_attr *ndc_attr;
		ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		ndc_attr->attribute_id = NAN_NDC_ATTR;
		memcpy(ndc_attr->ndc_id, cur_if->pnan_info->ndc_info[0].ndc_id,
		       ETH_ALEN);
		ndc_attr->map_id =
			cur_if->pnan_info->self_avail_info.map_id; // TODO Put
								   // map id of
								   // Avail attr
								   // here
		/*
		if(ndl_status == NDP_NDL_STATUS_CONTINUED){
		    ndc_attr->sched_ctrl.proposed_ndc = TRUE;
		}
		else{
		    ndc_attr->sched_ctrl.proposed_ndc = FALSE;
		}
		*/
		ndc_attr->sched_ctrl.proposed_ndc = TRUE;
		ndc_attr->time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
		ndc_attr->time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
		ndc_attr->time_bitmap_ctrl.start_offset = 0;
		ndc_attr->time_bitmap_len = 4;
		memcpy(ndc_attr->bitmap, &(cur_if->pnan_info->ndc_info[0].slot),
		       sizeof(u32));

		ndc_attr->len = sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
		var_sd_attr_len += sizeof(nan_ndc_attr) + 4;
	}

	/* NDL attribute */
	if (cur_if->pnan_info->ndl_attr.len > 0) {
		nan_ndl_attr *ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		memcpy(ndl_attr, cur_if->pnan_info->ndl_attr.data,
		       cur_if->pnan_info->ndl_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndl_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndl_attr.len;
	} else {
		nan_ndl_attr *ndl_attr;
		ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		ndl_attr->attribute_id = NAN_NDL_ATTR;
		ndl_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token;
		ndl_attr->type_and_status.type = NDP_NDL_TYPE_RESPONSE;
		ndl_attr->type_and_status.status = ndl_status;
		ndl_attr->reson_code = reason; // gbhat@HC, reserved
		ndl_attr->ndl_ctrl.ndl_peer_id_present = 0x0; // Only NDC
							      // attribute
							      // present bit set
							      // and peer id
							      // present bit set
		ndl_attr->ndl_ctrl.immutable_sched_present =
			0x0; // Only NDC attribute present bit set and peer id
			     // present bit set
		ndl_attr->ndl_ctrl.ndc_attr_present = 0x1; // Only NDC attribute
							   // present bit set
							   // and peer id
							   // present bit set
		ndl_attr->ndl_ctrl.ndl_qos_attr_present =
			0x1; // Only NDC attribute present bit set and peer id
			     // present bit set

		ndl_attr->len = sizeof(nan_ndl_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndl_attr);
		var_sd_attr_len += sizeof(nan_ndl_attr);
	}

	/*NDL QoS Attribute*/
	{
		nan_ndl_qos_attr *ndl_qos_attr =
			(nan_ndl_qos_attr *)ndp_var_attr_ptr;
		ndl_qos_attr->attribute_id = NAN_NDL_QOS_ATTR;
		ndl_qos_attr->min_slots =
			cur_if->pnan_info->self_avail_info.qos.min_slots;
		memcpy(ndl_qos_attr->max_latency,
		       &cur_if->pnan_info->self_avail_info.qos.max_latency,
		       sizeof(ndl_qos_attr->max_latency));
		ndl_qos_attr->len = sizeof(nan_ndl_qos_attr) - NAN_ATTR_HDR_LEN;

		ndp_var_attr_ptr += sizeof(nan_ndl_qos_attr);
		var_sd_attr_len += sizeof(nan_ndl_qos_attr);
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out Schedule response");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	return ret;
}

enum nan_error nan_send_ndp_resp(struct mwu_iface_info *cur_if,
				 nan_ndp_req *req, u8 ndl_status, u8 ndp_status,
				 u8 reason)
{
	/*Process NDP request*/
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;
	int ndpe_attr_included = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr,
	       &cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = NDP_RESP;
	/* device capability */
	nan_device_capability_attr *device_capa_attr;
	device_capa_attr = (nan_device_capability_attr *)ndp_var_attr_ptr;
	device_capa_attr->attribute_id = NAN_DEVICE_CAPA_ATTR;
	device_capa_attr->len =
		sizeof(nan_device_capability_attr) - NAN_ATTR_HDR_LEN;
	device_capa_attr->committed_dw_info._2g_dw =
		cur_if->pnan_info->awake_dw_interval; // gbhat@HC:available on
						      // all slots on 2.4GHz
	if (cur_if->pnan_info->a_band)
		device_capa_attr->committed_dw_info._5g_dw = 1;
	device_capa_attr->supported_bands =
		cur_if->pnan_info->a_band ? 0x14 : 0x04; // gbhat@HC:only 2.4GHz

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		device_capa_attr->capabilities.ndpe_attr_supported = TRUE;

	if (cur_if->pnan_info->ndpe_attr_negative)
		device_capa_attr->capabilities.ndpe_attr_supported = TRUE;
	// Keep below fields zero
	// device_capa_attr->operation_mode = 0x00;

	// device_capa_attr->no_of_antennas = 0x0;
	device_capa_attr->max_chan_sw_time = 5000;

	ndp_var_attr_ptr += sizeof(nan_device_capability_attr);
	var_sd_attr_len += sizeof(nan_device_capability_attr);

	/* NDP attribute*/
	{
		nan_ndp_attr *ndp_attr;
		NDP_INFO *ndp;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;
		ipv6_link_local_tlv *pipv6_link_local = NULL;
		service_info_tlv *pserv_info = NULL;
		transport_port_sub_attr *ptrans_port = NULL;
		transport_protocol_sub_attr *ptrans_protocol = NULL;
		u16 ipv6_tlv_len = 0, serv_info_tlv_len = 0;

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported)) {
			if (!cur_if->pnan_info->ndpe_attr_negative) {
				ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
				INFO("####here: 11: %d",
				     cur_if->pnan_info->ndpe_attr_negative);
				ndpe_attr_included = 1;
			} else {
				ndp_attr->attribute_id = NAN_NDP_ATTR;
				INFO("####here: 22: %d",
				     cur_if->pnan_info->ndpe_attr_negative);
			}
		} else {
			ndp_attr->attribute_id = NAN_NDP_ATTR;
		}
		ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_RESPONSE;
		ndp_attr->type_and_status.status = ndp_status;
		ndp_attr->reson_code = reason;
		memcpy(ndp_attr->initiator_ndi,
		       cur_if->pnan_info->ndc_info[0]
			       .ndl_info[0]
			       .ndp_info[0]
			       .initiator_ndi,
		       ETH_ALEN);
		ndp_attr->ndp_id = cur_if->pnan_info->ndc_info[0]
					   .ndl_info[0]
					   .ndp_info[0]
					   .ndp_id; // 0x1
		ndp_attr->ndp_ctrl.confirm_req =
			cur_if->pnan_info->confirm_required;

		ndp_attr->ndp_ctrl.security_present =
			cur_if->pnan_info->security_required;

		ndp_attr->ndp_ctrl.explicit_conf =
			ndp_attr->ndp_ctrl.confirm_req ? TRUE : FALSE;
		ndp_attr->ndp_ctrl.publish_id_present = FALSE;
		ndp_attr->ndp_ctrl.responder_ndi_present = TRUE;
		ndp = &cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0];
		memcpy(&ndp_attr->optional[0], ndp->responder_ndi, ETH_ALEN);

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported)) {
			if (!cur_if->pnan_info->ndpe_attr_negative) {
				/** Optional IPv6 TLV */
				if (cur_if->pnan_info
					    ->ndpe_attr_iface_identifier) {
					pipv6_link_local =
						(ipv6_link_local_tlv
							 *)(ndp_attr->optional +
							    ETH_ALEN);
					pipv6_link_local->header.type =
						TLV_TYPE_IPV6_LINK_LOCAL;
					pipv6_link_local->header.length =
						IPV6_IFACE_IDENTIFIER_LEN;
					nan_get_ipv6_iface_identifier(
						pipv6_link_local->iface_id,
						cur_if->device_mac_addr);
					ipv6_tlv_len +=
						sizeof(ipv6_link_local_tlv);
				}
				/** Service info TLV */
				pserv_info =
					(service_info_tlv *)(ndp_attr->optional +
							     ETH_ALEN) +
					ipv6_tlv_len;
				pserv_info->header.type = TLV_TYPE_SERVICE_INFO;
				pserv_info->oui[0] = 0x50;
				pserv_info->oui[1] = 0x6f;
				pserv_info->oui[2] = 0x9a;
				pserv_info->service_protocol_type =
					SERVICE_PROTOCOL_GENERIC;
				serv_info_tlv_len += sizeof(service_info_tlv);
				/** Transport port sub-attribute */
				ptrans_port =
					(transport_port_sub_attr *)
						pserv_info->service_spc_info;
				ptrans_port->header.type =
					SERVICE_PROTOCOL_TRANSPORT_PORT_SUB_ATTR;
				ptrans_port->header.length = sizeof(u16);
				ptrans_port->transport_port =
					cur_if->pnan_info
						->ndpe_attr_trans_port; // need
									// to
									// set
									// it to
									// required
									// port
				serv_info_tlv_len +=
					sizeof(transport_port_sub_attr);
				/** Optional transport protocol attribute */
				if ((cur_if->pnan_info->ndpe_attr_protocol !=
				     0) &&
				    (cur_if->pnan_info->ndpe_attr_protocol !=
				     -1)) {
					ptrans_protocol =
						(transport_protocol_sub_attr
							 *)((u8 *)pserv_info
								    ->service_spc_info +
							    sizeof(transport_port_sub_attr));
					ptrans_protocol->header.type =
						SERVICE_PROTOCOL_TRANSPORT_PORTOCOL_SUB_ATTR;
					ptrans_protocol->header.length =
						sizeof(u8);
					ptrans_protocol->transport_portocol =
						(cur_if->pnan_info->ndpe_attr_protocol ==
								 1 ?
							 TRANSPORT_PROTOCOL_TCP :
							 TRANSPORT_PROTOCOL_UCP);
					serv_info_tlv_len += sizeof(
						transport_protocol_sub_attr);
				}

				/** service info tlv length */
				pserv_info->header.length =
					serv_info_tlv_len - sizeof(tlv_header);

				mwu_hexdump(MSG_ERROR, "ndp_rsp optional:",
					    ndp_attr->optional,
					    (ETH_ALEN + ipv6_tlv_len +
					     serv_info_tlv_len));
			}
		}

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported) &&
		    (!cur_if->pnan_info->ndpe_attr_negative)) {
			ndp_attr->len = sizeof(nan_ndp_attr) -
					NAN_ATTR_HDR_LEN + ETH_ALEN +
					ipv6_tlv_len + serv_info_tlv_len;
			ndp_var_attr_ptr += sizeof(nan_ndp_attr) + ETH_ALEN +
					    ipv6_tlv_len + serv_info_tlv_len;
			var_sd_attr_len += sizeof(nan_ndp_attr) + ETH_ALEN +
					   ipv6_tlv_len + serv_info_tlv_len;
		} else {
			ndp_attr->len = sizeof(nan_ndp_attr) -
					NAN_ATTR_HDR_LEN + ETH_ALEN;
			ndp_var_attr_ptr += sizeof(nan_ndp_attr) + ETH_ALEN;
			var_sd_attr_len += sizeof(nan_ndp_attr) + ETH_ALEN;
		}
	}

	if ((cur_if->pnan_info->ndp_attr_present) &&
	    (ndpe_attr_included == 1)) // include ndp attribute if not included
				       // and asked by the WFA CAPI
	{
		nan_ndp_attr *ndp_attr;
		NDP_INFO *ndp;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;

		ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_RESPONSE;
		ndp_attr->type_and_status.status = ndp_status;
		ndp_attr->reson_code = reason;
		memcpy(ndp_attr->initiator_ndi,
		       cur_if->pnan_info->ndc_info[0]
			       .ndl_info[0]
			       .ndp_info[0]
			       .initiator_ndi,
		       ETH_ALEN);
		ndp_attr->ndp_id = cur_if->pnan_info->ndc_info[0]
					   .ndl_info[0]
					   .ndp_info[0]
					   .ndp_id; // 0x1
		ndp_attr->ndp_ctrl.confirm_req =
			cur_if->pnan_info->confirm_required;

		ndp_attr->ndp_ctrl.security_present =
			cur_if->pnan_info->security_required;

		ndp_attr->ndp_ctrl.explicit_conf =
			ndp_attr->ndp_ctrl.confirm_req ? TRUE : FALSE;
		ndp_attr->ndp_ctrl.publish_id_present = FALSE;
		ndp_attr->ndp_ctrl.responder_ndi_present = TRUE;
		ndp = &cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0];
		memcpy(&ndp_attr->optional[0], ndp->responder_ndi, ETH_ALEN);

		ndp_attr->len =
			sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN + ETH_ALEN;
		ndp_var_attr_ptr += sizeof(nan_ndp_attr) + ETH_ALEN;
		var_sd_attr_len += sizeof(nan_ndp_attr) + ETH_ALEN;
	}

	/* NAN availability attribute */
	// TODO Nachiket : Availability to be parsed first if came from sigma
	// if (cur_if->pnan_info->avail_attr.len > 0) {
	if (0) {
		nan_availability_attr *availability_attr =
			(nan_availability_attr *)ndp_var_attr_ptr;
		memcpy(availability_attr, cur_if->pnan_info->avail_attr.data,
		       cur_if->pnan_info->avail_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->avail_attr.len;
		var_sd_attr_len += cur_if->pnan_info->avail_attr.len;
	} else {
		nan_availability_attr *availability_attr;
		nan_availability_list *entry;
		nan_channel_entry_list *chan_list;
		u8 opt_fields_len = 7, entry_valid = 0, i = 0, k = 0;
		;
		avail_entry_t *self_entry = NULL;

		availability_attr = (nan_availability_attr *)ndp_var_attr_ptr;
		availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
		availability_attr->attr_ctrl.map_id =
			cur_if->pnan_info->self_avail_info.map_id;
		/*Report Change in committed & potential entry*/
		availability_attr->attr_ctrl.committed_changed =
			cur_if->pnan_info->self_avail_info.committed_changed;
		availability_attr->attr_ctrl.potential_changed =
			cur_if->pnan_info->self_avail_info.potential_changed;
		INFO("==> NDPRESP Map id is %d",
		     availability_attr->attr_ctrl.map_id);
		availability_attr->seq_id =
			++cur_if->pnan_info->self_avail_info.seq_id;

		availability_attr->len =
			sizeof(nan_availability_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_availability_attr);
		var_sd_attr_len += sizeof(nan_availability_attr);

		if (ndl_status == NDP_NDL_STATUS_CONTINUED) {
			self_entry =
				(avail_entry_t *)&cur_if->pnan_info
					->self_avail_info.entry_conditional;
			entry_valid = cur_if->pnan_info->self_avail_info
					      .conditional_valid;
		} else if (ndl_status == NDP_NDL_STATUS_ACCEPTED) {
			self_entry = (avail_entry_t *)&cur_if->pnan_info
					     ->self_avail_info.entry_committed;
			entry_valid = cur_if->pnan_info->self_avail_info
					      .committed_valid;
		}

		for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
			/*Send all the valid conditional/committed entries in
			 * resp*/
			if ((entry_valid & (1 << i))) {
				for (k = 0; k < self_entry[i].time_bitmap_count;
				     k++) {
					INFO("Adding conditional/committed entry#%u",
					     i);
					entry = (nan_availability_list *)
						ndp_var_attr_ptr;
					if (ndl_status ==
					    NDP_NDL_STATUS_CONTINUED) {
						entry->entry_ctrl.avail_type =
							NDP_AVAIL_TYPE_CONDITIONAL;
					} else {
						entry->entry_ctrl.avail_type =
							NDP_AVAIL_TYPE_COMMITTED;
					}

					entry->entry_ctrl.usage_preference =
						0x1;
					entry->entry_ctrl.utilization = 0x0;
					entry->entry_ctrl.rx_nss = 0x1;
					entry->entry_ctrl.time_bitmap_present =
						0x1;
					/*Populate optional attributes*/
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
							self_entry[i].period;
						time_bitmap_ctrl->start_offset =
							self_entry[i]
								.start_offset[k] /
							16;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						memcpy(time_bitmap,
						       &self_entry[i]
								.time_bitmap[k],
						       4); /*time bitmap*/
					}
					INFO("With time_bitmap 0x%x , offset %u , op_class %u and op_chan %u",
					     self_entry[i].time_bitmap[k],
					     self_entry[i].start_offset[k],
					     self_entry[i].op_class,
					     self_entry[i].channels[0]);

					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 0x1;
					chan_list->entry_ctrl.band_type = 0x0;
					chan_list->entry_ctrl.num_entries = 0x1;
					chan_list->chan_band_entry.chan_entry
						.op_class =
						self_entry[i].op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap =
						ndp_get_chan_bitmap(
							self_entry[i].op_class,
							self_entry[i]
								.channels[0]);
					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len - 2;
					availability_attr->len +=
						entry->len + 2;
					ndp_var_attr_ptr += entry->len + 2;
					var_sd_attr_len += entry->len + 2;
				}
			}
		}
	}

	/* NDC attribute */
	if (cur_if->pnan_info->ndc_attr.len > 0) {
		nan_ndc_attr *ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		memcpy(ndc_attr, cur_if->pnan_info->ndc_attr.data,
		       cur_if->pnan_info->ndc_attr.len);
		ndc_attr->sched_ctrl.proposed_ndc = TRUE;
		ndp_var_attr_ptr += cur_if->pnan_info->ndc_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndc_attr.len;
	} else {
		nan_ndc_attr *ndc_attr;
		ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		ndc_attr->attribute_id = NAN_NDC_ATTR;
		memcpy(ndc_attr->ndc_id, cur_if->pnan_info->ndc_info[0].ndc_id,
		       ETH_ALEN);
		ndc_attr->map_id =
			cur_if->pnan_info->self_avail_info.map_id; // TODO Put
								   // map id of
								   // Avail attr
								   // here
		/*
		if(ndl_status == NDP_NDL_STATUS_CONTINUED){
		    ndc_attr->sched_ctrl.proposed_ndc = TRUE;
		}
		else{
		    ndc_attr->sched_ctrl.proposed_ndc = FALSE;
		}
		*/
		ndc_attr->sched_ctrl.proposed_ndc = TRUE;

		if (cur_if->pnan_info->ndc_proposed)
			ndc_attr->sched_ctrl.proposed_ndc = TRUE;
		ndc_attr->time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
		ndc_attr->time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
		ndc_attr->time_bitmap_ctrl.start_offset = 0;
		ndc_attr->time_bitmap_len = 4;
		memcpy(ndc_attr->bitmap, &(cur_if->pnan_info->ndc_info[0].slot),
		       sizeof(u32));

		ndc_attr->len = sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
		var_sd_attr_len += sizeof(nan_ndc_attr) + 4;
	}

	/* NDL attribute */
	if (cur_if->pnan_info->ndl_attr.len > 0) {
		nan_ndl_attr *ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		memcpy(ndl_attr, cur_if->pnan_info->ndl_attr.data,
		       cur_if->pnan_info->ndl_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndl_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndl_attr.len;
	} else {
		nan_ndl_attr *ndl_attr;
		ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		ndl_attr->attribute_id = NAN_NDL_ATTR;
		ndl_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token;
		ndl_attr->type_and_status.type = NDP_NDL_TYPE_RESPONSE;
		ndl_attr->type_and_status.status = ndl_status;
		ndl_attr->reson_code = reason; // gbhat@HC, reserved
		ndl_attr->ndl_ctrl.ndl_peer_id_present = 0x0; // Only NDC
							      // attribute
							      // present bit set
							      // and peer id
							      // present bit set
		ndl_attr->ndl_ctrl.immutable_sched_present =
			0x0; // Only NDC attribute present bit set and peer id
			     // present bit set
		ndl_attr->ndl_ctrl.ndc_attr_present = 0x1; // Only NDC attribute
							   // present bit set
							   // and peer id
							   // present bit set
		ndl_attr->ndl_ctrl.ndl_qos_attr_present = 0x1;

		ndl_attr->len = sizeof(nan_ndl_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndl_attr);
		var_sd_attr_len += sizeof(nan_ndl_attr);
	}

	/*NDL QoS Attribute*/
	{
		nan_ndl_qos_attr *ndl_qos_attr =
			(nan_ndl_qos_attr *)ndp_var_attr_ptr;
		peer_availability_info *self_info =
			&cur_if->pnan_info->self_avail_info;
		ndl_qos_attr->attribute_id = NAN_NDL_QOS_ATTR;
		ndl_qos_attr->min_slots = self_info->qos.min_slots;
		memcpy(ndl_qos_attr->max_latency, &self_info->qos.max_latency,
		       sizeof(ndl_qos_attr->max_latency));
		ndl_qos_attr->len = sizeof(nan_ndl_qos_attr) - NAN_ATTR_HDR_LEN;

		ndp_var_attr_ptr += sizeof(nan_ndl_qos_attr);
		var_sd_attr_len += sizeof(nan_ndl_qos_attr);
	}
	/* container attribute */
	{
		u8 supported_rates[6] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
		u8 ext_supported_rates[10] = {0x32, 0x08, 0x0c, 0x12, 0x18,
					      0x24, 0x30, 0x48, 0x60, 0x6c};
		u8 htcap[28] = {0x2d, 0x1a, 0xee, 0x01, 0x1f, 0xff, 0xff,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		u8 htinfo[24] = {0x3d, 0x16, 0x06, 0x05, 0x15, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		u8 *elem_ptr;
		u16 elem_len = 0;

		nan_element_container_attr *element_container_attr;
		element_container_attr =
			(nan_element_container_attr *)ndp_var_attr_ptr;
		elem_ptr = (u8 *)(&element_container_attr->elements[0]);
		element_container_attr->attribute_id =
			NAN_ELEMENT_CONTAINER_ATTR;
		element_container_attr->len =
			sizeof(supported_rates) + sizeof(ext_supported_rates) +
			sizeof(htcap) + sizeof(htinfo) + 1;

		memcpy(elem_ptr, supported_rates, sizeof(supported_rates));
		// mwu_hexdump(MSG_INFO, "elem", elem_ptr,
		// sizeof(supported_rates)); mwu_hexdump(MSG_INFO, "suppor
		// rates", supported_rates, sizeof(supported_rates));
		elem_ptr += sizeof(supported_rates);
		elem_len += sizeof(supported_rates);

		memcpy(elem_ptr, ext_supported_rates,
		       sizeof(ext_supported_rates));
		elem_ptr += sizeof(ext_supported_rates);
		elem_len += sizeof(ext_supported_rates);

		memcpy(elem_ptr, htcap, sizeof(htcap));
		elem_ptr += sizeof(htcap);
		elem_len += sizeof(htcap);

		memcpy(elem_ptr, htinfo, sizeof(htinfo));
		elem_ptr += sizeof(htinfo);
		elem_len += sizeof(htinfo);

		ndp_var_attr_ptr = ndp_var_attr_ptr +
				   sizeof(nan_element_container_attr) +
				   elem_len;
		var_sd_attr_len = var_sd_attr_len +
				  sizeof(nan_element_container_attr) + elem_len;
	}

	/*Security attributes*/
	if (cur_if->pnan_info->security_required) {
		{
			/*Cipher suite info attr*/
			nan_cipher_suite_info_attr *csinfo =
				(nan_cipher_suite_info_attr *)ndp_var_attr_ptr;
			csinfo->attribute_id = NAN_CIPHER_SUITE_INFO_ATTR;
			csinfo->capabilities = 0;
			csinfo->attr_info[0].cipher_suite_id = NCS_SK_CCM_128;
			csinfo->attr_info[0].instance_id =
				(u8)cur_if->pnan_info->instance_id; /*All
								       services*/

			csinfo->len = sizeof(nan_cipher_suite_info_attr) +
				      sizeof(nan_cipher_suite_attr_info) -
				      NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_cipher_suite_info_attr) +
					    sizeof(nan_cipher_suite_attr_info);
			var_sd_attr_len += sizeof(nan_cipher_suite_info_attr) +
					   sizeof(nan_cipher_suite_attr_info);
		}
		{
			/*Security context info attr*/
			nan_security_context_info_attr *sec_ctx_info =
				(nan_security_context_info_attr *)
					ndp_var_attr_ptr;

			sec_ctx_info->attribute_id = NAN_SEC_CONTEXT_INFO_ATTR;
			sec_ctx_info->identifier_list[0].identifier_len =
				PMKID_LEN;
			sec_ctx_info->identifier_list[0].identifier_type =
				1 /*PMKID*/;
			sec_ctx_info->identifier_list[0].instance_id =
				(u8)cur_if->pnan_info->instance_id; /*All
								       services*/
			memcpy(&sec_ctx_info->identifier_list[0]
					.sec_context_identifier,
			       cur_if->pnan_info->nan_security.nan_pmkid,
			       PMKID_LEN); /*Copy PMKID*/

			sec_ctx_info->len =
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) +
				PMKID_LEN - NAN_ATTR_HDR_LEN;

			ndp_var_attr_ptr +=
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) +
				PMKID_LEN;
			var_sd_attr_len +=
				sizeof(nan_security_context_info_attr) +
				sizeof(nan_security_context_attr_field) +
				PMKID_LEN;
		}
		/*Shared key desc attr*/
		{
			nan_shared_key_desc_attr *temp_shared_key_desc;

			temp_shared_key_desc =
				(nan_shared_key_desc_attr *)nan_get_nan_attr(
					NAN_SHARED_KEY_DESC_ATTR,
					cur_if->pnan_info->nan_security.m1->buf,
					cur_if->pnan_info->nan_security.m1
						->size);

			/* If M1 does not contain valid
			 * NAN_SHARED_KEY_DESC_ATTR, do not send Shared key desc
			 * attr for M2*/
			if (temp_shared_key_desc) {
				/* >>>> TBD <<<<<< Put Key desc for M2 */

				nan_shared_key_desc_attr *key_desc =
					(nan_shared_key_desc_attr *)
						ndp_var_attr_ptr;
				u8 sz_key_desc;
				sz_key_desc = sizeof(eapol_key_frame);
				eapol_key_frame *m2_eapol;
				eapol_key_frame *rcvd_m1_eapol;
				u8 *m2_body;

				key_desc->attribute_id =
					NAN_SHARED_KEY_DESC_ATTR;
				key_desc->instance_id =
					(u8)cur_if->pnan_info
						->instance_id; /*all instances*/

				nan_alloc_key_desc_buf(&m2_eapol);

				rcvd_m1_eapol =
					(eapol_key_frame *)temp_shared_key_desc
						->rsna_key_desc;

				m2_body = (u8 *)sdf_buf;
				key_desc->len =
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc - NAN_ATTR_HDR_LEN;
				{
					memcpy(m2_eapol->key_nonce,
					       cur_if->pnan_info->nan_security
						       .local_key_nonce,
					       NONCE_LEN);
					nan_generate_key_desc_m2(
						m2_eapol,
						(eapol_key_frame *)&key_desc
							->rsna_key_desc,
						rcvd_m1_eapol,
						m2_body + 11, /* mac addr + ttl
								 +  tx_type*/
						var_sd_attr_len +
							sizeof(nan_shared_key_desc_attr) +
							sz_key_desc +
							7, /*Fixed params in
							      frame*/
						(cur_if->pnan_info->nan_security
							 .ptk_buf.kck));

					memcpy(&key_desc->rsna_key_desc,
					       m2_eapol, sz_key_desc);
					mwu_hexdump(
						MSG_INFO,
						"NAN2 DBG M2 Body with MIC=valid value\n",
						m2_body + 11,
						var_sd_attr_len +
							sizeof(nan_shared_key_desc_attr) +
							sz_key_desc + 7);
				}
				nan_free_key_desc_buf(&m2_eapol);

				ndp_var_attr_ptr +=
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc;
				var_sd_attr_len +=
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc;
			} else {
				ERR("M1 does not contain NAN_SHARED_KEY_DESC_ATTR attribute\n");
			}
		}
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out NDP response");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	/*Generate NDP request*/
	/*Tx NDP response*/
	/*Change state*/
	return ret;
}

enum nan_error nan_send_ndp_terminate(struct mwu_iface_info *cur_if)
{
	/*Process NDP request*/
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr,
	       cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = NDP_TERMINATE;
	/* NDP attribute*/
	{
		nan_ndp_attr *ndp_attr;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;
		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported))
			ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
		else
			ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token =
			cur_if->pnan_info->dialog_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_TERMINATE;
		ndp_attr->type_and_status.status = NDP_NDL_STATUS_RESERVED;
		ndp_attr->reson_code = NAN_REASON_CODE_DEFAULT;
		memcpy(ndp_attr->initiator_ndi, cur_if->device_mac_addr,
		       ETH_ALEN);
		ndp_attr->ndp_id = cur_if->pnan_info->ndc_info[0]
					   .ndl_info[0]
					   .ndp_info[0]
					   .ndp_id;
		ndp_attr->ndp_ctrl.confirm_req = FALSE;
		ndp_attr->ndp_ctrl.explicit_conf = FALSE;
		ndp_attr->ndp_ctrl.security_present =
			cur_if->pnan_info->security_required;
		;
		ndp_attr->ndp_ctrl.publish_id_present = FALSE;
		ndp_attr->ndp_ctrl.responder_ndi_present = FALSE;

		ndp_attr->len = sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndp_attr);
		var_sd_attr_len += sizeof(nan_ndp_attr);
	}
	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out NDP terminate");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	return ret;
}

enum nan_error nan_handle_schedule_confirm(struct mwu_iface_info *cur_if,
					   unsigned char *buffer, int size)
{
	/* Maintain a copy of ndp response buffer for later use.
	   Free this buffer later*/
	cur_if->pnan_info->rx_ndp_conf =
		(struct nan_generic_buf *)malloc(size + NAN_BUF_HDR_SIZE);
	memcpy(cur_if->pnan_info->rx_ndp_conf->buf, buffer, size);
	cur_if->pnan_info->rx_ndp_conf->size = size + NAN_BUF_HDR_SIZE;

	nan_parse_schedule_confirm(cur_if);

	nancmd_set_final_bitmap(cur_if, NAN_NDL);

	nan_iface_open();

	/*cur_if->pnan_info->self_avail_info.seq_id++;

	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap
	    &=
	~(cur_if->pnan_info->self_avail_info.entry_committed[0].time_bitmap);*/

	FREE(cur_if->pnan_info->rx_ndp_conf);
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_send_schedule_confirm(struct mwu_iface_info *cur_if,
					 nan_ndl_attr *resp_ndl_attr,
					 u32 ndl_status)
{
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr,
	       cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = SCHEDULE_CONFIRM;

	/* NAN availability attribute */
	{
		nan_availability_attr *availability_attr;
		nan_availability_list *entry;
		nan_channel_entry_list *chan_list;
		u8 opt_fields_len = 7, i = 0, k = 0;

		availability_attr = (nan_availability_attr *)ndp_var_attr_ptr;
		availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
		availability_attr->attr_ctrl.map_id =
			cur_if->pnan_info->self_avail_info.map_id;
		availability_attr->seq_id =
			cur_if->pnan_info->self_avail_info.seq_id;

		availability_attr->len =
			sizeof(nan_availability_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_availability_attr);
		var_sd_attr_len += sizeof(nan_availability_attr);

		for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
			/*Send all the valid committed entries in proposal*/
			if ((cur_if->pnan_info->self_avail_info.committed_valid &
			     (1 << i))) {
				for (k = 0;
				     k < cur_if->pnan_info->self_avail_info
						 .entry_committed[i]
						 .time_bitmap_count;
				     k++) {
					INFO("Adding committed entry#%u", i);
					entry = (nan_availability_list *)
						ndp_var_attr_ptr;
					if (ndl_status ==
					    NDP_NDL_STATUS_CONTINUED) {
						entry->entry_ctrl.avail_type =
							NDP_AVAIL_TYPE_CONDITIONAL;
					} else {
						entry->entry_ctrl.avail_type =
							NDP_AVAIL_TYPE_COMMITTED;
					}
					entry->entry_ctrl.usage_preference =
						0x1;
					entry->entry_ctrl.utilization = 0x0;
					entry->entry_ctrl.rx_nss = 0x1;
					entry->entry_ctrl.time_bitmap_present =
						0x1;
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
							cur_if->pnan_info
								->self_avail_info
								.entry_committed
									[i]
								.period;
						time_bitmap_ctrl->start_offset =
							cur_if->pnan_info
								->self_avail_info
								.entry_committed
									[i]
								.start_offset[k] /
							16;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						memcpy(time_bitmap,
						       &cur_if->pnan_info
								->self_avail_info
								.entry_committed
									[i]
								.time_bitmap[k],
						       4); /*time bitmap*/
					}
					INFO("With time_bitmap 0x%x , offset %u , op_class %u and op_chan %u",
					     cur_if->pnan_info->self_avail_info
						     .entry_committed[i]
						     .time_bitmap[k],
					     cur_if->pnan_info->self_avail_info
						     .entry_committed[i]
						     .start_offset[k],
					     cur_if->pnan_info->self_avail_info
						     .entry_committed[i]
						     .op_class,
					     cur_if->pnan_info->self_avail_info
						     .entry_committed[i]
						     .channels[0]);

					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 0x1;
					chan_list->entry_ctrl.band_type = 0x0;
					chan_list->entry_ctrl.num_entries = 0x1;

					chan_list->chan_band_entry.chan_entry
						.op_class =
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[i]
							.op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap = ndp_get_chan_bitmap(
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[i]
							.op_class,
						cur_if->pnan_info
							->self_avail_info
							.entry_committed[i]
							.channels[0]);

					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len - 2;
					availability_attr->len +=
						entry->len + 2;
					ndp_var_attr_ptr += entry->len + 2;
					var_sd_attr_len += entry->len + 2;
				}
			}
		}
	}

	/* NDC attribute */
	if (cur_if->pnan_info->ndc_attr.len > 0) {
		nan_ndc_attr *ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		memcpy(ndc_attr, cur_if->pnan_info->ndc_attr.data,
		       cur_if->pnan_info->ndc_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndc_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndc_attr.len;
	} else {
		nan_ndc_attr *ndc_attr;
		u8 wfa_ndc_id[6] = {0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00};
		ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
		ndc_attr->attribute_id = NAN_NDC_ATTR;
		memcpy(ndc_attr->ndc_id, wfa_ndc_id, ETH_ALEN);
		ndc_attr->map_id =
			cur_if->pnan_info->self_avail_info.map_id; // TODO Put
								   // map id of
								   // Avail attr
								   // here
		ndc_attr->sched_ctrl.proposed_ndc = TRUE;
		ndc_attr->time_bitmap_ctrl.bit_duration = NDP_TIME_BM_DUR_16;
		ndc_attr->time_bitmap_ctrl.bit_period = NDP_TIME_BM_PERIOD_512;
		ndc_attr->time_bitmap_ctrl.start_offset = 0;
		ndc_attr->time_bitmap_len = 4;
		memcpy(ndc_attr->bitmap, &(cur_if->pnan_info->ndc_info[0].slot),
		       sizeof(u32));

		ndc_attr->len = sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
		var_sd_attr_len += sizeof(nan_ndc_attr) + 4;
	}

	/* NDL attribute */
	if (cur_if->pnan_info->ndl_attr.len > 0) {
		nan_ndl_attr *ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		memcpy(ndl_attr, cur_if->pnan_info->ndl_attr.data,
		       cur_if->pnan_info->ndl_attr.len);
		ndp_var_attr_ptr += cur_if->pnan_info->ndl_attr.len;
		var_sd_attr_len += cur_if->pnan_info->ndl_attr.len;
	} else {
		nan_ndl_attr *ndl_attr;
		ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
		ndl_attr->attribute_id = NAN_NDL_ATTR;
		ndl_attr->dialogue_token =
			cur_if->pnan_info->ndc_info[0]
				.ndl_info[0]
				.ndp_info[0]
				.dialogue_token; // gbhat@HC: as per spec, set
						 // to non-zero value
		ndl_attr->type_and_status.type = NDP_NDL_TYPE_CONFIRM;
		ndl_attr->type_and_status.status = ndl_status;
		ndl_attr->reson_code = NDP_RESERVED; // gbhat@HC, reserved
		ndl_attr->ndl_ctrl.ndl_peer_id_present = 0x0; // Only NDC
							      // attribute
							      // present bit set
							      // and peer id
							      // present bit set
		ndl_attr->ndl_ctrl.immutable_sched_present =
			0x0; // Only NDC attribute present bit set and peer id
			     // present bit set
		ndl_attr->ndl_ctrl.ndc_attr_present = 0x1; // Only NDC attribute
							   // present bit set
							   // and peer id
							   // present bit set
		ndl_attr->ndl_ctrl.ndl_qos_attr_present =
			0x0; // Only NDC attribute present bit set and peer id
			     // present bit set

		ndl_attr->len = sizeof(nan_ndl_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndl_attr);
		var_sd_attr_len += sizeof(nan_ndl_attr);
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out schedule Confirm");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	return ret;
}

enum nan_error nan_handle_schedule_resp(struct mwu_iface_info *cur_if,
					unsigned char *buffer, int size)
{
	nan_ndl_attr *ndl_attr = NULL;
	nan_ndc_attr *ndc_attr = NULL;
	u8 i = 0;
	enum nan_error ret = NAN_ERR_SUCCESS;

	cur_if->pnan_info->self_avail_info.committed_valid = 0;
	cur_if->pnan_info->peer_avail_info.committed_valid = 0;
	cur_if->pnan_info->self_avail_info.conditional_valid = 0;
	cur_if->pnan_info->peer_avail_info.conditional_valid = 0;

	/* Maintain a copy of ndp response buffer for later use.
	   Free this buffer later */
	cur_if->pnan_info->rx_ndp_resp =
		(struct nan_generic_buf *)malloc(size + NAN_BUF_HDR_SIZE);
	memcpy(cur_if->pnan_info->rx_ndp_resp->buf, buffer, size);
	cur_if->pnan_info->rx_ndp_resp->size = size + NAN_BUF_HDR_SIZE;

	ndl_attr = (nan_ndl_attr *)nan_get_nan_attr(NAN_NDL_ATTR, buffer, size);
	ndc_attr = (nan_ndc_attr *)nan_get_nan_attr(NAN_NDC_ATTR, buffer, size);

	/*extract NDC bitmap if proposed*/
	if (ndc_attr && ndc_attr->sched_ctrl.proposed_ndc) {
		NDC_INFO *ndc = NULL;
		u32 ndc_time_bitmap[MAX_SCHEDULE_ENTRIES] = {0},
		    combined_ndc_bitmap = 0;
		memset(ndc_time_bitmap, 0,
		       (sizeof(u32) * MAX_SCHEDULE_ENTRIES));
		u8 ndc_time_bitmap_count = nan_parse_generic_time_bitmap(
			&ndc_attr->time_bitmap_ctrl, ndc_attr->time_bitmap_len,
			ndc_attr->bitmap, ndc_time_bitmap);
		for (i = 0; i < ndc_time_bitmap_count; i++) {
			combined_ndc_bitmap |= ndc_time_bitmap[i];
		}

		ERR("NAN2 : Received proposed NDC with slots - %x",
		    combined_ndc_bitmap);
		ndc = &cur_if->pnan_info->ndc_info[0];
		ndc->slot = combined_ndc_bitmap;
	}

	/*Take further actions based on NDL status*/
	{
		u8 tx_ndl_status = NDP_NDL_STATUS_ACCEPTED;
		if (ndl_attr && ndl_attr->type_and_status.status ==
					NDP_NDL_STATUS_ACCEPTED) {
			/*Parse recvd ndp resp*/
			nan_parse_schedule_resp(cur_if);

			/*Send the bitmap to firmware*/
			nancmd_set_final_bitmap(cur_if, NAN_NDL);

			/* */
			nan_send_bcast_schedule_update(cur_if);

			/*Open nan interface*/
			nan_iface_open();
		}

		if (ndl_attr && ndl_attr->type_and_status.status ==
					NDP_NDL_STATUS_CONTINUED) {
			/*This is a case of counter proposal*/
			nan_parse_schedule_resp(cur_if);

			/*Generate and send NDP confirm*/
			ret = nan_send_schedule_confirm(cur_if, ndl_attr,
							tx_ndl_status);
			if (ret != NAN_ERR_SUCCESS) {
				ERR("Failed to send schedule_confirm");
				return ret;
			}

			/*Send the bitmap to firmware*/
			nancmd_set_final_bitmap(cur_if, NAN_NDL);

			/*Open nan interface*/
			nan_iface_open();
		}
	}
	FREE(cur_if->pnan_info->rx_ndp_resp);
	return NAN_ERR_SUCCESS;
}

enum nan_error nan_send_ndp_confirm(struct mwu_iface_info *cur_if,
				    nan_ndl_attr *resp_ndl_attr,
				    nan_ndp_attr *resp_ndp_attr, u32 ndl_status,
				    u32 ndp_status)
{
	/*Process NDP request*/
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr,
	       cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	sdf_buf->subtype = NDP_CONFIRM;

	if (resp_ndl_attr->type_and_status.status == NDP_NDL_STATUS_CONTINUED) {
		/* device capability */
		{
			nan_device_capability_attr *device_capa_attr;
			device_capa_attr =
				(nan_device_capability_attr *)ndp_var_attr_ptr;
			device_capa_attr->attribute_id = NAN_DEVICE_CAPA_ATTR;
			device_capa_attr->len =
				sizeof(nan_device_capability_attr) -
				NAN_ATTR_HDR_LEN;
			device_capa_attr->committed_dw_info._2g_dw =
				cur_if->pnan_info
					->awake_dw_interval; // gbhat@HC:available
							     // on all slots
							     // on 2.4GHz
			if (cur_if->pnan_info->a_band)
				device_capa_attr->committed_dw_info._5g_dw = 1;
			device_capa_attr->supported_bands =
				cur_if->pnan_info->a_band ? 0x14 :
							    0x04; // gbhat@HC:only
								  // 2.4GHz
			if ((cur_if->pnan_info->ndpe_attr_supported) &&
			    (cur_if->pnan_info->peer_avail_info_published
				     .ndpe_attr_supported))
				device_capa_attr->capabilities
					.ndpe_attr_supported = TRUE;

			if (cur_if->pnan_info->ndpe_attr_negative)
				device_capa_attr->capabilities
					.ndpe_attr_supported = TRUE;
			// Keep below fields zero
			// device_capa_attr->operation_mode = 0x00;
			// device_capa_attr->no_of_antennas = 0x0;
			device_capa_attr->max_chan_sw_time = 5000;

			ndp_var_attr_ptr += sizeof(nan_device_capability_attr);
			var_sd_attr_len += sizeof(nan_device_capability_attr);
		}

		/* NAN availability attribute */
		{
			nan_availability_attr *availability_attr;
			nan_availability_list *entry;
			nan_channel_entry_list *chan_list;
			u8 opt_fields_len = 7, i = 0, k = 0;

			availability_attr =
				(nan_availability_attr *)ndp_var_attr_ptr;
			availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
			availability_attr->attr_ctrl.map_id =
				cur_if->pnan_info->self_avail_info.map_id;
			availability_attr->attr_ctrl.committed_changed =
				cur_if->pnan_info->self_avail_info
					.committed_changed;
			availability_attr->attr_ctrl.potential_changed =
				cur_if->pnan_info->self_avail_info
					.potential_changed;
			availability_attr->seq_id =
				cur_if->pnan_info->self_avail_info.seq_id;

			availability_attr->len = sizeof(nan_availability_attr) -
						 NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_availability_attr);
			var_sd_attr_len += sizeof(nan_availability_attr);

			for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
				/*Send all the valid committed entries*/
				if ((cur_if->pnan_info->self_avail_info
					     .committed_valid &
				     (1 << i))) {
					for (k = 0;
					     k <
					     cur_if->pnan_info->self_avail_info
						     .entry_committed[i]
						     .time_bitmap_count;
					     k++) {
						INFO("Adding committed entry#%u",
						     i);
						entry = (nan_availability_list *)
							ndp_var_attr_ptr;
						if (ndl_status ==
						    NDP_NDL_STATUS_CONTINUED) {
							entry->entry_ctrl
								.avail_type =
								NDP_AVAIL_TYPE_CONDITIONAL;
						} else {
							entry->entry_ctrl
								.avail_type =
								NDP_AVAIL_TYPE_COMMITTED;
						}
						entry->entry_ctrl
							.usage_preference = 0x1;
						entry->entry_ctrl.utilization =
							0x0;
						entry->entry_ctrl.rx_nss = 0x1;
						entry->entry_ctrl
							.time_bitmap_present =
							0x1;
						/*Polulate optional attributes*/
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
						INFO("With time_bitmap 0x%x , offset %u , op_class %u and op_chan %u",
						     cur_if->pnan_info
							     ->self_avail_info
							     .entry_committed[i]
							     .time_bitmap[k],
						     cur_if->pnan_info
							     ->self_avail_info
							     .entry_committed[i]
							     .start_offset[k],
						     cur_if->pnan_info
							     ->self_avail_info
							     .entry_committed[i]
							     .op_class,
						     cur_if->pnan_info
							     ->self_avail_info
							     .entry_committed[i]
							     .channels[0]);

						chan_list =
							(nan_channel_entry_list
								 *)(ndp_var_attr_ptr +
								    sizeof(nan_availability_list) +
								    opt_fields_len);
						chan_list->entry_ctrl
							.entry_type = 0x1;
						chan_list->entry_ctrl.band_type =
							0x0;
						chan_list->entry_ctrl
							.num_entries = 0x1;

						chan_list->chan_band_entry
							.chan_entry.op_class =
							cur_if->pnan_info
								->self_avail_info
								.entry_committed
									[i]
								.op_class;
						chan_list->chan_band_entry
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
						chan_list->chan_band_entry
							.chan_entry
							.primary_chan_bitmap =
							0x0;
						// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
						// = 0x0;

						entry->len =
							sizeof(nan_channel_entry_list) +
							sizeof(nan_availability_list) +
							opt_fields_len - 2;
						availability_attr->len +=
							entry->len + 2;
						ndp_var_attr_ptr +=
							entry->len + 2;
						var_sd_attr_len +=
							entry->len + 2;
					}
				}
			}
		}

		/* NDC attribute */
		if (cur_if->pnan_info->ndc_attr.len > 0) {
			nan_ndc_attr *ndc_attr =
				(nan_ndc_attr *)ndp_var_attr_ptr;
			memcpy(ndc_attr, cur_if->pnan_info->ndc_attr.data,
			       cur_if->pnan_info->ndc_attr.len);
			ndp_var_attr_ptr += cur_if->pnan_info->ndc_attr.len;
			var_sd_attr_len += cur_if->pnan_info->ndc_attr.len;
		} else {
			nan_ndc_attr *ndc_attr;
			u8 wfa_ndc_id[6] = {0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00};
			ndc_attr = (nan_ndc_attr *)ndp_var_attr_ptr;
			ndc_attr->attribute_id = NAN_NDC_ATTR;
			memcpy(ndc_attr->ndc_id, wfa_ndc_id, ETH_ALEN);
			ndc_attr->map_id = cur_if->pnan_info->self_avail_info
						   .map_id; // TODO Put map id
							    // of Avail attr
							    // here
			ndc_attr->sched_ctrl.proposed_ndc = TRUE;
			ndc_attr->time_bitmap_ctrl.bit_duration =
				NDP_TIME_BM_DUR_16;
			ndc_attr->time_bitmap_ctrl.bit_period =
				NDP_TIME_BM_PERIOD_512;
			ndc_attr->time_bitmap_ctrl.start_offset = 0;
			ndc_attr->time_bitmap_len = 4;
			memcpy(ndc_attr->bitmap,
			       &(cur_if->pnan_info->ndc_info[0].slot),
			       sizeof(u32));

			ndc_attr->len =
				sizeof(nan_ndc_attr) + 4 - NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_ndc_attr) + 4;
			var_sd_attr_len += sizeof(nan_ndc_attr) + 4;
		}

		/* NDL attribute */
		if (cur_if->pnan_info->ndl_attr.len > 0) {
			nan_ndl_attr *ndl_attr =
				(nan_ndl_attr *)ndp_var_attr_ptr;
			memcpy(ndl_attr, cur_if->pnan_info->ndl_attr.data,
			       cur_if->pnan_info->ndl_attr.len);
			ndp_var_attr_ptr += cur_if->pnan_info->ndl_attr.len;
			var_sd_attr_len += cur_if->pnan_info->ndl_attr.len;
		} else {
			nan_ndl_attr *ndl_attr;
			ndl_attr = (nan_ndl_attr *)ndp_var_attr_ptr;
			ndl_attr->attribute_id = NAN_NDL_ATTR;
			ndl_attr->dialogue_token =
				cur_if->pnan_info->ndc_info[0]
					.ndl_info[0]
					.ndp_info[0]
					.dialogue_token; // gbhat@HC: as per
							 // spec, set to
							 // non-zero value
			ndl_attr->type_and_status.type = NDP_NDL_TYPE_CONFIRM;
			ndl_attr->type_and_status.status = ndl_status;
			ndl_attr->reson_code = NDP_RESERVED; // gbhat@HC,
							     // reserved
			ndl_attr->ndl_ctrl.ndl_peer_id_present =
				0x0; // Only NDC attribute present bit set and
				     // peer id present bit set
			ndl_attr->ndl_ctrl.immutable_sched_present =
				0x0; // Only NDC attribute present bit set and
				     // peer id present bit set
			ndl_attr->ndl_ctrl.ndc_attr_present =
				0x1; // Only NDC attribute present bit set and
				     // peer id present bit set
			ndl_attr->ndl_ctrl.ndl_qos_attr_present =
				0x0; // Only NDC attribute present bit set and
				     // peer id present bit set

			ndl_attr->len = sizeof(nan_ndl_attr) - NAN_ATTR_HDR_LEN;
			ndp_var_attr_ptr += sizeof(nan_ndl_attr);
			var_sd_attr_len += sizeof(nan_ndl_attr);
		}
	}
	if (resp_ndp_attr->type_and_status.status == NDP_NDL_STATUS_CONTINUED) {
		/*Populate NDP attribute*/
		nan_ndp_attr *ndp_attr;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;
		if (cur_if->pnan_info->security_required)
			/*If security required then status should always be
			 * continued*/
			ndp_status = NDP_NDL_STATUS_CONTINUED;
		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported))
			ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
		else
			ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_CONFIRM;
		ndp_attr->type_and_status.status = ndp_status;
		ndp_attr->reson_code = NAN_REASON_CODE_DEFAULT;
		memcpy(ndp_attr->initiator_ndi, cur_if->device_mac_addr,
		       ETH_ALEN);
		ndp_attr->ndp_id = cur_if->pnan_info->ndc_info[0]
					   .ndl_info[0]
					   .ndp_info[0]
					   .ndp_id;
		ndp_attr->ndp_ctrl.confirm_req = FALSE;
		ndp_attr->ndp_ctrl.explicit_conf = FALSE;
		ndp_attr->ndp_ctrl.security_present =
			cur_if->pnan_info->security_required;
		ndp_attr->ndp_ctrl.publish_id_present = FALSE;
		ndp_attr->ndp_ctrl.responder_ndi_present = FALSE;

		ndp_attr->len = sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndp_attr);
		var_sd_attr_len += sizeof(nan_ndp_attr);
	} else if (resp_ndp_attr->type_and_status.status ==
		   NDP_NDL_STATUS_REJECTED) {
		/*Populate NDP attribute*/
		nan_ndp_attr *ndp_attr;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;
#if 0
        if(cur_if->pnan_info->security_required)
        /*If security required then status should always be continued*/
            ndp_status = NDP_NDL_STATUS_CONTINUED;
#endif

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported))
			ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
		else
			ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_CONFIRM;
		ndp_attr->type_and_status.status = NDP_NDL_STATUS_REJECTED;
		ndp_attr->reson_code = NAN_REASON_CODE_NDP_REJECTED;
		memcpy(ndp_attr->initiator_ndi, cur_if->device_mac_addr,
		       ETH_ALEN);
		ndp_attr->ndp_id = cur_if->pnan_info->ndc_info[0]
					   .ndl_info[0]
					   .ndp_info[0]
					   .ndp_id;
		ndp_attr->ndp_ctrl.confirm_req = FALSE;
		ndp_attr->ndp_ctrl.explicit_conf = FALSE;
		ndp_attr->ndp_ctrl.security_present =
			cur_if->pnan_info->security_required;
		ndp_attr->ndp_ctrl.publish_id_present = FALSE;
		ndp_attr->ndp_ctrl.responder_ndi_present = FALSE;

		ndp_attr->len = sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndp_attr);
		var_sd_attr_len += sizeof(nan_ndp_attr);
	}

	/*NDL QoS Attribute*/
	{
		nan_ndl_qos_attr *ndl_qos_attr =
			(nan_ndl_qos_attr *)ndp_var_attr_ptr;
		ndl_qos_attr->attribute_id = NAN_NDL_QOS_ATTR;
		ndl_qos_attr->min_slots =
			cur_if->pnan_info->peer_avail_info.qos.min_slots;
		memcpy(ndl_qos_attr->max_latency,
		       &cur_if->pnan_info->peer_avail_info.qos.max_latency,
		       sizeof(ndl_qos_attr->max_latency));
		ndl_qos_attr->len = sizeof(nan_ndl_qos_attr) - NAN_ATTR_HDR_LEN;

		ndp_var_attr_ptr += sizeof(nan_ndl_qos_attr);
		var_sd_attr_len += sizeof(nan_ndl_qos_attr);
	}

	/*Security attributes*/
	if (cur_if->pnan_info->security_required) {
		/* If M2 does not contain valid NAN_SHARED_KEY_DESC_ATTR, do not
		 * send Shared key desc attr for M1*/

		/*Shared key desc attr*/
		{
			/* >>>> TBD <<<<<< Put Key desc for M3 */

			nan_shared_key_desc_attr *key_desc =
				(nan_shared_key_desc_attr *)ndp_var_attr_ptr;
			u8 sz_key_desc = sizeof(eapol_key_frame);
			key_desc->attribute_id = NAN_SHARED_KEY_DESC_ATTR;
			key_desc->instance_id =
				cur_if->pnan_info->instance_id; /*all
								   instances*/

			eapol_key_frame *m3_eapol;
			u8 *m3_body;

			nan_alloc_key_desc_buf(&m3_eapol);

			m3_body = (u8 *)sdf_buf;
			/*
			 * M3 should contain same nonce value as that of M1*/
			memcpy(m3_eapol->key_nonce,
			       cur_if->pnan_info->nan_security.local_key_nonce,
			       NONCE_LEN);

			key_desc->len = sizeof(nan_shared_key_desc_attr) +
					sz_key_desc - NAN_ATTR_HDR_LEN;
			nan_generate_key_desc_m3(
				m3_eapol,
				(eapol_key_frame *)&key_desc->rsna_key_desc,
				&cur_if->pnan_info->nan_security.m1->buf[11],
				cur_if->pnan_info->nan_security.m1->size - 11,
				m3_body + 11, /* mac addr + ttl +  tx_type*/
				var_sd_attr_len +
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc + 7, /*Fixed params in
							    frame*/
				(cur_if->pnan_info->nan_security.ptk_buf.kck));

			INFO("M1 body length : %d \n",
			     cur_if->pnan_info->nan_security.m1->size);
			INFO("M3 body length : %lu \n",
			     var_sd_attr_len +
				     sizeof(nan_shared_key_desc_attr) +
				     sz_key_desc + 7);
			memcpy(&key_desc->rsna_key_desc, m3_eapol, sz_key_desc);

			// nan_free_key_desc_buf(&m3_eapol);
			ndp_var_attr_ptr +=
				sizeof(nan_shared_key_desc_attr) + sz_key_desc;
			var_sd_attr_len +=
				sizeof(nan_shared_key_desc_attr) + sz_key_desc;
		}
		free(cur_if->pnan_info->nan_security.m1);
		free(cur_if->pnan_info->nan_security.m2);
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out NDP Confirm");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	/*Generate NDP request*/
	/*Tx NDP response*/
	/*Change state*/
	return ret;
}
enum nan_error nan_handle_security_install(struct mwu_iface_info *cur_if,
					   unsigned char *buffer, int size)
{
	int ret = NAN_ERR_SUCCESS;

	nan_ndp_attr *ndp_attr = NULL;

	if ((cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_EXT_ATTR,
							    buffer, size);
	else
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_ATTR,
							    buffer, size);

	if (ndp_attr->type_and_status.status == NDP_NDL_STATUS_REJECTED) {
		INFO("NAN2: reject received in Security install");
		/*Change state to IDLE*/
		change_ndp_state(cur_if, NDP_IDLE);

		free(cur_if->pnan_info->nan_security.m3);
		return NAN_ERR_INVAL;
	}

	if (cur_if->pnan_info->security_required) {
		nan_shared_key_desc_attr *temp_shared_key_desc;
		eapol_key_frame *rcvd_m4_eapol;

		cur_if->pnan_info->nan_security.m4 =
			(struct nan_sec_buf *)malloc(size + NAN_BUF_HDR_SIZE);
		memcpy(cur_if->pnan_info->nan_security.m4->buf, buffer, size);
		cur_if->pnan_info->nan_security.m4->size =
			nan_calculate_ndp_frame_size(buffer, size) +
			7 /*Category ,Action, OUI, subtype, NAF type*/
			+ NAN_BUF_HDR_SIZE;
		ERR("DBG RCVD M2 SIZE %lu",
		    cur_if->pnan_info->nan_security.m4->size -
			    NAN_BUF_HDR_SIZE);

		temp_shared_key_desc =
			(nan_shared_key_desc_attr *)nan_get_nan_attr(
				NAN_SHARED_KEY_DESC_ATTR,
				cur_if->pnan_info->nan_security.m4->buf,
				cur_if->pnan_info->nan_security.m4->size);

		/* Check if NAN_SHARED_KEY_DESC_ATTR is present in recvd M4; */
		if (temp_shared_key_desc) {
			rcvd_m4_eapol =
				(eapol_key_frame *)
					temp_shared_key_desc->rsna_key_desc;
			if (check_replay_counter_values(rcvd_m4_eapol) == 0) {
				u8 temp_mic[MIC_LEN] = {0};
				u8 rcvd_mic[MIC_LEN] = {0};

				INFO("rcvd M4 body length : %d \n",
				     cur_if->pnan_info->nan_security.m4->size);
				memcpy(rcvd_mic, rcvd_m4_eapol->key_mic,
				       MIC_LEN);
				memset(rcvd_m4_eapol->key_mic, 0x0, MIC_LEN);

				eapol_key_mic(cur_if->pnan_info->nan_security
						      .ptk_buf.kck,
					      16,
					      &cur_if->pnan_info->nan_security
						       .m4->buf[10],
					      cur_if->pnan_info->nan_security
							      .m4->size -
						      NAN_BUF_HDR_SIZE,
					      temp_mic);
				mwu_hexdump(
					MSG_INFO,
					"NAN2 DBG MIC generated after M4 is rcvd \n",
					temp_mic, MIC_LEN);

				if (memcmp(rcvd_mic, temp_mic, MIC_LEN)) {
					ERR("Invalid EAPOL-Key MIC");
					INFO("MIC validation has failed for rcvd M4\n");

					free(cur_if->pnan_info->nan_security.m4);
					free(cur_if->pnan_info->nan_security.m3);
					/*Change state to IDLE*/
					change_ndp_state(cur_if, NDP_IDLE);
					ret = NAN_ERR_INVAL;
					ERR("Failed to process NDP install and 5E cmd to firmware\n");
					return ret;
				}

				KEY_MATERIAL key_material_buf;

				encrypt_key pdata_buf;
				encrypt_key *pdata_ptr;
				pdata_ptr = &pdata_buf;
				pdata_ptr->key_len = 16;

				memcpy(cur_if->pnan_info->nan_security.tk,
				       &(cur_if->pnan_info->nan_security.ptk_buf
						 .tk),
				       TK_LEN);
				memcpy(pdata_ptr->key,
				       &(cur_if->pnan_info->nan_security.ptk_buf
						 .tk),
				       TK_LEN);
				memcpy(pdata_ptr->mac_addr,
				       &(cur_if->pnan_info->ndc_info[0]
						 .ndl_info[0]
						 .ndp_info[0]
						 .peer_ndi),
				       ETH_ALEN);

				mwu_hexdump(MSG_INFO, "key material",
					    cur_if->pnan_info->nan_security.tk,
					    TK_LEN);
				free(cur_if->pnan_info->nan_security.m4);
				free(cur_if->pnan_info->nan_security.m3);

				if (mwu_add_nan_peer(cur_if) !=
				    MWU_ERR_SUCCESS) {
					ERR("Security install : ERR in sending peer_mac to firmware\n");
				}

				if (mwu_key_material(cur_if, &key_material_buf,
						     &pdata_buf) !=
				    MWU_ERR_SUCCESS) {
					ERR("Security install : ERR in sending key material commnad to firmware\n");
					ret = NAN_ERR_INVAL;
				} else {
					/*Change state to IDLE*/
					change_ndp_state(cur_if, NDP_CONNECTED);

					/*Populate Data Confirmation fields to
					 * be sent in Event Buffer*/
					nan_send_data_confirm_event(
						cur_if, buffer, size);

					/*Set the schedule bitmap*/
					nancmd_set_final_bitmap(cur_if,
								NAN_NDL);

					/*Open nan interface*/
					nan_iface_open();

					/*Clear peers published entries*/
					nan_clear_peer_avail_published_entries(
						cur_if);

					ERR("NAN2 Security key installed successfully\n");
					ret = NAN_ERR_SUCCESS;
				}
			} else {
				INFO("M1tx M2rx not successful\n");
				INFO("Replay counter values of M3 and M4 do not match, changing state to NDP_IDLE\n");
				ERR("Security Key install failed. Replay counter values of M3 and M4 do not match\n");

				/*Change state to IDLE*/
				change_ndp_state(cur_if, NDP_IDLE);

				free(cur_if->pnan_info->nan_security.m4);
				free(cur_if->pnan_info->nan_security.m3);
				ret = NAN_ERR_INVAL;
				return ret;
			}
		} else {
			ERR("M4 does not contain NAN_SHARED_KEY_DESC_ATTR attribute\n");
			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);

			free(cur_if->pnan_info->nan_security.m4);
			free(cur_if->pnan_info->nan_security.m3);
			ret = NAN_ERR_INVAL;
			return ret;
		}
	}
	return ret;
}

enum nan_error nan_send_security_install(struct mwu_iface_info *cur_if)
{
	/*Process NDP request*/
	int ret = NAN_ERR_SUCCESS, status = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	memcpy(sdf_buf->peer_mac_addr,
	       cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = (u8 *)(&sdf_buf->ndp_payload_frame[0]);

	{
		/*Populate NDP attribute*/
		nan_ndp_attr *ndp_attr;
		ndp_attr = (nan_ndp_attr *)ndp_var_attr_ptr;

		if ((cur_if->pnan_info->ndpe_attr_supported) &&
		    (cur_if->pnan_info->peer_avail_info_published
			     .ndpe_attr_supported))
			ndp_attr->attribute_id = NAN_NDP_EXT_ATTR;
		else
			ndp_attr->attribute_id = NAN_NDP_ATTR;
		ndp_attr->dialogue_token = cur_if->pnan_info->ndc_info[0]
						   .ndl_info[0]
						   .ndp_info[0]
						   .dialogue_token; // TODO
		ndp_attr->type_and_status.type = NDP_NDL_TYPE_SECINSTALL;
		ndp_attr->type_and_status.status = NDP_NDL_STATUS_ACCEPTED;
		ndp_attr->reson_code = NAN_REASON_CODE_DEFAULT;
		/*Test only code to send explicit rejection in M4. TBR after
		 * PF*/
		if (cur_if->pnan_info->nan_security.m4_reject_test) {
			ndp_attr->type_and_status.status =
				NDP_NDL_STATUS_REJECTED;
			ndp_attr->reson_code = NAN_REASON_CODE_SECURITY_POLICY;
			ret = NAN_ERR_INVAL;
		}
		memcpy(ndp_attr->initiator_ndi,
		       cur_if->pnan_info->ndc_info[0]
			       .ndl_info[0]
			       .ndp_info[0]
			       .initiator_ndi,
		       ETH_ALEN);
		ndp_attr->ndp_id = cur_if->pnan_info->ndc_info[0]
					   .ndl_info[0]
					   .ndp_info[0]
					   .ndp_id;
		ndp_attr->ndp_ctrl.confirm_req = FALSE;
		ndp_attr->ndp_ctrl.explicit_conf = FALSE;
		ndp_attr->ndp_ctrl.security_present = TRUE;
		ndp_attr->ndp_ctrl.publish_id_present = FALSE;
		ndp_attr->ndp_ctrl.responder_ndi_present = FALSE;

		ndp_attr->len = sizeof(nan_ndp_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_ndp_attr);
		var_sd_attr_len += sizeof(nan_ndp_attr);
	}

	/*Security attributes*/
	if (cur_if->pnan_info->security_required) {
		sdf_buf->subtype = NDP_KEY_INSTALL;

		nan_shared_key_desc_attr *temp_shared_key_desc;
		temp_shared_key_desc =
			(nan_shared_key_desc_attr *)nan_get_nan_attr(
				NAN_SHARED_KEY_DESC_ATTR,
				cur_if->pnan_info->nan_security.m3->buf,
				cur_if->pnan_info->nan_security.m3->size);

		/* If M3 does not contain valid NAN_SHARED_KEY_DESC_ATTR, do not
		 * send Shared key desc attr for M4*/
		if (temp_shared_key_desc) {
			/*Shared key desc attr*/
			{
				/* >>>> TBD <<<<<< Put Key desc for M4 */

				nan_shared_key_desc_attr *key_desc =
					(nan_shared_key_desc_attr *)
						ndp_var_attr_ptr;
				u8 sz_key_desc = sizeof(eapol_key_frame);
				key_desc->attribute_id =
					NAN_SHARED_KEY_DESC_ATTR;
				key_desc->instance_id =
					(u8)cur_if->pnan_info
						->instance_id; /*all instances*/

				eapol_key_frame *m4_eapol;
				eapol_key_frame *rcvd_m3_eapol;
				u8 *m4_body;

				nan_alloc_key_desc_buf(&m4_eapol);

				rcvd_m3_eapol =
					(eapol_key_frame *)temp_shared_key_desc
						->rsna_key_desc;

				m4_body = (u8 *)sdf_buf;
				/**
				 * M4 should contain same nonce value as that of
				 * M2*/
				memcpy(m4_eapol->key_nonce,
				       cur_if->pnan_info->nan_security
					       .local_key_nonce,
				       NONCE_LEN);

				key_desc->len =
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc - NAN_ATTR_HDR_LEN;
				nan_generate_key_desc_m4(
					m4_eapol,
					(eapol_key_frame *)&key_desc
						->rsna_key_desc,
					rcvd_m3_eapol,
					m4_body + 11, /* mac addr + ttl +
							 tx_type*/
					var_sd_attr_len +
						sizeof(nan_shared_key_desc_attr) +
						sz_key_desc + 7, /*Fixed params
								    in frame*/
					(cur_if->pnan_info->nan_security.ptk_buf
						 .kck));

				/*This is a test only option to set wrong mic
				 * and TBR after PF*/
				if (cur_if->pnan_info->nan_security
					    .wrong_mic_test) {
					memset(m4_eapol->key_mic, 0, 16);
					ret = NAN_ERR_INVAL;
				}

				INFO("M4 body length : %lu \n",
				     NDP_ATTR_START_OFFSET + var_sd_attr_len +
					     sizeof(nan_shared_key_desc_attr) +
					     sz_key_desc);
				memcpy(&key_desc->rsna_key_desc, m4_eapol,
				       sz_key_desc);

				nan_free_key_desc_buf(&m4_eapol);

				ndp_var_attr_ptr +=
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc;
				var_sd_attr_len +=
					sizeof(nan_shared_key_desc_attr) +
					sz_key_desc;
			}

			memcpy(cur_if->pnan_info->nan_security.tk,
			       &(cur_if->pnan_info->nan_security.ptk_buf.tk),
			       TK_LEN);
			free(cur_if->pnan_info->nan_security.m3);
		} else {
			ERR("M3 does not contain valid shared key descriptor\n");
			ERR("M4 does not contain NAN_SHARED_KEY_DESC_ATTR attribute\n");
			/*Change state to IDLE*/
			change_ndp_state(cur_if, NDP_IDLE);

			free(cur_if->pnan_info->nan_security.m3);
			ret = NAN_ERR_INVAL;
			return ret;
		}
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out Security install");

	status = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	/* The time delay below differs for different SOCs.
	   i.e. the time for getting SDF_TX_DONE from FW for the security
	   install frame. Sometimes the keys are tried to be sent to FW even
	   before this SDF goes out, causing some inconsistent behaviour.
	   8987-CA2 requires relatively more time, so increasing this to 2
	   seconds for safer side. */
	sleep(2);

	if (cur_if->pnan_info->security_required && status == NAN_ERR_SUCCESS &&
	    ret == NAN_ERR_SUCCESS) {
		KEY_MATERIAL key_material_buf;

		encrypt_key pdata_buf;
		encrypt_key *pdata_ptr;
		pdata_ptr = &pdata_buf;
		pdata_ptr->key_len = TK_LEN;
		memcpy(pdata_ptr->key,
		       &(cur_if->pnan_info->nan_security.ptk_buf.tk),
		       pdata_ptr->key_len);
		memcpy(pdata_ptr->mac_addr,
		       &(cur_if->pnan_info->ndc_info[0]
				 .ndl_info[0]
				 .ndp_info[0]
				 .peer_ndi),
		       ETH_ALEN);
		mwu_hexdump(MSG_INFO, "key material",
			    cur_if->pnan_info->nan_security.tk,
			    pdata_ptr->key_len);

		if (mwu_add_nan_peer(cur_if) != MWU_ERR_SUCCESS) {
			ERR("Security install : ERR in sending peer_mac to firmware\n");
		}

		if (mwu_key_material(cur_if, &key_material_buf, &pdata_buf) !=
		    0) {
			ret = NAN_ERR_INVAL;
			ERR("ERR in key material\n");
		}
	}

	FREE(mrvl_cmd);
	return ret;
}

enum nan_error nan_send_schedule_update(struct mwu_iface_info *cur_if,
					nan_ulw_param *ulw, char mac[ETH_ALEN])
{
	int ret = NAN_ERR_SUCCESS;
	static int seq_id = 1;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;
	u32 sched_req_bitmap = 0, peer_bitmap = 0;
	u8 sched_op_class = 0, sched_op_chan = 0;

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	// memcpy(sdf_buf->peer_mac_addr,cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac,
	// ETH_ALEN); memcpy(sdf_buf->peer_mac_addr,
	// cur_if->pnan_info->peer_mac, ETH_ALEN);
	memcpy(sdf_buf->peer_mac_addr, mac, ETH_ALEN);
	INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	     UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = &sdf_buf->ndp_payload_frame[0];

	sdf_buf->subtype = SCHEDULE_UPDATE;

	/*Unalgined schedule attribute*/
	if (ulw) {
		nan_unaligned_sched_attr *unaligned_sched =
			(nan_unaligned_sched_attr *)ndp_var_attr_ptr;
		u64 cur_tsf, start_tsf;
		unaligned_sched->attribute_id = NAN_UNALIGNED_SCHEDULE_ATTR;

		unaligned_sched->attribute_ctrl.sched_id = 1;
		unaligned_sched->attribute_ctrl.seq_id = seq_id++;

		/*Get hosts current tsf value*/
		cur_tsf = nancmd_get_tsf_for_ulw(cur_if, NULL);
		/*Start time is lower 32 bits of current tsf + start_time
		 * parameter*/
		start_tsf = cur_tsf + ulw->start_time;
		unaligned_sched->start_time = start_tsf & 0x00000000ffffffff;
		ulw->start_time = unaligned_sched->start_time;

		unaligned_sched->duration = ulw->duration;
		unaligned_sched->period = ulw->period;
		unaligned_sched->count_down = ulw->count_down;
		INFO("NAN2:count down %d", unaligned_sched->count_down);

		unaligned_sched->ulw_overwrite.overwrite_all =
			1; /*higher precedence over all availability attrs*/

		unaligned_sched->len =
			sizeof(nan_unaligned_sched_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_unaligned_sched_attr);
		var_sd_attr_len += sizeof(nan_unaligned_sched_attr);

		if (ulw->channel) {
			chan_entry_list_t *chan_list;
			nan_ulw_control *ulw_control =
				(nan_ulw_control *)&unaligned_sched->optional[0];
			ulw_control->type = 0x1;
			ulw_control->channel_avail = ulw->avail_type;
			ulw_control->rx_nss = 1;

			chan_list = (chan_entry_list_t *)&unaligned_sched
					    ->optional[1];
			chan_list->op_class = ulw->op_class;
			INFO("NAN2:op class %d", chan_list->op_class);
			chan_list->chan_bitmap = ndp_get_chan_bitmap(
				ulw->op_class, ulw->channel);
			chan_list->primary_chan_bitmap = 0x0;
			unaligned_sched->len += sizeof(nan_ulw_control) +
						sizeof(chan_entry_list_t);
			ndp_var_attr_ptr += sizeof(nan_ulw_control) +
					    sizeof(chan_entry_list_t);
			var_sd_attr_len += sizeof(nan_ulw_control) +
					   sizeof(chan_entry_list_t);
		}

	} else {
		// Availability attr
		nan_availability_attr *availability_attr;
		nan_availability_list *entry;
		nan_channel_entry_list *chan_list;
		u8 opt_fields_len = 7;
		u8 sz_of_chan_entry_list = sizeof(chan_entry_list_t);
		u8 sz_of_band_id = sizeof(u8);
		u8 sz_of_entry_len = sizeof(u16);
		NDC_INFO *ndc;
		NDL_INFO *ndl;

		ndc = &cur_if->pnan_info->ndc_info[0]; // current NDC_INFO
		ndl = &ndc->ndl_info[0]; // current NDL_INFO

		availability_attr = (nan_availability_attr *)ndp_var_attr_ptr;
		availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
		availability_attr->attr_ctrl.map_id =
			cur_if->pnan_info->self_avail_info.map_id;
		availability_attr->attr_ctrl.committed_changed = 1;
		availability_attr->seq_id =
			++cur_if->pnan_info->self_avail_info.seq_id;

		availability_attr->len =
			sizeof(nan_availability_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_availability_attr);
		var_sd_attr_len += sizeof(nan_availability_attr);

		if (ndc->ndc_setup && ndl->ndl_setup) { // schedule update

			{// conditional entries

			 {// Adding the first conditional entry

			  if (cur_if->pnan_info->schedule_update_needed){
				  sched_op_class =
					  cur_if->pnan_info->ndl_sched.op_class;
			sched_op_chan = cur_if->pnan_info->ndl_sched.op_chan;
			sched_req_bitmap =
				cur_if->pnan_info->ndl_sched.availability_map[0];
		} else {
			/* Compute the bitmap and specify default channel
			 * parameters*/
			// peer_availability_info *peer_info
			peer_availability_info *self_info;
			// peer_info = &cur_if->pnan_info->peer_avail_info;

			self_info = &cur_if->pnan_info->self_avail_info;

			sched_op_class = self_info->entry_committed[0].op_class;
			sched_op_chan =
				self_info->entry_committed[0].channels[0];

			// include the existing NDC schedule
			sched_req_bitmap = ndc->slot;

			// include the existing immutable schedule if any
			if (ndl->immutable_sched_present) {
				sched_req_bitmap |= ndl->immutable_sched_bitmap;
			}

			/*
			// select random slots from peer's committed
			availability peer_bitmap =
			self_info->entry_committed[0].combined_time_bitmap;

			if((peer_bitmap & PREFERRED_BITMAP1) == 0)
			    peer_bitmap &= PREFERRED_BITMAP2;
			else
			    peer_bitmap &= PREFERRED_BITMAP1;
			*/

			sched_req_bitmap |= NAN_DEFAULT_BITMAP;

			/*
					// Add more random slots from the ones
			   not covered by above cases if((sched_req_bitmap |
			   RANDOM_BITMAP2) == sched_req_bitmap) sched_req_bitmap
			   |= RANDOM_BITMAP1; else sched_req_bitmap |=
			   RANDOM_BITMAP2;
			*/

			/* consider peer's potential bitmap sent previously
			peer_bitmap =
			peer_info->entry_potential[0].combined_time_bitmap;
			if(peer_bitmap)
			    sched_req_bitmap &= peer_bitmap;
			*/

			// Consider current committed availability
			peer_bitmap = self_info->entry_committed[0]
					      .combined_time_bitmap;
			if (peer_bitmap)
				sched_req_bitmap &= peer_bitmap;

			// self potential
			sched_req_bitmap &= NAN_POTENTIAL_BITMAP;

			// In case the immutable and ndc slots go outside
			sched_req_bitmap = ndc->slot;
			if (ndl->immutable_sched_present) {
				sched_req_bitmap |= ndl->immutable_sched_bitmap;
			}
		}

		INFO("Adding the schedule-update committed entry 1");
		entry = (nan_availability_list *)ndp_var_attr_ptr;
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

			memcpy(time_bitmap, &sched_req_bitmap, 4); /*time bitmap
								      computed
								      above*/
		}
		/*Hop over by (avail list + optional)bytes to get chan list */
		chan_list =
			(nan_channel_entry_list *)(ndp_var_attr_ptr +
						   sizeof(nan_availability_list) +
						   opt_fields_len);
		chan_list->entry_ctrl.entry_type = 1;
		chan_list->entry_ctrl.band_type = 0;
		chan_list->entry_ctrl.num_entries = 1;
		chan_list->chan_band_entry.chan_entry.op_class = sched_op_class;
		chan_list->chan_band_entry.chan_entry.chan_bitmap =
			ndp_get_chan_bitmap(sched_op_class, sched_op_chan);
		chan_list->chan_band_entry.chan_entry.primary_chan_bitmap = 0x0;
		// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap = 0x0;

		entry->len = sizeof(nan_channel_entry_list) +
			     sizeof(nan_availability_list) + opt_fields_len -
			     sz_of_entry_len;
		availability_attr->len += entry->len + sz_of_entry_len;
		ndp_var_attr_ptr += entry->len + sz_of_entry_len;
		var_sd_attr_len += entry->len + sz_of_entry_len;

	} // End of adding the first entry

	{ // Adding the second conditional entry- with other random channel and
	  // bitmap

		/* Compute the bitmap and specify default channel parameters*/
		// peer_availability_info *peer_info;
		u8 sched_op_class2, sched_op_chan2;
		u32 sched_req_bitmap2 = 0;
		// peer_info = &cur_if->pnan_info->peer_avail_info;

		sched_op_class2 = DEFAULT_2G_OP_CLASS;
		sched_op_chan2 = DEFAULT_2G_OP_CHAN;

		generate_sched_update_bitmap(sched_req_bitmap,
					     &sched_req_bitmap2);

		/*
				// Add more random slots from the ones not
		   covered by above cases if((sched_req_bitmap | RANDOM_BITMAP2)
		   == sched_req_bitmap) sched_req_bitmap |= RANDOM_BITMAP1; else
				    sched_req_bitmap |= RANDOM_BITMAP2;

				//consider peer's potential bitmap sent
		   previously peer_bitmap =
		   peer_info->entry_potential[0].combined_time_bitmap;
				if(peer_bitmap)
				    sched_req_bitmap &= peer_bitmap;
		*/

		INFO("Adding the schedule-update committed entry 2");
		entry = (nan_availability_list *)ndp_var_attr_ptr;
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

			memcpy(time_bitmap, &sched_req_bitmap2, 4); /*time
								       bitmap
								       computed
								       above*/
		}
		/*Hop over by (avail list + optional)bytes to get chan list */
		chan_list =
			(nan_channel_entry_list *)(ndp_var_attr_ptr +
						   sizeof(nan_availability_list) +
						   opt_fields_len);
		chan_list->entry_ctrl.entry_type = 1;
		chan_list->entry_ctrl.band_type = 0;
		chan_list->entry_ctrl.num_entries = 1;
		chan_list->chan_band_entry.chan_entry.op_class =
			sched_op_class2;
		chan_list->chan_band_entry.chan_entry.chan_bitmap =
			ndp_get_chan_bitmap(sched_op_class2, sched_op_chan2);
		chan_list->chan_band_entry.chan_entry.primary_chan_bitmap = 0x0;
		// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap = 0x0;

		entry->len = sizeof(nan_channel_entry_list) +
			     sizeof(nan_availability_list) + opt_fields_len -
			     sz_of_entry_len;
		availability_attr->len += entry->len + sz_of_entry_len;
		ndp_var_attr_ptr += entry->len + sz_of_entry_len;
		var_sd_attr_len += entry->len + sz_of_entry_len;

	} // End of adding the second entry
}
// Add the potential entry
{
	nan_availability_list *potential_entry;
	potential_entry = (nan_availability_list *)ndp_var_attr_ptr;

	potential_entry->entry_ctrl.avail_type = NDP_AVAIL_TYPE_POTENTIAL;
	potential_entry->entry_ctrl.usage_preference = 1;
	potential_entry->entry_ctrl.utilization = 0;
	potential_entry->entry_ctrl.rx_nss = 1;

	if (cur_if->pnan_info->self_avail_info.band_entry_potential) {
		/*Device available on all slots on the given band*/
		potential_entry->entry_ctrl.time_bitmap_present = 0;
		chan_list = (nan_channel_entry_list
				     *)(ndp_var_attr_ptr +
					sizeof(nan_availability_list));
		chan_list->entry_ctrl.entry_type = 0x0;
		chan_list->entry_ctrl.band_type = 0x0;
		chan_list->entry_ctrl.num_entries = 0x1;
		if (cur_if->pnan_info->a_band)
			chan_list->chan_band_entry.band_id = 0x04; /*5 GHz*/
		else
			chan_list->chan_band_entry.band_id = 0x02; /*2.4 GHz*/

		potential_entry->len = sizeof(nan_channel_entry_list) +
				       sizeof(nan_availability_list) -
				       sz_of_entry_len - sz_of_chan_entry_list +
				       sz_of_band_id; // set the potential
						      // entry's length
		// add the potetial entry's length in the main avail_attr and
		// other pointers
		availability_attr->len +=
			potential_entry->len + sz_of_entry_len;
		ndp_var_attr_ptr += potential_entry->len + sz_of_entry_len;
		var_sd_attr_len += potential_entry->len + sz_of_entry_len;

	} else { // non-band entry

		potential_entry->entry_ctrl.time_bitmap_present = 1;
		/*Populate optional fields*/
		{
			time_bitmap_control *time_bitmap_ctrl =
				(time_bitmap_control *)&potential_entry
					->optional[0];
			u8 *time_bitmap_len = &potential_entry->optional[2];
			u8 *time_bitmap = &potential_entry->optional[3];

			time_bitmap_ctrl->bit_duration = NDP_TIME_BM_DUR_16;
			time_bitmap_ctrl->bit_period = NDP_TIME_BM_PERIOD_512;
			time_bitmap_ctrl->start_offset = 0;
			*time_bitmap_len = 4; /*time_bitmap_len*/

			memcpy(time_bitmap,
			       &cur_if->pnan_info->self_avail_info
					.entry_potential[0]
					.time_bitmap[0],
			       4); /*time bitmap*/
		}
		/*Hop over by 5 bytes(time_bitmap_len + time_bitmap) to get chan
		 * list */
		chan_list =
			(nan_channel_entry_list *)(ndp_var_attr_ptr +
						   sizeof(nan_availability_list) +
						   opt_fields_len);
		chan_list->entry_ctrl.entry_type = 1;
		chan_list->entry_ctrl.band_type = 0;
		chan_list->entry_ctrl.num_entries = 1;

		chan_list->chan_band_entry.chan_entry.op_class =
			cur_if->pnan_info->self_avail_info.entry_potential[0]
				.op_class;
		chan_list->chan_band_entry.chan_entry.chan_bitmap =
			ndp_get_chan_bitmap(cur_if->pnan_info->self_avail_info
						    .entry_potential[0]
						    .op_class,
					    cur_if->pnan_info->self_avail_info
						    .entry_potential[0]
						    .channels[0]);
		chan_list->chan_band_entry.chan_entry.primary_chan_bitmap = 0x0;
		// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap = 0x0;

		// set the potential entry's length
		potential_entry->len = sizeof(nan_channel_entry_list) +
				       sizeof(nan_availability_list) +
				       opt_fields_len - sz_of_entry_len;
		// add the potetial entry's length in the main avail_attr and
		// other pointers
		availability_attr->len +=
			potential_entry->len + sz_of_entry_len;
		ndp_var_attr_ptr += potential_entry->len + sz_of_entry_len;
		var_sd_attr_len += potential_entry->len + sz_of_entry_len;
	}
} // end of the block adding potential entry

} // End of the if for ndp-ndl setup
}

cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

/* adjust cmd_size */
cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
INFO("Sending out Schedule Update");

ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
FREE(mrvl_cmd);

if (ulw) {
	nancmd_set_unaligned_sched(cur_if, ulw);
} else {
	peer_availability_info *peer_info, *self_info;

	peer_info = &cur_if->pnan_info->peer_avail_info;
	self_info = &cur_if->pnan_info->self_avail_info;

	cur_if->pnan_info->self_avail_info.committed_valid = 1;
	cur_if->pnan_info->peer_avail_info.committed_valid = 1;
	cur_if->pnan_info->self_avail_info.conditional_valid = 0;
	cur_if->pnan_info->peer_avail_info.conditional_valid = 0;

	peer_info->entry_committed[0].time_bitmap[0] = sched_req_bitmap;
	peer_info->entry_committed[0].op_class = sched_op_class;
	peer_info->entry_committed[0].channels[0] = sched_op_chan;

	self_info->entry_committed[0].time_bitmap[0] = sched_req_bitmap;
	self_info->entry_committed[0].op_class = sched_op_class;
	self_info->entry_committed[0].channels[0] = sched_op_chan;

	nancmd_set_final_bitmap(cur_if, NAN_NDL);
}

return ret;
}

enum nan_error nan_send_bcast_schedule_update(struct mwu_iface_info *cur_if)
{
	int ret = NAN_ERR_SUCCESS;
	mrvl_cmd_head_buf *cmd;
	nan_ndp *sdf_buf = NULL;
	u8 *ndp_var_attr_ptr = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len = strlen(CMD_NXP) + strlen(NAN_SDF_CMD);
	int cmd_len = 0;
	int var_sd_attr_len = 0;
	unsigned char mac[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};

	ENTER();

	mrvl_cmd = nan_cmdbuf_alloc(MRVDRV_SIZE_OF_CMD_BUFFER, NAN_SDF_CMD,
				    HostCmd_CMD_NAN_SDF); /* adjust
							     mrvl_cmd->size at
							     the end */
	if (!mrvl_cmd)
		return NAN_ERR_NOMEM;

	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);

	sdf_buf = (nan_ndp *)cmd->cmd_data;
	// memcpy(sdf_buf->peer_mac_addr,cur_if->pnan_info->ndc_info[0].ndl_info[0].peer_mac,
	// ETH_ALEN); memcpy(sdf_buf->peer_mac_addr,
	// cur_if->pnan_info->peer_mac, ETH_ALEN);
	memcpy(sdf_buf->peer_mac_addr, mac, ETH_ALEN);
	// INFO("\nsdf_buf->peer_mac_addr: " UTIL_MACSTR,
	// UTIL_MAC2STR(sdf_buf->peer_mac_addr));
	sdf_buf->tx_type = 3;
	sdf_buf->ttl = 0;
	sdf_buf->category = NAN_CATEGORY_PUBLIC_ACTION_FRAME;
	sdf_buf->action = 0x09;
	sdf_buf->oui[0] = 0x50;
	sdf_buf->oui[1] = 0x6F;
	sdf_buf->oui[2] = 0x9A;
	sdf_buf->oui_type = NAN_ACTION_FRAME;

	ndp_var_attr_ptr = &sdf_buf->ndp_payload_frame[0];

	sdf_buf->subtype = SCHEDULE_UPDATE;

	{
		nan_availability_attr *availability_attr;
		nan_availability_list *entry;
		nan_channel_entry_list *chan_list;
		u8 opt_fields_len = 7, entry_valid = 0, i = 0, k = 0;
		;
		avail_entry_t *self_entry = NULL;

		availability_attr = (nan_availability_attr *)ndp_var_attr_ptr;
		availability_attr->attribute_id = NAN_AVAILABILITY_ATTR;
		availability_attr->attr_ctrl.map_id =
			cur_if->pnan_info->self_avail_info.map_id;
		/*Report Change in committed entry*/
		availability_attr->attr_ctrl.committed_changed =
			cur_if->pnan_info->self_avail_info.committed_changed;
		INFO("==> NDPRESP Map id is %d",
		     availability_attr->attr_ctrl.map_id);
		availability_attr->seq_id =
			cur_if->pnan_info->self_avail_info.seq_id;

		availability_attr->len =
			sizeof(nan_availability_attr) - NAN_ATTR_HDR_LEN;
		ndp_var_attr_ptr += sizeof(nan_availability_attr);
		var_sd_attr_len += sizeof(nan_availability_attr);

		self_entry = cur_if->pnan_info->self_avail_info.entry_committed;
		entry_valid =
			cur_if->pnan_info->self_avail_info.committed_valid;

		for (i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
			/*Send all the valid committed entries */
			if ((entry_valid & (1 << i))) {
				for (k = 0; k < self_entry[i].time_bitmap_count;
				     k++) {
					INFO("Adding committed entry#%u", i);
					entry = (nan_availability_list *)
						ndp_var_attr_ptr;
					entry->entry_ctrl.avail_type =
						NDP_AVAIL_TYPE_COMMITTED;

					entry->entry_ctrl.usage_preference =
						0x1;
					entry->entry_ctrl.utilization = 0x0;
					entry->entry_ctrl.rx_nss = 0x1;
					entry->entry_ctrl.time_bitmap_present =
						0x1;
					/*Populate optional attributes*/
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
							(u16)NDP_TIME_BM_DUR_16;
						time_bitmap_ctrl->bit_period =
							self_entry[i].period;
						time_bitmap_ctrl->start_offset =
							self_entry[i]
								.start_offset[k] /
							16;
						*time_bitmap_len =
							4; /*time_bitmap_len*/
						memcpy(time_bitmap,
						       &self_entry[i]
								.time_bitmap[k],
						       4); /*time bitmap*/
					}
					INFO("With time_bitmap 0x%x , offset %u , op_class %u and op_chan %u",
					     self_entry[i].time_bitmap[k],
					     self_entry[i].start_offset[k],
					     self_entry[i].op_class,
					     self_entry[i].channels[0]);

					chan_list =
						(nan_channel_entry_list
							 *)(ndp_var_attr_ptr +
							    sizeof(nan_availability_list) +
							    opt_fields_len);
					chan_list->entry_ctrl.entry_type = 0x1;
					chan_list->entry_ctrl.band_type = 0x0;
					chan_list->entry_ctrl.num_entries = 0x1;
					chan_list->chan_band_entry.chan_entry
						.op_class =
						self_entry[i].op_class;
					chan_list->chan_band_entry.chan_entry
						.chan_bitmap =
						ndp_get_chan_bitmap(
							self_entry[i].op_class,
							self_entry[i]
								.channels[0]);
					chan_list->chan_band_entry.chan_entry
						.primary_chan_bitmap = 0x0;
					// chan_list->chan_band_entry.chan_entry.aux_chan_bitmap
					// = 0x0;

					entry->len =
						sizeof(nan_channel_entry_list) +
						sizeof(nan_availability_list) +
						opt_fields_len - 2;
					availability_attr->len +=
						entry->len + 2;
					ndp_var_attr_ptr += entry->len + 2;
					var_sd_attr_len += entry->len + 2;
				}
			}
		}
	}

	cmd_len = cmd_len + sizeof(nan_ndp) + var_sd_attr_len;

	/* adjust cmd_size */
	cmd->size = sizeof(mrvl_cmd_head_buf) + cmd_len;
	INFO("Sending out broadcast Schedule Update");

	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);
	FREE(mrvl_cmd);

	return ret;
}

void nan_send_data_confirm_event(struct mwu_iface_info *cur_if,
				 unsigned char *buffer, int size)
{
	struct ndp_data_confirm *data_conf = NULL;
	nan_ndp_attr *ndp_attr = NULL;
	u8 publish_id = 0, *responder_ndi = NULL, *service_info = NULL;
	int service_len = 0, data_conf_len = 0;
	/*Extract NDP and NDL Attribute*/
	if ((!cur_if->pnan_info->ndpe_not_present) &&
	    (cur_if->pnan_info->ndpe_attr_supported) &&
	    (cur_if->pnan_info->peer_avail_info_published.ndpe_attr_supported))
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_EXT_ATTR,
							    buffer, size);
	else
		ndp_attr = (nan_ndp_attr *)nan_get_nan_attr(NAN_NDP_ATTR,
							    buffer, size);
	if (ndp_attr) {
		nan_get_optional_ndp_attr(ndp_attr, &publish_id, &responder_ndi,
					  &service_info, &service_len);
	}

	/*Populate Data Confirmation fields to be sent in Event Buffer*/
	data_conf_len = sizeof(struct ndp_data_confirm) + service_len;
	data_conf = (struct ndp_data_confirm *)malloc(data_conf_len);
	memset(data_conf, 0, data_conf_len);
	data_conf->type = NDP_TYPE_UNICAST;
	data_conf->status_reason.status = NDP_NDL_STATUS_ACCEPTED;
	data_conf->status_reason.reason = NAN_REASON_CODE_DEFAULT;
	data_conf->ndp_chan =
		cur_if->pnan_info->self_avail_info.entry_committed[0]
			.channels[0];
	data_conf->ndp_chan2 =
		cur_if->pnan_info->self_avail_info.entry_committed[1]
			.channels[0];
	memcpy(data_conf->peer_ndi,
	       cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0].peer_ndi,
	       ETH_ALEN);
	// if(ndp_attr)
	{
		data_conf->publish_id = publish_id;
		data_conf->ndp_id = cur_if->pnan_info->ndc_info[0]
					    .ndl_info[0]
					    .ndp_info[0]
					    .ndp_id;
		memcpy(data_conf->initiator_addr,
		       cur_if->pnan_info->ndc_info[0]
			       .ndl_info[0]
			       .ndp_info[0]
			       .initiator_ndi,
		       ETH_ALEN);
		memcpy(data_conf->responder_addr,
		       cur_if->pnan_info->ndc_info[0]
			       .ndl_info[0]
			       .ndp_info[0]
			       .responder_ndi,
		       ETH_ALEN);
		/*if(responder_ndi)
		    memcpy(data_conf->responder_addr,responder_ndi,ETH_ALEN);*/
		data_conf->service_info.len = service_len;
		if (service_info && service_len)
			memcpy(data_conf->service_info.data, service_info,
			       service_len);
	}
	INFO("Data Confirm Event buffer populated");

	/*Send event to app*/
	nan_send_ndp_event(cur_if, NAN_EVENT_DATA_CONFIRM, (u8 *)data_conf,
			   data_conf_len);
	FREE(data_conf);
}

void nan_clear_self_avail_entries(struct mwu_iface_info *cur_if)
{
	cur_if->pnan_info->self_avail_info.committed_valid = 0;
	cur_if->pnan_info->self_avail_info.time_bitmap_present_potential = 0;
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] =
		NAN_POTENTIAL_BITMAP;
	cur_if->pnan_info->self_avail_info.band_entry_potential = TRUE;
	INFO("NAN2: self availability info is reset");
}

void nan_clear_peer_avail_entries(struct mwu_iface_info *cur_if)
{
	cur_if->pnan_info->peer_avail_info.committed_valid = 0;
	cur_if->pnan_info->peer_avail_info.potential_valid = 0;
	cur_if->pnan_info->peer_avail_info.conditional_valid = 0;
	INFO("NAN2: cleared peer availability info");
}

void nan_clear_peer_avail_published_entries(struct mwu_iface_info *cur_if)
{
	cur_if->pnan_info->peer_avail_info_published.committed_valid = 0;
	cur_if->pnan_info->peer_avail_info_published.potential_valid = 0;
	cur_if->pnan_info->peer_avail_info_published.conditional_valid = 0;
	INFO("NAN2: cleared peer availability info in publish");
}

void nan_clear_avail_entries(struct mwu_iface_info *cur_if)
{
	nan_clear_self_avail_entries(cur_if);
	nan_clear_peer_avail_published_entries(cur_if);
	nan_clear_peer_avail_entries(cur_if);
}
