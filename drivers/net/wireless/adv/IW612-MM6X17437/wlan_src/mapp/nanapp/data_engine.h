#ifndef __DATA_ENGINE_H__
#define __DATA_ENGINE_H__

#include "nan.h"

/*Attribute types*/
#define NAN_DEVICE_CAPABILITY_ATTR 0x0f
#define NAN_NDP_ATTR 0x10
#define NAN_AVAILABILITY_ATTR 0x12
#define NAN_NDC_ATTR 0x13
#define NAN_NDL_ATTR 0x14
#define NAN_NDL_QOS_ATTR 0x15
#define NAN_UNALIGNED_SCHEDULE_ATTR 0x17
#define NAN_ELEMENT_CONTAINER_ATTR 0x1d
#define NAN_CIPHER_SUITE_INFO_ATTR 0x22
#define NAN_SEC_CONTEXT_INFO_ATTR 0x23
#define NAN_SHARED_KEY_DESC_ATTR 0x24
#define NAN_NDP_EXT_ATTR 0x29
/*flags used in scehdule bitmaps*/
#define NDP_DEFAULT_DILOGUE_TOKEN 0x1

#define NAN_NDP_BITMAP_PUB_ID 0x08
#define NAN_NDP_BITMAP_RESP_NDI 0x10
#define NAN_NDP_BITMAP_OPAQ_INFO_PRESENT 0x20
#define NDP_CONFIRM_REQUIRED 0x01
#define NDP_TIME_BITMAP_PRESENT 0x2000

#define NDP_TYPE_STATUS_BITMAP_REQ 0x0000
#define NDP_TYPE_STATUS_BITMAP_RESP 0x0001
#define NDP_TYPE_STATUS_BITMAP_CONFIRM 0x0002
#define NDP_TYPE_STATUS_BITMAP_TERMINATE 0x0004

#define NDP_TYPE_STATUS_BITMAP_CONTINUED 0x0000
#define NDP_TYPE_STATUS_BITMAP_ACCEPTED 0x0010
#define NDP_TYPE_STATUS_BITMAP_REJECTED 0x0020

#define NDP_TYPE_STATUS_BITMAP_MASK 0x00F0

#define NDP_AVAIL_TYPE_COMMITTED 0x1
#define NDP_AVAIL_TYPE_POTENTIAL 0x2
#define NDP_AVAIL_TYPE_CONDITIONAL 0x4
#define NDP_AVAIL_TYPE_POTENTIAL_CONDITIONAL 0x6

#define NDP_AVAIL_TIME_BM_PRESENT 0x2000
#define NDP_AVAIL_CHAN_BM_PRESENT 0x4000

#define NDP_TIME_BM_DUR_16 0x0000
#define NDP_TIME_BM_PERIOD_512 0x0003

#define NDP_CHAN_ENT_TYPE_BAND 0x0000
#define NDP_CHAN_ENT_TYPE_CHAN 0x0001

#define NDP_CHAN_ENT_BW_CONT 0x0000

#define NDP_CHAN_ENT_NUM_ONE 0x0010

#define NDP_CHAN_ENT_BAND_B 0x0002

/*Fields used in NAN attributes*/
#define NDP_NDL_TYPE_REQUEST 0x00
#define NDP_NDL_TYPE_RESPONSE 0x01
#define NDP_NDL_TYPE_CONFIRM 0x02
#define NDP_NDL_TYPE_SECINSTALL 0x03
#define NDP_NDL_TYPE_TERMINATE 0x04
#define NDP_NDL_TYPE_RESERVED 0x05

#define NDP_NDL_STATUS_CONTINUED 0x00
#define NDP_NDL_STATUS_ACCEPTED 0x01
#define NDP_NDL_STATUS_REJECTED 0x02
#define NDP_NDL_STATUS_RESERVED 0x03

#define NDP_TYPE_UNICAST 0
#define NDP_TYPE_MULTICAST 1

