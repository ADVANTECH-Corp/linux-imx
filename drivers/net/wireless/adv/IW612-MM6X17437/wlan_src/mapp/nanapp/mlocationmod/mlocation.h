#ifndef __MLOCATION_H__
#define __MLOCATION_H__

#include "util.h"
#include "module.h"
#include "wlan_hostcmd.h" // for mrvl_cmd_head_buf

#define RADIO_MEASUREMENT_CATEGORY_PUBLIC_ACTION_FRAME 5
#define MEASUREMENT_REQUEST_ELEMENT_ID 38
#define MEASUREMENT_REPORT_ELEMENT_ID 39
#define FTM_RANGE_REQUEST 16
#define LCI_REQUEST_SUBELEMENT_ID 8
#define CIVIC_REQUEST_SUBELEMENT_ID 11
#define FTM_TLV_HEADER_LENGTH 2

#define RADIO_MEASUREMENT_RANGE_REQUEST_SET 1
#define RADIO_MEASUREMENT_CIVIC_REQUEST_SET 2
#define RADIO_MEASUREMENT_LCI_REQUEST_SET 4

#define GAS_INITIAL_REQUEST 0xa
#define GAS_INITIAL_RESPONSE 0xb
#define ADVERTISEMENT_PROTO 0x6c
#define GAS_COMEBACK_REQUEST 0x0c
#define GAS_COMEBACK_RESPONSE 0x0d

#define EXT_CAP_CIVIC 14
#define EXT_CAP_LCI 15
#define EXT_CAP_INTERWORKING 31

#define MEASUREMENT_REQUEST 0x26

/** 16 bits byte swap */
#define swap_byte_16(x)                                                        \
	((u16)((((u16)(x)&0x00ffU) << 8) | (((u16)(x)&0xff00U) >> 8)))

#define swap_byte_24(x) (((((x)&0x00ffffU) << 16) | (((x)&0xffff00U) >> 16)))

/** 32 bits byte swap */
#define swap_byte_32(x)                                                        \
	((u32)((((u32)(x)&0x000000ffUL) << 24) |                               \
	       (((u32)(x)&0x0000ff00UL) << 8) |                                \
	       (((u32)(x)&0x00ff0000UL) >> 8) |                                \
	       (((u32)(x)&0xff000000UL) >> 24)))

#ifndef BIG_ENDIAN_SUPPORT
/** Do nothing */
#define le16_to_cpu(x) x
/** Do nothing */
#define le32_to_cpu(x) x
/** Do nothing */
#define le64_to_cpu(x) x
/** Do nothing */
#define cpu_to_le16(x) x
/** Do nothing */
#define cpu_to_le32(x) x
#else
/** CPU to little-endian convert for 16-bit */
#define cpu_to_le16(x) swap_byte_16(x)
/** CPU to little-endian convert for 32-bit */
#define cpu_to_le32(x) swap_byte_32(x)
/** Little-endian to CPU convert for 16-bit */
#define le16_to_cpu(x) swap_byte_16(x)
/** Little-endian to CPU convert for 32-bit */
#define le32_to_cpu(x) swap_byte_32(x)
#endif

#define MAX_CA_VALUES 256
enum mlocation_event_type {
	MLOCATION_EVENT_SESSION_COMPLETE = 0,
	MLOCATION_RADIO_REQUEST_RECEIVED,
	MLOCATION_RADIO_REPORT_RECEIVED,
	MLOCATION_ANQP_RESP_RECEIVED,
};

enum radio_measurement_action {
	RADIO_MEASUREMENT_REQUEST = 0,
	RADIO_MEASUREMENT_REPORT,
	LINK_MEASUREMENT_REQUEST,
	LINK_MEASUREMENT_REPORT,
	NEIGHBOR_REPORT_REQUEST,
	NEIGHBOR_REPORT_RESPONSE,
};

typedef struct _action_frame_header {
	u8 category;
	u8 action;
} __ATTRIB_PACK__ action_frame_header;