#define NAN_REASON_CODE_DEFAULT 0
#define NAN_REASON_CODE_UNSPECIFIED 1
#define NAN_REASON_CODE_RESOURCE_LIMITATION 2
#define NAN_REASON_CODE_INVALID_PARAM 3
#define NAN_REASON_CODE_FTM_PARAM_INCAPABLE 4
#define NAN_REASON_CODE_NO_MOVEMENT 5
#define NAN_REASON_CODE_INVALID_AVAILABILITY 6
#define NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE 7
#define NAN_REASON_CODE_SECURITY_POLICY 8
#define NAN_REASON_CODE_QOS_UNACCEPTABLE 9
#define NAN_REASON_CODE_NDP_REJECTED 10
#define NAN_REASON_CODE_NDL_UNACCEPTABLE 11
#define NAN_REASON_CODE_RESERVED 12

/*NAN Security macros*/
#define NCS_SK_CCM_128 1
/* Misc Flags*/
#define NDP_IGNORE 0
#define NDP_RESERVED 0
#define NDP_ATTR_START_OFFSET                                                  \
	17 /*Peer Mac(6),RSSI(4),category(1), action(1),                       \
	    OUI(3),OUI_Type(1),frame_subtype(1)*/
#define TLV_TYPE_IPV6_LINK_LOCAL 0x00
#define TLV_TYPE_SERVICE_INFO 0x01

#define SERVICE_PROTOCOL_GENERIC 0x02

#define SERVICE_PROTOCOL_TRANSPORT_PORT_SUB_ATTR 0x00
#define SERVICE_PROTOCOL_TRANSPORT_PORTOCOL_SUB_ATTR 0x01

#define TRANSPORT_PROTOCOL_TCP 0x06
#define TRANSPORT_PROTOCOL_UCP 0x11

#define IPV6_IFACE_IDENTIFIER_LEN 8

#define ENTRY0_VALID 0x1
#define ENTRY1_VALID 0x2

/*INIT MACROs*/
#define DEFAULT_DEVICE_CABAP                                                   \
	{                                                                      \
		.attribute_id = NAN_DEVICE_CAPA_ATTR,                          \
		.len = sizeof(nan_device_capa_attr) - NAN_ATTR_HDR_LEN,        \
		.committed_dw_info =                                           \
			{                                                      \
				._2g_dw = 1,                                   \
				._5g_dw = 0,                                   \
				._2g_dw_overwrite = 0,                         \
				._5g_dw_overwrite = 0,                         \
				.reserved = 0,                                 \
			}                                                      \
				.supported_bands = 0x04,                       \
		.op_mode =                                                     \
			{                                                      \
				.phy_mode = 0,                                 \
				.vht_80_80 = 0,                                \
				.vht_160 = 0,                                  \
				.pndl_supported = 0,                           \
				.reserved = 0,                                 \
			}                                                      \
				.no_of_antennas = 0,                           \
		.max_chan_sw_time = 0,                                         \
	}

/*Attribute structures*/
typedef struct _tlv_header {
	/** TLV type */
	u8 type;
	/** TLV length */
	u16 length;
} __ATTRIB_PACK__ tlv_header, *ptlv_header;

typedef struct _type_status {
	u8 type : 4;
	u8 status : 4;
} __ATTRIB_PACK__ type_status;

typedef struct _nan_ndp_attr {
	u8 attribute_id;
	u16 len;
	u8 dialogue_token;
	type_status type_and_status;
	u8 reson_code;
	u8 initiator_ndi[ETH_ALEN];
	u8 ndp_id;
	struct {
		u8 confirm_req : 1;
		u8 explicit_conf : 1;
		u8 security_present : 1;
		u8 publish_id_present : 1;
		u8 responder_ndi_present : 1;
		u8 ndp_specific_info_present : 1;
		u8 reserved : 2;
	} ndp_ctrl;
	u8 optional[0]; /* This is a placeholder for optional fields
			 publisher id, responder ndi, ndp specific info */
} __ATTRIB_PACK__ nan_ndp_attr;

typedef struct _ipv6_link_local_tlv {
	tlv_header header;
	u8 iface_id[IPV6_IFACE_IDENTIFIER_LEN];
} __ATTRIB_PACK__ ipv6_link_local_tlv;

typedef struct _service_info_tlv {
	tlv_header header;
	u8 oui[3];
	u8 service_protocol_type;
	u8 service_spc_info[0];
} __ATTRIB_PACK__ service_info_tlv;

typedef struct _transport_port_sub_attr {
	tlv_header header;
	u16 transport_port;
} __ATTRIB_PACK__ transport_port_sub_attr;

typedef struct _transport_portocol_sub_attr {
	tlv_header header;
	u8 transport_portocol;
} __ATTRIB_PACK__ transport_protocol_sub_attr;
typedef struct _nan_element_container_attr {
	u8 attribute_id;
	u16 len;
	u8 map_id;
	u8 elements[0];
} __ATTRIB_PACK__ nan_element_container_attr;

typedef struct _time_bitmap_control {
	u16 bit_duration : 3;
	u16 bit_period : 3;
	u16 start_offset : 9;
	u16 reserved : 1;
} __ATTRIB_PACK__ time_bitmap_control;

typedef struct _immutable_sched_entry {
	u8 map_id;
	time_bitmap_control time_bitmap_ctrl;
	u8 time_bitmap_len;
	u8 bitmap[0];
} __ATTRIB_PACK__ immutable_sched_entry;

typedef struct {
	// u8 no_of_entries;
	immutable_sched_entry entry[0];
} __ATTRIB_PACK__ immutable_schedule;

typedef struct _nan_ndl_attr {
	u8 attribute_id;
	u16 len;
	u8 dialogue_token;
	type_status type_and_status;
	u8 reson_code;
	struct {
		u8 ndl_peer_id_present : 1;
		u8 immutable_sched_present : 1;
		u8 ndc_attr_present : 1;
		u8 ndl_qos_attr_present : 1;
		u8 max_idle_period_present : 1;
		u8 ndl_type : 1;
		u8 setup_reason : 2;
	} ndl_ctrl;
	u8 optional[0];
} __ATTRIB_PACK__ nan_ndl_attr;

typedef struct _nan_ndc_attr {
	u8 attribute_id;
	u16 len;
	u8 ndc_id[ETH_ALEN];
	struct {
		u8 proposed_ndc : 1;
		u8 reserved3 : 7;
	} sched_ctrl;
	u8 map_id;
	time_bitmap_control time_bitmap_ctrl;
	u8 time_bitmap_len;
	u8 bitmap[0];
} __ATTRIB_PACK__ nan_ndc_attr;

typedef struct _nan_device_capability_attr {
	u8 attribute_id;
	u16 len;
	u8 map_id;
	struct {
		u16 _2g_dw : 3;
		u16 _5g_dw : 3;
		u16 _2g_dw_overwrite : 4;
		u16 _5g_dw_overwrite : 4;
		u16 reserved : 2;
	} committed_dw_info;
	u8 supported_bands;
	struct {
		u8 phy_mode : 1;
		u8 vht_80_80 : 1;
		u8 vht_160 : 1;
		u8 pndl_support : 1;
		u8 reserved : 4;
	} op_mode;
	u8 no_of_antennas;
	u16 max_chan_sw_time;
	struct {
		u8 dfs_master_device : 1;
		u8 ext_key_ID : 1;
		u8 simult_ndp_data_reception : 1;
		u8 ndpe_attr_supported : 1;
		u8 reserved : 4;
	} capabilities;
} __ATTRIB_PACK__ nan_device_capability_attr;

typedef struct _nan_ndl_qos_attr {
	u8 attribute_id;
	u16 len;
	u8 min_slots;
	u8 max_latency[2];
} __ATTRIB_PACK__ nan_ndl_qos_attr;

// Nachiket changes
typedef struct {
	u8 entry_type : 1;
	u8 band_type : 1;
	u8 reserved : 2;
	u8 num_entries : 4;
} __ATTRIB_PACK__ chan_entry_ctrl_t;
typedef struct {
	u8 op_class;
	u16 chan_bitmap;
	u8 primary_chan_bitmap;
	// u16 aux_chan_bitmap;
} __ATTRIB_PACK__ chan_entry_list_t;

typedef struct _nan_channel_entry_list {
	chan_entry_ctrl_t entry_ctrl;
	union {
		u8 band_id;
		chan_entry_list_t chan_entry;
	} chan_band_entry;
} __ATTRIB_PACK__ nan_channel_entry_list;