typedef struct _gas_initial_resp {
	u8 category;
	u8 action;
	u8 DialogToken;
	u8 Status;
	u16 GasComebackDelay;
	u8 AdvtProtocolIE;
	u16 QueryRspLen;
	u8 QueryRsp[0];
} __ATTRIB_PACK__ gas_initial_resp;

typedef struct _gas_comeback_resp {
	u8 category;
	u8 action;
	u8 DialogToken;
	u8 Status;
	u8 frag_id;
	u16 GasComebackDelay;
	u8 AdvtProtocolIE;
	u16 QueryRspLen;
	u8 QueryRsp[0];
} __ATTRIB_PACK__ gas_comeback_resp;

typedef union _gas_frames {
	action_frame_header hdr;
	gas_initial_resp init_resp;
	gas_comeback_resp cbk_resp;
} __ATTRIB_PACK__ gas_frames;

struct mlocation_cfg {
	u8 burst_exp;
	u8 burst_duration;
	u8 min_delta;
	u8 asap;
	u8 mlocation_per_burst;
	u8 bw;
	u16 burst_period;
	u8 format_bw;
	u8 max_i2r_sts_upto80;
	u8 max_r2i_sts_upto80;
	u8 az_measurement_freq;
	u8 az_number_of_measurements;
	u8 protocol_type;
	u8 civic_location;
	u8 civic_location_type;
	u8 country_code;
	u8 ca_type;
	u8 ca_length;
	u8 lci_request;
	double latitude;
	double longitude;
	double altitude;
	u8 lat_unc;
	u8 long_unc;
	u8 alt_unc;
	char ca_value[MAX_CA_VALUES];
};

typedef struct _mlocation_anqp_cfg {
	u32 civic;
	u32 lci;
	u8 mac[ETH_ALEN];
	u8 channel;
	u32 fqdn;
} mlocation_anqp_cfg;

typedef struct _mlocation_event {
	u8 bssNum;
	u8 bssType;
	u8 mac_address[ETH_ALEN];
	u32 AverageRTT;
	u32 AverageClockOffset;
	u32 meas_start_time;
	u8 civic_location;
	u8 civic_location_type;
	u8 country_code;
	u8 ca_type;
	u8 ca_length;
	char ca_value[MAX_CA_VALUES];
} __ATTRIB_PACK__ mlocation_event;

typedef struct _mlocation_radio_receive_event {
	u32 tsf_low;
	u8 bssid[6];
	u8 len;
	u8 buffer[0];
} __ATTRIB_PACK__ mlocation_radio_receive_event;

typedef struct mlocation_radio_report_event {
	u32 distance;
	u8 mac[6];
} __ATTRIB_PACK__ mlocation_radio_report_event;

typedef struct _mlocation_anqp_resp_event {
	u8 mac[6];
	u8 len;
	u8 buffer[0];
} __ATTRIB_PACK__ mlocation_anqp_resp_event;

typedef struct _mlocation_generic_event {
	u8 event_subtype;
	union {
		mlocation_radio_receive_event rm_evt;
		mlocation_event mlocation_event;
		mlocation_radio_report_event rm_rpt;
		mlocation_anqp_resp_event anqp_rsp;
	} __ATTRIB_PACK__ u;
} __ATTRIB_PACK__ mlocation_generic_event;

struct mlocation_session {
	u16 action;
	unsigned char mac[ETH_ALEN];
	u8 civic_req;
	u8 lci_req;
	u8 channel;
};

typedef struct _mlocation_session_config {
	u8 burst_exponent;
	u8 burst_duration;
	u8 min_delta_mlocation;
	u8 asap;
	u8 mlocation_per_burst;
	u8 chan_spacing;
	u16 burst_period;
	u8 civic_location;
	u8 civic_location_type;
	u8 country_code;
	u8 ca_type;
	u8 ca_length;
	u8 lci_request;
	double latitude;
	double longitude;
	u8 lat_unc;
	u8 long_unc;
	double altitude;
	u8 alt_unc;
	u8 ca_value[MAX_CA_VALUES];
} __ATTRIB_PACK__ mlocation_session_config;