typedef struct {
	u16 avail_type : 3;
	u16 usage_preference : 2;
	u16 utilization : 3;
	u16 rx_nss : 4;
	u16 time_bitmap_present : 1;
	u16 reserved : 3;
} __ATTRIB_PACK__ avail_entry_ctrl;

typedef struct _nan_availability_list {
	u16 len;
	avail_entry_ctrl entry_ctrl;
	// time_bitmap_control time_bitmap_ctrl;
	// u8 time_bitmap_len;
	// u8 time_bitmap[0];
	u8 optional[0];
} __ATTRIB_PACK__ nan_availability_list;

typedef struct nan_avail_attr_ctrl {
	u16 map_id : 4;
	u16 committed_changed : 1;
	u16 potential_changed : 1;
	u16 public_avail_changed : 1;
	u16 ndc_changed : 1;
	u16 mcast_sched_attr_changed : 1;
	u16 mcast_sched_change_attr_changed : 1;
	u16 reserved : 6;
} __ATTRIB_PACK__ nan_avail_attr_ctrl;

typedef struct nan_availability_attr {
	u8 attribute_id;
	u16 len;
	u8 seq_id;
	nan_avail_attr_ctrl attr_ctrl;
	nan_availability_list entry[0];
} __ATTRIB_PACK__ nan_availability_attr;
/*Structures for NAN security attributes*/
typedef struct _nan_cipher_suite_attr_info {
	u8 cipher_suite_id;
	u8 instance_id;
} __ATTRIB_PACK__ nan_cipher_suite_attr_info;

typedef struct _nan_cipher_suite_info_attr {
	u8 attribute_id;
	u16 len;
	u8 capabilities;
	nan_cipher_suite_attr_info attr_info[0];
} __ATTRIB_PACK__ nan_cipher_suite_info_attr;

typedef struct _nan_security_context_attr_field {
	u16 identifier_len;
	u8 identifier_type;
	u8 instance_id;
	u8 sec_context_identifier[0];
} __ATTRIB_PACK__ nan_security_context_attr_field;

typedef struct _nan_security_context_info_attr {
	u8 attribute_id;
	u16 len;
	nan_security_context_attr_field identifier_list[0];
} __ATTRIB_PACK__ nan_security_context_info_attr;

typedef struct _nan_shared_key_desc_attr {
	u8 attribute_id;
	u16 len;
	u8 instance_id;
	u8 rsna_key_desc[0];
} __ATTRIB_PACK__ nan_shared_key_desc_attr;

typedef struct _nan_unaligned_attr_ctrl {
	u8 sched_id : 4;
	u8 reserved : 4;
	u8 seq_id;
} __ATTRIB_PACK__ nan_unaligned_attr_ctrl;

typedef struct _nan_ulw_overwrite {
	u8 overwrite_all : 1;
	u8 map_id : 4;
	u8 reserved : 3;
} __ATTRIB_PACK__ nan_ulw_overwrite;

typedef struct _nan_ulw_control {
	u8 type : 2;
	u8 channel_avail : 1;
	u8 rx_nss : 4;
	u8 reserved : 1;
} __ATTRIB_PACK__ nan_ulw_control;

typedef struct _nan_unaligned_sched_attr {
	u8 attribute_id;
	u16 len;
	nan_unaligned_attr_ctrl attribute_ctrl;
	u32 start_time;
	u32 duration;
	u32 period;
	u8 count_down;
	nan_ulw_overwrite ulw_overwrite;
	u8 optional[0];
} __ATTRIB_PACK__ nan_unaligned_sched_attr;

/*Function declaration*/
tlv_header *nan_get_nan_attr(u8 attr, u8 *buffer, int size);

tlv_header *nan_get_nan_attr_second(u8 attr, u8 *buffer, int size);

void nan_get_optional_ndp_ext_attr(nan_ndp_attr *ndp_attr, u8 **tlv_list,
				   u16 *total_tlv_len);
enum nan_error nan_validate_ndp_request(struct mwu_iface_info *cur_if,
					u8 *buffer, int size);
enum nan_error nan_handle_ndp_req(struct mwu_iface_info *cur_if,
				  unsigned char *buffer, int size);
enum nan_error nan_handle_ndp_resp(struct mwu_iface_info *cur_if,
				   unsigned char *buffer, int size);
enum nan_error nan_handle_ndp_confirm(struct mwu_iface_info *cur_if,
				      unsigned char *buffer, int size);
enum nan_error nan_send_ndp_confirm(struct mwu_iface_info *cur_if,
				    nan_ndl_attr *ndl_attr,
				    nan_ndp_attr *ndp_attr, u32 ndl_status,
				    u32 ndp_status);
enum nan_error nan_send_ndp_terminate(struct mwu_iface_info *cur_if);
enum nan_error nan_send_bcast_schedule_update(struct mwu_iface_info *cur_if);
enum nan_error nan_send_security_install(struct mwu_iface_info *cur_if);
enum nan_error nan_handle_security_install(struct mwu_iface_info *cur_if,
					   unsigned char *buffer, int size);
enum nan_error nan_send_ndp_resp(struct mwu_iface_info *cur_if,
				 nan_ndp_req *req, u8 ndl_status, u8 ndp_status,
				 u8 reason);
enum nan_error nan_send_ndp_req(struct mwu_iface_info *cur_if, nan_ndp_req *req,
				int *ndp_req_id);
enum nan_error nan_parse_availability(nan_availability_attr *attr,
				      peer_availability_info *peer_info);
void nan_send_data_confirm_event(struct mwu_iface_info *cur_if,
				 unsigned char *buffer, int size);
int nan_parse_generic_time_bitmap(time_bitmap_control *ctrl, u8 bitmap_len,
				  u8 *bitmap,
				  u32 time_bitmap[MAX_SCHEDULE_ENTRIES]);
extern u16 ndp_get_chan_bitmap(int op_class, int chan);
extern enum nan_error nancmd_set_final_bitmap(struct mwu_iface_info *cur_if,
					      u8 fav_type);
enum nan_error nan_parse_ndp_req(struct mwu_iface_info *cur_if, u8 *ndl_status,
				 u8 *ndp_status, u8 *reason);
enum nan_error nan_generate_validate_pmkid(const u8 *pmk, size_t pmk_len,
					   const u8 *IAddr, const u8 *RAddr,
					   u8 *nan_pmkid, u8 *service_id,
					   u8 *m1_buf, int m1_size);
void nan_clear_peer_avail_published_entries(struct mwu_iface_info *cur_if);
void nan_clear_peer_avail_entries(struct mwu_iface_info *cur_if);
void nan_clear_avail_entries(struct mwu_iface_info *cur_if);
enum nan_error nan_handle_schedule_req(struct mwu_iface_info *cur_if,
				       unsigned char *buffer, int size);
enum nan_error nan_parse_schedule_req(struct mwu_iface_info *cur_if,
				      u8 *ndl_status, u8 *ndp_status,
				      u8 *reason);
enum nan_error nan_send_schedule_resp(struct mwu_iface_info *cur_if,
				      nan_ndp_req *req, u8 ndl_status,
				      u8 ndp_status, u8 reason);
enum nan_error nan_handle_schedule_resp(struct mwu_iface_info *cur_if,
					unsigned char *buffer, int size);
enum nan_error nan_handle_schedule_confirm(struct mwu_iface_info *cur_if,
					   unsigned char *buffer, int size);
enum nan_error nan_send_schedule_req(struct mwu_iface_info *cur_if,
				     char responder_mac[ETH_ALEN]);
enum nan_error nan_send_schedule_update(struct mwu_iface_info *cur_if,
					nan_ulw_param *ulw, char mac[ETH_ALEN]);
enum nan_error nan_handle_schedule_update(struct mwu_iface_info *cur_if,
					  unsigned char *buffer, int size);
void nan_get_ipv6_link_local_tlv(struct mwu_iface_info *cur_if,
				 ipv6_link_local_tlv *ipv6_tlv);
void nan_send_ndpe_data_event(struct mwu_iface_info *cur_if,
			      ipv6_link_local_tlv *ipv6_tlv,
			      transport_port_sub_attr *tport_sub_attr,
			      transport_protocol_sub_attr *tprotocol_sub_attr);

#endif //__DATA_ENGINE_H__