typedef struct _mlocation_session_ctrl { /** Action */
	u16 action; /* 1 = Start MLOCATION session; 2 = Abort ongoing MLOCATION
		       session; */
	u8 ftm_for_nan_ranging;
	u8 peer_mac[ETH_ALEN];
	u8 channel;
} __ATTRIB_PACK__ mlocation_session_ctrl;

typedef struct _mlocation_neighbor_req { /** Action */
	u16 action;
	u8 dialog_token;
	u8 lci_req;
	u8 loc_civic_req;
} __ATTRIB_PACK__ mlocation_neighbor_req;

typedef struct _mlocation_init_tlv {
	u16 tag;
	u16 len;
	u8 burst_exponent;
	u8 burst_duration;
	u8 min_delta_mlocation;
	u8 asap;
	u8 mlocation_per_burst;
	u8 chan_spacing;
	u16 burst_period;
	u8 civic_req;
	u8 lci_req;
} __ATTRIB_PACK__ mlocation_init_tlv;

typedef struct _mlocation_Ranging_NTB_Params_tlv {
	u16 tag;
	u16 len;
	u8 format_bw;
	u8 max_i2r_sts_upto80;
	u8 max_r2i_sts_upto80;
	u8 az_measurement_freq;
	u8 az_number_of_measurements;
	u8 civic_req;
	u8 lci_req;
} __ATTRIB_PACK__ mlocation_Ranging_NTB_Params_tlv;

typedef struct _mlocation_location_civic_tlv {
	u16 tag;
	u16 len;
	u8 civic_location_type;
	u16 country_code;
	u8 civic_address_type;
	u8 civic_address_length;
	u8 civic_address[0];
} __ATTRIB_PACK__ mlocation_location_civic_tlv;

typedef struct _mlocation_session_cfg_lci_tlv {
	u16 tag;
	u16 len;
	double longitude;
	double latitude;
	double altitude;
	u8 lat_unc;
	u8 long_unc;
	u8 alt_unc;
	u32 zinfo;
} __ATTRIB_PACK__ mlocation_session_cfg_lci_tlv;

typedef struct _mlocation_cmd_session_config {
	/** Action */
	u16 action; /* 1 = Start MLOCATION session; 2 = Abort ongoing MLOCATION
		       session; */
	u8 tlvs[0];
} __ATTRIB_PACK__ mlocation_cmd_session_config;

/* structures for action frames */

typedef struct _report_mode_t {
	u8 late : 1;
	u8 incapable : 1;
	u8 refused : 1;
	u8 reserved : 5;
} __ATTRIB_PACK__ report_mode_t;

typedef struct _request_mode_t {
	u8 parallel : 1;
	u8 enable : 1;
	u8 request : 1;
	u8 report : 1;
	u8 duration : 1;
	u8 reserved : 3;
} __ATTRIB_PACK__ request_mode_t;

typedef struct _location_request_element {
	u8 element_id;
	u8 length;
	u8 token;
	request_mode_t request_mode;
	u8 type;
	u8 request[0];
} __ATTRIB_PACK__ location_request_element;

typedef struct _ftm_range_request_element {
	u16 random_interval;
	u8 min_ap_count;
	u8 ftm_req_subelements[0];
} __ATTRIB_PACK__ ftm_range_request_element;

typedef struct _range_entry_feild {
	u32 measurement_start_time;
	u8 bssid[6];
	u32 range : 24;
	u8 max_range_exponent;
	u8 reserved;
} __ATTRIB_PACK__ range_entry_feild;

typedef struct _error_entry_feild {
	u32 measurement_start_time;
	u8 bssid[6];
	u8 error_code;
} __ATTRIB_PACK__ error_entry_feild;

typedef struct _ftm_range_response_element {
	u8 entry_count;
	range_entry_feild range_entry[255];
	u8 error_entry_count;
	error_entry_feild error_entry[255];
	u8 ftm_res_subelements[0];
} __ATTRIB_PACK__ ftm_range_response_element;

typedef struct _location_response_element {
	u8 element_id;
	u8 length;
	u8 token;
	report_mode_t report_mode;
	u8 type;
	u8 report[0];
} __ATTRIB_PACK__ location_response_element;

typedef struct _location_request_frame {
	u8 mac[6];
	u8 category;
	u8 action;
	u8 dialog_token;
	u16 repeatations;
	u8 ie_list[0];
} __ATTRIB_PACK__ location_request_frame;

typedef struct _location_anqp_frame {
	u8 mac[6];
	u8 category;
	u8 action;
	u8 dialog_token;
	u8 channel;
	u8 ie_list[0];
} __ATTRIB_PACK__ location_anqp_frame;

typedef struct _location_response_frame {
	u8 mac[6];
	u8 category;
	u8 action;
	u8 dialog_token;
	u8 ie_list[0];
} __ATTRIB_PACK__ location_response_frame;

/* Neighbor Report element*/
typedef struct _neighbor_request_frame {
	u8 mac[6];
	u8 category;
	u8 action;
	u8 dialog_token;
	u8 optional[0];
} __ATTRIB_PACK__ neighbor_request_frame;

typedef struct _bss_capab {
	u8 spectrumMgmt : 1;
	u8 QoS : 1;
	u8 apsd : 1;
	u8 radioMesurement : 1;
	u8 delayedBlockAck : 1;
	u8 immeBlockAck : 1;
} __ATTRIB_PACK__ bss_capab;

typedef struct _bssid_info {
	u8 APreachability : 2;
	u8 security : 1;
	u8 keyScope : 1;
	bss_capab capabilities;
	u8 mobilityDomain : 1;
	u8 highThroughput : 1;
	u8 veryhighThroughput : 1;
	u8 FTM : 1;
	u32 reserved : 18;
} __ATTRIB_PACK__ bssid_info;

typedef struct _neighbor_request_element {
	u8 tag;
	u8 length;
	u8 bssid[6];
	bssid_info bssid_info;
	u8 operating_class;
	u8 channel;
	u8 phy_type;
	u8 optionalSubelements;
} __ATTRIB_PACK__ neighbor_request_element;

typedef struct _ftm_range_req {
	u8 bssid[6];
	u8 channel;
	u8 bandwidth;
	u32 start_time;
	u32 range;
} __ATTRIB_PACK__ ftm_range_request;

typedef struct _ftm_lci_request {
	u8 subject;
	u8 subelement_id;
	u8 length;
	u8 data[0];
} __ATTRIB_PACK__ ftm_lci_request;

typedef struct _ftm_lci_report {
	u8 element_id;
	u8 length;
	u8 lci_feild[0];
} __ATTRIB_PACK__ ftm_lci_report;

typedef struct _ftm_lci_info {
	u8 lat_unc : 6;
	u64 latitude : 34;
	u8 long_unc : 6;
	u64 longitude : 34;
	u8 alt_type : 4;
	u8 alt_unc : 6;
	u64 altitude : 30;
	u8 datum : 3;
	u8 regloc : 1;
	u8 dependent_sta : 1;
	u8 version : 2;
} __ATTRIB_PACK__ ftm_lci_info;
/* Measurement Request Element format
typedef struct _measurement_reqmode
{
    u8  parallel:1;
    u8  enable:1;
    u8  request:1;
    u8  report:1;
    u8  duration:1;
    u8  reserved:3;
} __ATTRIB_PACK__ measurement_reqmode;

typedef struct _measurement_request_element
{
    u8  tag;
    u8  length;
    u8  measurement_token;
    measurement_reqmode reqmode;
    u8  type;
    u8  request[0];
} __ATTRIB_PACK__ measurement_request_element;
*/

void nan_send_ftm_complete_event(mlocation_event *dev, char iface[],
				 float distance);

#endif
