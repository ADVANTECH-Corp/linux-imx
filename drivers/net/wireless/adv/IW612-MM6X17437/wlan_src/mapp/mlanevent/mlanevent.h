/** @file  mlanevent.h
 *
 *  @brief Header file for mlanevent application
 *
 *
 * Copyright 2008-2021 NXP
 *
 * NXP CONFIDENTIAL
 * The source code contained or described herein and all documents related to
 * the source code (Materials) are owned by NXP, its
 * suppliers and/or its licensors. Title to the Materials remains with NXP,
 * its suppliers and/or its licensors. The Materials contain
 * trade secrets and proprietary and confidential information of NXP, its
 * suppliers and/or its licensors. The Materials are protected by worldwide
 * copyright and trade secret laws and treaty provisions. No part of the
 * Materials may be used, copied, reproduced, modified, published, uploaded,
 * posted, transmitted, distributed, or disclosed in any way without NXP's prior
 * express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by NXP in writing.
 *
 */
/************************************************************************
Change log:
    03/18/08: Initial creation
************************************************************************/

#ifndef _MLAN_EVENT_H
#define _MLAN_EVENT_H

/** Character, 1 byte */
typedef signed char t_s8;
/** Unsigned character, 1 byte */
typedef unsigned char t_u8;

/** Short integer */
typedef signed short t_s16;
/** Unsigned short integer */
typedef unsigned short t_u16;

/** Integer */
typedef signed int t_s32;
/** Unsigned integer */
typedef unsigned int t_u32;

/** Long long integer */
typedef long long t_s64;
/** Unsigned long long integer */
typedef unsigned long long t_u64;

#if (BYTE_ORDER == LITTLE_ENDIAN)
#undef BIG_ENDIAN_SUPPORT
#endif

/** 16 bits byte swap */
#define swap_byte_16(x)                                                        \
	((t_u16)((((t_u16)(x)&0x00ffU) << 8) | (((t_u16)(x)&0xff00U) >> 8)))

/** 32 bits byte swap */
#define swap_byte_32(x)                                                        \
	((t_u32)((((t_u32)(x)&0x000000ffUL) << 24) |                           \
		 (((t_u32)(x)&0x0000ff00UL) << 8) |                            \
		 (((t_u32)(x)&0x00ff0000UL) >> 8) |                            \
		 (((t_u32)(x)&0xff000000UL) >> 24)))

/** 64 bits byte swap */
#define swap_byte_64(x)                                                        \
	((t_u64)((t_u64)(((t_u64)(x)&0x00000000000000ffULL) << 56) |           \
		 (t_u64)(((t_u64)(x)&0x000000000000ff00ULL) << 40) |           \
		 (t_u64)(((t_u64)(x)&0x0000000000ff0000ULL) << 24) |           \
		 (t_u64)(((t_u64)(x)&0x00000000ff000000ULL) << 8) |            \
		 (t_u64)(((t_u64)(x)&0x000000ff00000000ULL) >> 8) |            \
		 (t_u64)(((t_u64)(x)&0x0000ff0000000000ULL) >> 24) |           \
		 (t_u64)(((t_u64)(x)&0x00ff000000000000ULL) >> 40) |           \
		 (t_u64)(((t_u64)(x)&0xff00000000000000ULL) >> 56)))

#ifdef BIG_ENDIAN_SUPPORT
/** Convert from 16 bit little endian format to CPU format */
#define uap_le16_to_cpu(x) swap_byte_16(x)
/** Convert from 32 bit little endian format to CPU format */
#define uap_le32_to_cpu(x) swap_byte_32(x)
/** Convert from 64 bit little endian format to CPU format */
#define uap_le64_to_cpu(x) swap_byte_64(x)
/** Convert to 16 bit little endian format from CPU format */
#define uap_cpu_to_le16(x) swap_byte_16(x)
/** Convert to 32 bit little endian format from CPU format */
#define uap_cpu_to_le32(x) swap_byte_32(x)
/** Convert to 64 bit little endian format from CPU format */
#define uap_cpu_to_le64(x) swap_byte_64(x)
#else /* BIG_ENDIAN_SUPPORT */
/** Do nothing */
#define uap_le16_to_cpu(x) x
/** Do nothing */
#define uap_le32_to_cpu(x) x
/** Do nothing */
#define uap_le64_to_cpu(x) x
/** Do nothing */
#define uap_cpu_to_le16(x) x
/** Do nothing */
#define uap_cpu_to_le32(x) x
/** Do nothing */
#define uap_cpu_to_le64(x) x
#endif /* BIG_ENDIAN_SUPPORT */

/** Convert WPS TLV header from network to host order */
#define endian_convert_tlv_wps_header_in(t, l)                                 \
	{                                                                      \
		(t) = ntohs(t);                                                \
		(l) = ntohs(l);                                                \
	}

/**
 * Hex or Decimal to Integer
 * @param   num string to convert into decimal or hex
 */
#define A2HEXDECIMAL(num)                                                      \
	(strncasecmp("0x", (num), 2) ? (unsigned int)strtoll((num), NULL, 0) : \
				       a2hex((num)))

/**
 * Check of decimal or hex string
 * @param   num string
 */
#define IS_HEX_OR_DIGIT(num)                                                   \
	(strncasecmp("0x", (num), 2) ? ISDIGIT((num)) : ishexstring((num)))

/** MLan Event application version string */
#define MLAN_EVENT_VERSION "MlanEvent 2.0"

/** Failure */
#define MLAN_EVENT_FAILURE -1

#ifdef __GNUC__
/** Structure packing begins */
#define PACK_START
/** Structure packeing end */
#define PACK_END __attribute__((packed))
#else
/** Structure packing begins */
#define PACK_START __packed
/** Structure packeing end */
#define PACK_END
#endif

#ifndef ETH_ALEN
/** MAC address length */
#define ETH_ALEN 6
#endif

/** Netlink protocol number */
#define NETLINK_NXP (MAX_LINKS - 1)
/** Netlink maximum payload size */
#define NL_MAX_PAYLOAD (3 * 1024)
/** Netlink multicast group number */
#define NL_MULTICAST_GROUP 1
/** Default wait time in seconds for events */
#define UAP_RECV_WAIT_DEFAULT 10
/** Maximum number of devices */
#define MAX_NO_OF_DEVICES 6

#ifndef NLMSG_HDRLEN
/** NL message header length */
#define NLMSG_HDRLEN ((int)NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#endif

/** Event ID mask */
#define EVENT_ID_MASK 0xffff

/** BSS number mask */
#define BSS_NUM_MASK 0xf

/** Get BSS number from event cause (bit 23:16) */
#define EVENT_GET_BSS_NUM(event_cause) (((event_cause) >> 16) & BSS_NUM_MASK)

/** Invitation Flag mask */
#define INVITATION_FLAG_MASK 0x01

/* Event buffer */
typedef PACK_START struct _evt_buf {
	/** Flag to check if event data is present in the buffer or not  */
	int flag;
	/** Event length */
	int length;
	/** Event data */
	t_u8 buffer[NL_MAX_PAYLOAD];
} PACK_END evt_buf;

/** Event header */
typedef PACK_START struct _event_header {
	/** Event ID */
	t_u32 event_id;
	/** Event data */
	t_u8 event_data[];
} PACK_END event_header;

/** Event ID length */
#define EVENT_ID_LEN 4

/** Event ID : WMM status change */
#define MICRO_AP_EV_WMM_STATUS_CHANGE 0x00000017

/** Event ID: STA deauth */
#define MICRO_AP_EV_ID_STA_DEAUTH 0x0000002c

/** Event ID: STA associated */
#define MICRO_AP_EV_ID_STA_ASSOC 0x0000002d

/** Event ID: BSS started */
#define MICRO_AP_EV_ID_BSS_START 0x0000002e

/** Event ID: Debug event */
#define MICRO_AP_EV_ID_DEBUG 0x00000036

/** Event ID: BSS idle event */
#define MICRO_AP_EV_BSS_IDLE 0x00000043

/** Event ID: BSS active event */
#define MICRO_AP_EV_BSS_ACTIVE 0x00000044

/** Event ID: WEP ICV error */
#define EVENT_WEP_ICV_ERROR 0x00000046

#ifdef WIFI_DIRECT_SUPPORT
/** Event ID: UAP,STA wifidirect generic event */
#define EVENT_WIFIDIRECT_GENERIC 0x00000049

/** Event ID: UAP,STA wifidirect service discovery event */
#define EVENT_WIFIDIRECT_SERVICE_DISCOVERY 0x0000004a
#endif

/** Event ID: MIC Countermeasures event */
#define MICRO_AP_EV_ID_MIC_COUNTERMEASURES 0x0000004c

/** Event ID: RSN Connect event */
#define MICRO_AP_EV_RSN_CONNECT 0x00000051

/** Event ID: TDLS generic event */
#define EVENT_TDLS_GENERIC 0x00000052

/** event type for tdls setup failure */
#define TDLS_EVENT_TYPE_SETUP_FAILURE 1
/** event type for tdls setup request received */
#define TDLS_EVENT_TYPE_SETUP_REQ_RCVD 2
/** event type for tdls link torn down */
#define TDLS_EVENT_TYPE_LINK_TORN_DOWN 3
/** event type for tdls link established */
#define TDLS_EVENT_TYPE_LINK_ESTABLISHED 4
/** event type for tdls debug */
#define TDLS_EVENT_TYPE_DEBUG 5
/** event type for tdls packet */
#define TDLS_EVENT_TYPE_PACKET 6
/** event type for channel switch result */
#define TDLS_EVENT_TYPE_CHAN_SWITCH_RESULT 7
/** event type for start channel switch */
#define TDLS_EVENT_TYPE_START_CHAN_SWITCH 8
/** event type for stop channel switch */
#define TDLS_EVENT_TYPE_CHAN_SWITCH_STOPPED 9

/** Event ID: Radar Detected */
#define EVENT_RADAR_DETECTED 0x00000053
/** Event ID: Channel Report Ready */
#define EVENT_CHANNEL_REPORT_RDY 0x00000054

/** HS WAKE UP event id */
#define UAP_EVENT_ID_HS_WAKEUP 0x80000001
/** HS_ACTIVATED event id */
#define UAP_EVENT_ID_DRV_HS_ACTIVATED 0x80000002
/** HS DEACTIVATED event id */
#define UAP_EVENT_ID_DRV_HS_DEACTIVATED 0x80000003
/** HOST SLEEP AWAKE  event id in legacy PS*/
#define UAP_EVENT_HOST_SLEEP_AWAKE 0x00000012

/** HS WAKE UP event id */
#define UAP_EVENT_ID_DRV_MGMT_FRAME 0x80000005

/** SCAN REPORT Event id */
#define EVENT_ID_DRV_SCAN_REPORT 0x80000009

#define EVENT_IMD3_CAL_START 0x000000A0
#define EVENT_IMD3_CAL_END 0x000000A1

/** WPA IE Tag */
#define IEEE_WPA_IE 221
/** RSN IE Tag */
#define IEEE_RSN_IE 48

/** TLV ID : WAPI Information */
#define MRVL_WAPI_INFO_TLV_ID 0x0167

/** TLV ID : Management Frame */
#define MRVL_MGMT_FRAME_TLV_ID 0x0168

/** TLV Id : Channel Config */
#define MRVL_CHANNELCONFIG_TLV_ID 0x012a

/** Assoc Request */
#define SUBTYPE_ASSOC_REQUEST 0
/** Assoc Response */
#define SUBTYPE_ASSOC_RESPONSE 1
/** ReAssoc Request */
#define SUBTYPE_REASSOC_REQUEST 2
/** ReAssoc Response */
#define SUBTYPE_REASSOC_RESPONSE 3
/** WEP key user input length */
#define WEP_KEY_USER_INPUT 13

/** TLV buffer header*/
typedef PACK_START struct _tlvbuf_header {
	/** Header type */
	t_u16 type;
	/** Header length */
	t_u16 len;
} PACK_END tlvbuf_header;

/** Event body : STA deauth */
typedef PACK_START struct _eventbuf_sta_deauth {
	/** Deauthentication reason */
	t_u16 reason_code;
	/** MAC address of deauthenticated STA */
	t_u8 sta_mac_address[ETH_ALEN];
} PACK_END eventbuf_sta_deauth;

/** Event body : WEP ICV error */
typedef PACK_START struct _eventbuf_wep_icv_error {
	/** Deauthentication reason */
	t_u16 reason_code;
	/** MAC address of deauthenticated STA */
	t_u8 sta_mac_address[ETH_ALEN];
	/** WEP key index */
	t_u8 wep_key_index;
	/** WEP key length */
	t_u8 wep_key_length;
	/** WEP key */
	t_u8 wep_key[WEP_KEY_USER_INPUT];
} PACK_END eventbuf_wep_icv_error;

/** Event body : STA associated */
typedef PACK_START struct _eventbuf_sta_assoc {
	/** Reserved */
	t_u8 reserved[2];
	/** MAC address of associated STA */
	t_u8 sta_mac_address[ETH_ALEN];
	/** Assoc request/response buffer */
	t_u8 assoc_payload[];
} PACK_END eventbuf_sta_assoc;

/** Event body : RSN Connect */
typedef PACK_START struct _eventbuf_rsn_connect {
	/** Reserved */
	t_u8 reserved[2];
	/** MAC address of Station */
	t_u8 sta_mac_address[ETH_ALEN];
	/** WPA/WPA2 TLV IEs */
	t_u8 tlv_list[];
} PACK_END eventbuf_rsn_connect;

/** Event body : BSS started */
typedef PACK_START struct _eventbuf_bss_start {
	/** Reserved */
	t_u8 reserved[2];
	/** MAC address of BSS */
	t_u8 ap_mac_address[ETH_ALEN];
} PACK_END eventbuf_bss_start;

/** Event body : MIC Countermeasures */
typedef PACK_START struct _eventbuf_mic_countermeasures {
	/** Status */
	t_u16 status;
} PACK_END eventbuf_mic_countermeasures;

/**
 *                 IEEE 802.11 MAC Message Data Structures
 *
 * Each IEEE 802.11 MAC message includes a MAC header, a frame body (which
 * can be empty), and a frame check sequence field. This section gives the
 * structures that used for the MAC message headers and frame bodies that
 * can exist in the three types of MAC messages - 1) Control messages,
 * 2) Data messages, and 3) Management messages.
 */
typedef PACK_START struct _IEEEtypes_FrameCtl_t {
	/** Protocol Version */
	t_u8 protocol_version : 2;
	/** Type */
	t_u8 type : 2;
	/** Sub Type */
	t_u8 sub_type : 4;
	/** To DS */
	t_u8 to_ds : 1;
	/** From DS */
	t_u8 from_ds : 1;
	/** More Frag */
	t_u8 more_frag : 1;
	/** Retry */
	t_u8 retry : 1;
	/** Power Mgmt */
	t_u8 pwr_mgmt : 1;
	/** More Data */
	t_u8 more_data : 1;
	/** Wep */
	t_u8 wep : 1;
	/** Order */
	t_u8 order : 1;
} PACK_END IEEEtypes_FrameCtl_t;

/** IEEEtypes_AssocRqst_t */
typedef PACK_START struct _IEEEtypes_AssocRqst_t {
	/** Capability Info */
	t_u16 cap_info;
	/** Listen Interval */
	t_u16 listen_interval;
	/** IE Buffer */
	t_u8 ie_buffer[];
} PACK_END IEEEtypes_AssocRqst_t;

/** IEEEtypes_AssocRsp_t */
typedef PACK_START struct _IEEEtypes_AssocRsp_t {
	/** Capability Info */
	t_u16 cap_info;
	/** Status Code */
	t_u16 status_code;
	/** AID */
	t_u16 aid;
} PACK_END IEEEtypes_AssocRsp_t;

/** IEEEtypes_ReAssocRqst_t */
typedef PACK_START struct _IEEEtypes_ReAssocRqst_t {
	/** Capability Info */
	t_u16 cap_info;
	/** Listen Interval */
	t_u16 listen_interval;
	/** Current AP Address */
	t_u8 current_ap_addr[ETH_ALEN];
	/** IE Buffer */
	t_u8 ie_buffer[];
} PACK_END IEEEtypes_ReAssocRqst_t;

/** channel band */
enum {
	BAND_2GHZ = 0,
	BAND_5GHZ = 1,
	BAND_6GHZ = 2,
	BAND_4GHZ = 3,
};

/** channel offset */
enum {
	SEC_CHAN_NONE = 0,
	SEC_CHAN_ABOVE = 1,
	SEC_CHAN_5MHZ = 2,
	SEC_CHAN_BELOW = 3
};

/** channel bandwidth */
enum {
	CHAN_BW_20MHZ = 0,
	CHAN_BW_10MHZ,
	CHAN_BW_40MHZ,
	CHAN_BW_80MHZ,

};

/** scan mode */
enum {
	SCAN_MODE_MANUAL = 0,
	SCAN_MODE_ACS,
	SCAN_MODE_USER,
};

/** Band_Config_t */
typedef PACK_START struct _Band_Config_t {
#ifdef BIG_ENDIAN_SUPPORT
	/** Channel Selection Mode - (00)=manual, (01)=ACS,  (02)=user*/
	t_u8 scanMode : 2;
	/** Secondary Channel Offset - (00)=None, (01)=Above, (11)=Below */
	t_u8 chan2Offset : 2;
	/** Channel Width - (00)=20MHz, (10)=40MHz, (11)=80MHz */
	t_u8 chanWidth : 2;
	/** Band Info - (00)=2.4GHz, (01)=5GHz */
	t_u8 chanBand : 2;
#else
	/** Band Info - (00)=2.4GHz, (01)=5GHz */
	t_u8 chanBand : 2;
	/** Channel Width - (00)=20MHz, (10)=40MHz, (11)=80MHz */
	t_u8 chanWidth : 2;
	/** Secondary Channel Offset - (00)=None, (01)=Above, (11)=Below */
	t_u8 chan2Offset : 2;
	/** Channel Selection Mode - (00)=manual, (01)=ACS, (02)=Adoption mode*/
	t_u8 scanMode : 2;
#endif
} PACK_END Band_Config_t;

/** TLV buffer : Channel Config */
typedef PACK_START struct _tlvbuf_channel_config {
	/** Type */
	t_u16 type;
	/** Length */
	t_u16 len;
	/** Band Configuration */
	Band_Config_t bandcfg;
	/** Channel number */
	t_u8 chan_number;
} PACK_END tlvbuf_channel_config;

/** MrvlIEtypes_WapiInfoSet_t */
typedef PACK_START struct _MrvlIEtypes_WapiInfoSet_t {
	/** Type */
	t_u16 type;
	/** Length */
	t_u16 len;
	/** Multicast PN */
	t_u8 multicast_PN[16];
} PACK_END MrvlIEtypes_WapiInfoSet_t;

/** MrvlIETypes_MgmtFrameSet_t */
typedef PACK_START struct _MrvlIETypes_MgmtFrameSet_t {
	/** Type */
	t_u16 type;
	/** Length */
	t_u16 len;
	/** Frame Control */
	IEEEtypes_FrameCtl_t frame_control;
	/** Frame Contents */
	t_u8 frame_contents[];
} PACK_END MrvlIETypes_MgmtFrameSet_t;

/** Debug Type : Event */
#define DEBUG_TYPE_EVENT 0
/** Debug Type : Info */
#define DEBUG_TYPE_INFO 1

/** Major debug id: Authenticator */
#define DEBUG_ID_MAJ_AUTHENTICATOR 1
/** Minor debug id: PWK1 */
#define DEBUG_MAJ_AUTH_MIN_PWK1 0
/** Minor debug id: PWK2 */
#define DEBUG_MAJ_AUTH_MIN_PWK2 1
/** Minor debug id: PWK3 */
#define DEBUG_MAJ_AUTH_MIN_PWK3 2
/** Minor debug id: PWK4 */
#define DEBUG_MAJ_AUTH_MIN_PWK4 3
/** Minor debug id: GWK1 */
#define DEBUG_MAJ_AUTH_MIN_GWK1 4
/** Minor debug id: GWK2 */
#define DEBUG_MAJ_AUTH_MIN_GWK2 5
/** Minor debug id: station reject */
#define DEBUG_MAJ_AUTH_MIN_STA_REJ 6
/** Minor debug id: EAPOL_TR */
#define DEBUG_MAJ_AUTH_MIN_EAPOL_TR 7

/** Major debug id: Assoicate agent */
#define DEBUG_ID_MAJ_ASSOC_AGENT 2
/** Minor debug id: WPA IE*/
#define DEBUG_ID_MAJ_ASSOC_MIN_WPA_IE 0
/** Minor debug id: station reject */
#define DEBUG_ID_MAJ_ASSOC_MIN_STA_REJ 1

/** ether_hdr */
typedef PACK_START struct {
	/** Dest address */
	t_u8 da[ETH_ALEN];
	/** Src address */
	t_u8 sa[ETH_ALEN];
	/** Header type */
	t_u16 type;
} PACK_END ether_hdr_t;

/** 8021x header */
typedef PACK_START struct {
	/** Protocol version*/
	t_u8 protocol_ver;
	/** Packet type*/
	t_u8 pckt_type;
	/** Packet len */
	t_u8 pckt_body_len;
} PACK_END hdr_8021x_t;

/** Nonce size */
#define NONCE_SIZE 32
/** Max WPA IE len */
#define MAX_WPA_IE_LEN 64
/** EAPOL mic size */
#define EAPOL_MIC_SIZE 16

/** EAPOL key message */
typedef PACK_START struct {
	/** Ether header */
	ether_hdr_t ether_hdr;
	/** 8021x header */
	hdr_8021x_t hdr_8021x;
	/** desc_type */
	t_u8 desc_type;
	/** Key info */
	t_u16 k;
	/** Key length */
	t_u16 key_length;
	/** Replay count */
	t_u32 replay_cnt[2];
	/** Key nonce */
	t_u8 key_nonce[NONCE_SIZE];
	/** Key IV */
	t_u8 eapol_key_iv[16];
	/** Key RSC */
	t_u8 key_rsc[8];
	/** Key ID */
	t_u8 key_id[8];
	/** Key MIC */
	t_u8 key_mic[EAPOL_MIC_SIZE];
	/** Key len */
	t_u16 key_material_len;
	/** Key data */
	t_u8 key_data[MAX_WPA_IE_LEN];
} PACK_END eapol_keymsg_debug_t;

/** Failure after receive EAPOL MSG2 PMK */
#define REJECT_STATE_FAIL_EAPOL_2 1
/** Failure after receive EAPOL MSG4 PMK*/
#define REJECT_STATE_FAIL_EAPOL_4 2
/** Failure after receive EAPOL Group MSG2 GWK */
#define REJECT_STATE_FAIL_EAPOL_GROUP_2 3

/** Fail reason: Invalid ie */
#define IEEEtypes_REASON_INVALID_IE 13
/** Fail reason: Mic failure */
#define IEEEtypes_REASON_MIC_FAILURE 14

/** Station reject */
typedef PACK_START struct {
	/** Reject state */
	t_u8 reject_state;
	/** Reject reason */
	t_u16 reject_reason;
	/** Station mac address */
	t_u8 sta_mac_addr[ETH_ALEN];
} PACK_END sta_reject_t;

/** wpa_ie */
typedef PACK_START struct {
	/** Station mac address */
	t_u8 sta_mac_addr[ETH_ALEN];
	/** WPA IE */
	t_u8 wpa_ie[MAX_WPA_IE_LEN];
} PACK_END wpaie_t;

/** Initial state of the state machine */
#define EAPOL_START 1
/** Sent eapol msg1, wait for msg2 from the client */
#define EAPOL_WAIT_PWK2 2
/** Sent eapol msg3, wait for msg4 from the client */
#define EAPOL_WAIT_PWK4 3
/** Sent eapol group key msg1, wait for group msg2 from the client */
#define EAPOL_WAIT_GTK2 4
/** Eapol handshake complete */
#define EAPOL_END 5

#ifdef WIFI_DIRECT_SUPPORT
/** TLV : WifiDirect status */
#define TLV_TYPE_WIFIDIRECT_STATUS 0x0000
/** TLV : WifiDirect param capability */
#define TLV_TYPE_WIFIDIRECT_CAPABILITY 0x0002
/** TLV : WifiDirect param device Id */
#define TLV_TYPE_WIFIDIRECT_DEVICE_ID 0x0003
/** TLV : WifiDirect param group owner intent */
#define TLV_TYPE_WIFIDIRECT_GROUPOWNER_INTENT 0x0004
/** TLV : WifiDirect param config timeout */
#define TLV_TYPE_WIFIDIRECT_CONFIG_TIMEOUT 0x0005
/** TLV : WifiDirect param channel */
#define TLV_TYPE_WIFIDIRECT_CHANNEL 0x0006
/** TLV : WifiDirect param group bssId */
#define TLV_TYPE_WIFIDIRECT_GROUP_BSS_ID 0x0007
/** TLV : WifiDirect param extended listen time */
#define TLV_TYPE_WIFIDIRECT_EXTENDED_LISTEN_TIME 0x0008
/** TLV : WifiDirect param intended address */
#define TLV_TYPE_WIFIDIRECT_INTENDED_ADDRESS 0x0009
/** TLV : WifiDirect param manageability */
#define TLV_TYPE_WIFIDIRECT_MANAGEABILITY 0x000a
/** TLV : WifiDirect param channel list */
#define TLV_TYPE_WIFIDIRECT_CHANNEL_LIST 0x000b
/** TLV : WifiDirect Notice of Absence */
#define TLV_TYPE_WIFIDIRECT_NOTICE_OF_ABSENCE 0x000c
/** TLV : WifiDirect param device Info */
#define TLV_TYPE_WIFIDIRECT_DEVICE_INFO 0x000d
/** TLV : WifiDirect param Group Info */
#define TLV_TYPE_WIFIDIRECT_GROUP_INFO 0x000e
/** TLV : WifiDirect param group Id */
#define TLV_TYPE_WIFIDIRECT_GROUP_ID 0x000f
/** TLV : WifiDirect param interface */
#define TLV_TYPE_WIFIDIRECT_INTERFACE 0x0010
/** TLV : WifiDirect param operating channel */
#define TLV_TYPE_WIFIDIRECT_OPCHANNEL 0x0011
/** TLV : WifiDirect param invitation flag */
#define TLV_TYPE_WIFIDIRECT_INVITATION_FLAG 0x0012

/** WPS Device Info OUI+Type+SubType Length */
#define WPS_DEVICE_TYPE_LEN 8

/** IE header len */
#define IE_HEADER_LEN 2

/** WIFIDIRECT IE header len */
#define WIFIDIRECT_IE_HEADER_LEN 3

/** OUI Type WIFIDIRECT */
#define WIFIDIRECT_OUI_TYPE 9
/** OUI Type WPS */
#define WIFI_WPS_OUI_TYPE 4

/*
 * To handle overlapping WIFIDIRECT IEs
 */
/** IE next byte type */
#define WIFIDIRECT_OVERLAP_TYPE 1
/** IE next byte length */
#define WIFIDIRECT_OVERLAP_LEN 2
/** IE next byte data */
#define WIFIDIRECT_OVERLAP_DATA 3

/** Max payload for IE buffer  */
#define WIFI_IE_MAX_PAYLOAD 256

/** Fixed length fields in bonjour payload query data */
#define WIFIDIRECT_DISCOVERY_BONJOUR_FIXED_LEN 5

/** Fixed length fields in uPnP payload query data */
#define WIFIDIRECT_DISCOVERY_UPNP_FIXED_LEN 3

/** Action field for discovery request */
#define WIFIDIRECT_DISCOVERY_REQUEST_ACTION 10

/** Action field for discovery response */
#define WIFIDIRECT_DISCOVERY_RESPONSE_ACTION 11

/** TLV buffer : WifiDirect Status */
typedef PACK_START struct _tlvbuf_wifidirect_status {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT status code */
	t_u8 status_code;
} PACK_END tlvbuf_wifidirect_status;

/** TLV buffer : wifidirect IE device Id */
typedef PACK_START struct _tlvbuf_wifidirect_device_id {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT device MAC address */
	t_u8 dev_mac_address[ETH_ALEN];
} PACK_END tlvbuf_wifidirect_device_id;

/** TLV buffer : wifidirect IE capability */
typedef PACK_START struct _tlvbuf_wifidirect_capability {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT device capability */
	t_u8 dev_capability;
	/** WIFIDIRECT group capability */
	t_u8 group_capability;
} PACK_END tlvbuf_wifidirect_capability;

/** TLV buffer : wifidirect IE Group owner intent */
typedef PACK_START struct _tlvbuf_wifidirect_group_owner_intent {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT device group owner intent */
	t_u8 dev_intent;
} PACK_END tlvbuf_wifidirect_group_owner_intent;

/** TLV buffer : WifiDirect IE Manageability */
typedef PACK_START struct _tlvbuf_wifidirect_manageability {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT Manageability */
	t_u8 manageability;
} PACK_END tlvbuf_wifidirect_manageability;

/** TLV buffer : WifiDirect IE Invitation Flag */
typedef PACK_START struct _tlvbuf_wifidirect_invitation_flag {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT Manageability */
	t_u8 invitation_flag;
} PACK_END tlvbuf_wifidirect_invitation_flag;

/** TLV buffer : wifidirect IE capability */
typedef PACK_START struct _tlvbuf_wifidirect_channel {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT country string */
	t_u8 country_string[3];
	/** WIFIDIRECT regulatory class */
	t_u8 regulatory_class;
	/** WIFIDIRECT channel number */
	t_u8 channel_number;
} PACK_END tlvbuf_wifidirect_channel;

/** Channel Entry */
typedef PACK_START struct _chan_entry {
	/** WIFIDIRECT regulatory class */
	t_u8 regulatory_class;
	/** WIFIDIRECT no of channels */
	t_u8 num_of_channels;
	/** WIFIDIRECT channel number */
	t_u8 chan_list[];
} PACK_END chan_entry;

/** TLV buffer : wifidirect IE channel list */
typedef PACK_START struct _tlvbuf_wifidirect_channel_list {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT country string */
	t_u8 country_string[3];
	/** WIFIDIRECT channel entries */
	chan_entry wifidirect_chan_entry_list[];
} PACK_END tlvbuf_wifidirect_channel_list;

/** NoA Descriptor */
typedef PACK_START struct _noa_descriptor {
	/** WIFIDIRECT count OR type */
	t_u8 count_type;
	/** WIFIDIRECT duration */
	t_u32 duration;
	/** WIFIDIRECT interval */
	t_u32 interval;
	/** WIFIDIRECT start time */
	t_u32 start_time;
} PACK_END noa_descriptor;

/** TLV buffer : WifiDirect IE Notice of Absence */
typedef PACK_START struct _tlvbuf_wifidirect_notice_of_absence {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT NoA Index */
	t_u8 noa_index;
	/** WIFIDIRECT CTWindow and OppPS parameters */
	t_u8 ctwindow_opp_ps;
	/** WIFIDIRECT NoA Descriptor list */
	noa_descriptor wifidirect_noa_descriptor_list[];
} PACK_END tlvbuf_wifidirect_notice_of_absence;

/** TLV buffer : wifidirect IE device Info */
typedef PACK_START struct _tlvbuf_wifidirect_device_info {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT device address */
	t_u8 dev_address[ETH_ALEN];
	/** WPS config methods */
	t_u16 config_methods;
	/** Primary device type : category */
	t_u16 primary_category;
	/** Primary device type : OUI */
	t_u8 primary_oui[4];
	/** Primary device type : sub-category */
	t_u16 primary_subcategory;
	/** Secondary Device Count */
	t_u8 secondary_dev_count;
	/** Secondary Device Info */
	t_u8 secondary_dev_info[];
#if 0
    /** WPS Device Name Tag */
    t_u16 device_name_type;
    /** WPS Device Name Length */
    t_u16 device_name_len;
    /** Device name */
    t_u8 device_name[];
#endif
} PACK_END tlvbuf_wifidirect_device_info;

typedef PACK_START struct _wifidirect_client_dev_info {
	/** Length of each device */
	t_u8 dev_length;
	/** WIFIDIRECT device address */
	t_u8 wifidirect_dev_address[ETH_ALEN];
	/** WIFIDIRECT Interface  address */
	t_u8 wifidirect_intf_address[ETH_ALEN];
	/** WIFIDIRECT Device capability*/
	t_u8 wifidirect_dev_capability;
	/** WPS config methods */
	t_u16 config_methods;
	/** Primary device type : category */
	t_u16 primary_category;
	/** Primary device type : OUI */
	t_u8 primary_oui[4];
	/** Primary device type : sub-category */
	t_u16 primary_subcategory;
	/** Secondary Device Count */
	t_u8 wifidirect_secondary_dev_count;
	/** Secondary Device Info */
	t_u8 wifidirect_secondary_dev_info[0];
	/** WPS WIFIDIRECT Device Name Tag */
	t_u16 wifidirect_device_name_type;
	/** WPS WIFIDIRECT Device Name Length */
	t_u16 wifidirect_device_name_len;
	/** WIFIDIRECT Device name */
	t_u8 wifidirect_device_name[];
} PACK_END wifidirect_client_dev_info;

typedef PACK_START struct _tlvbuf_wifidirect_group_info {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** Secondary Device Info */
	t_u8 wifidirect_client_dev_list[];
} PACK_END tlvbuf_wifidirect_group_info;

/** TLV buffer : wifidirect IE group Id */
typedef PACK_START struct _tlvbuf_wifidirect_group_id {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT group MAC address */
	t_u8 group_address[ETH_ALEN];
	/** WIFIDIRECT group SSID */
	t_u8 group_ssid[];
} PACK_END tlvbuf_wifidirect_group_id;

/** TLV buffer : wifidirect IE group BSS Id */
typedef PACK_START struct _tlvbuf_wifidirect_group_bss_id {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT group Bss Id */
	t_u8 group_bssid[ETH_ALEN];
} PACK_END tlvbuf_wifidirect_group_bss_id;

/** TLV buffer : wifidirect IE Interface */
typedef PACK_START struct _tlvbuf_wifidirect_interface {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT interface Id */
	t_u8 interface_id[ETH_ALEN];
	/** WIFIDIRECT interface count */
	t_u8 interface_count;
	/** WIFIDIRECT interface addresslist */
	t_u8 interface_idlist[];
} PACK_END tlvbuf_wifidirect_interface;

/** TLV buffer : WifiDirect configuration timeout */
typedef PACK_START struct _tlvbuf_wifidirect_config_timeout {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** Group configuration timeout */
	t_u8 group_config_timeout;
	/** Device configuration timeout */
	t_u8 device_config_timeout;
} PACK_END tlvbuf_wifidirect_config_timeout;

/** TLV buffer : WifiDirect extended listen time */
typedef PACK_START struct _tlvbuf_wifidirect_ext_listen_time {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** Availability period */
	t_u16 availability_period;
	/** Availability interval */
	t_u16 availability_interval;
} PACK_END tlvbuf_wifidirect_ext_listen_time;

/** TLV buffer : WifiDirect Intended Interface Address */
typedef PACK_START struct _tlvbuf_wifidirect_intended_addr {
	/** TLV Header tag */
	t_u8 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFIDIRECT Group interface address */
	t_u8 group_address[ETH_ALEN];
} PACK_END tlvbuf_wifidirect_intended_addr;

/** TLV buffer : Wifi WPS IE */
typedef PACK_START struct _tlvbuf_wifi_wps_ie {
	/** TLV Header tag */
	t_u16 tag;
	/** TLV Header length */
	t_u16 length;
	/** WIFI WPS IE data */
	t_u8 data[];
} PACK_END tlvbuf_wps_ie;

/** WifiDirect IE Header */
typedef PACK_START struct _wifidirect_ie_header {
	/** Element ID */
	t_u8 element_id;
	/** IE Length */
	t_u8 ie_length;
	/** OUI */
	t_u8 oui[3];
	/** OUI type */
	t_u8 oui_type;
	/** IE List of TLV */
	t_u8 ie_list[];
} PACK_END wifidirect_ie_header;

/** Event : WifiDirect Generic event */
typedef PACK_START struct _apeventbuf_wifidirect_generic {
	/** Event Length */
	t_u16 event_length;
	/** Event Type */
	t_u16 event_type;
	/** Event SubType */
	t_u16 event_sub_type;
	/** Peer mac address */
	t_u8 peer_mac_addr[ETH_ALEN];
	/** IE List of TLV */
	t_u8 entire_ie_list[];
} PACK_END apeventbuf_wifidirect_generic;

/** Internal WIFIDIRECT structure for Query Data */
typedef PACK_START struct wifidirect_query_data {
	union {
		PACK_START struct upnp_specific_query {
			/** version field */
			t_u8 version;
			/** value */
			t_u8 value[];
		} PACK_END upnp;

		PACK_START struct bonjour_specific_query {
			/** DNS name */
			t_u8 dns[0];
			/** DNS type */
			t_u8 dns_type;
			/** version field */
			t_u8 version;
		} PACK_END bonjour;
	} u;
} PACK_END wifidirect_query_data;

/** Internal WIFIDIRECT structure for Response Data */
typedef PACK_START struct wifidirect_Response_data {
	union {
		PACK_START struct upnp_specific_response {
			/** version field */
			t_u8 version;
			/** value */
			t_u8 value[];
		} PACK_END upnp;

		PACK_START struct bonjour_specific_response {
			/** DNS name */
			t_u8 dns[0];
			/** DNS type */
			t_u8 dns_type;
			/** version field */
			t_u8 version;
			/** DNS name */
			t_u8 record[];
		} PACK_END bonjour;
	} u;
} PACK_END wifidirect_response_data;

/** Event : Service Discovery request */
typedef PACK_START struct _apeventbuf_wifidirect_discovery_request {
	/** Peer mac address */
	t_u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	t_u8 category;
	/** Action */
	t_u8 action;
	/** Dialog taken */
	t_u8 dialog_taken;
	/** Advertize protocol IE */
	t_u8 advertize_protocol_ie[4];
	/** Query request Length */
	t_u16 query_len;
	/** Information identifier */
	t_u8 info_id[2];
	/** Request Length */
	t_u16 request_len;
	/** OUI */
	t_u8 oui[3];
	/** OUI sub type */
	t_u8 oui_sub_type;
	/** Service update indicator */
	t_u16 service_update_indicator;
	/** Vendor Length */
	t_u16 vendor_len;
	/** Service protocol */
	t_u8 service_protocol;
	/** Service transaction Id */
	t_u8 service_transaction_id;
	/** Query Data */
	wifidirect_query_data disc_query;
} PACK_END apeventbuf_wifidirect_discovery_request;

/** HostCmd_CMD_WIFIDIRECT_SERVICE_DISCOVERY response */
typedef PACK_START struct _apeventbuf_wifidirect_discovery_response {
	/** Peer mac address */
	t_u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	t_u8 category;
	/** Action */
	t_u8 action;
	/** Dialog taken */
	t_u8 dialog_taken;
	/** Status code */
	t_u8 status_code;
	/** GAS comback reply */
	t_u16 gas_reply;
	/** Advertize protocol IE */
	t_u8 advertize_protocol_ie[4];
	/** Query response Length */
	t_u16 query_len;
	/** Information identifier */
	t_u8 info_id[2];
	/** Response Length */
	t_u16 response_len;
	/** OUI */
	t_u8 oui[3];
	/** OUI sub type */
	t_u8 oui_sub_type;
	/** Service update indicator */
	t_u16 service_update_indicator;
	/** Vendor Length */
	t_u16 vendor_len;
	/** Service protocol */
	t_u8 service_protocol;
	/** Service transaction Id */
	t_u8 service_transaction_id;
	/** Discovery status code */
	t_u8 disc_status_code;
	/** Response Data */
	wifidirect_response_data disc_resp;
} PACK_END apeventbuf_wifidirect_discovery_response;

/** enum : WPS attribute type */
typedef enum {
	SC_AP_Channel = 0x1001,
	SC_Association_State = 0x1002,
	SC_Authentication_Type = 0x1003,
	SC_Authentication_Type_Flags = 0x1004,
	SC_Authenticator = 0x1005,
	SC_Config_Methods = 0x1008,
	SC_Configuration_Error = 0x1009,
	SC_Confirmation_URL4 = 0x100A,
	SC_Confirmation_URL6 = 0x100B,
	SC_Connection_Type = 0x100C,
	SC_Connection_Type_Flags = 0x100D,
	SC_Credential = 0x100E,
	SC_Device_Name = 0x1011,
	SC_Device_Password_ID = 0x1012,
	SC_E_Hash1 = 0x1014,
	SC_E_Hash2 = 0x1015,
	SC_E_SNonce1 = 0x1016,
	SC_E_SNonce2 = 0x1017,
	SC_Encrypted_Settings = 0x1018,
	SC_Encryption_Type = 0X100F,
	SC_Encryption_Type_Flags = 0x1010,
	SC_Enrollee_Nonce = 0x101A,
	SC_Feature_ID = 0x101B,
	SC_Identity = 0X101C,
	SC_Identity_Proof = 0X101D,
	SC_Key_Wrap_Authenticator = 0X101E,
	SC_Key_Identifier = 0X101F,
	SC_MAC_Address = 0x1020,
	SC_Manufacturer = 0x1021,
	SC_Message_Type = 0x1022,
	SC_Model_Name = 0x1023,
	SC_Model_Number = 0x1024,
	SC_Network_Index = 0x1026,
	SC_Network_Key = 0x1027,
	SC_Network_Key_Index = 0x1028,
	SC_New_Device_Name = 0x1029,
	SC_New_Password = 0x102A,
	SC_OOB_Device_Password = 0X102C,
	SC_OS_Version = 0X102D,
	SC_Power_Level = 0X102F,
	SC_PSK_Current = 0x1030,
	SC_PSK_Max = 0x1031,
	SC_Public_Key = 0x1032,
	SC_Radio_Enabled = 0x1033,
	SC_Reboot = 0x1034,
	SC_Registrar_Current = 0x1035,
	SC_Registrar_Established = 0x1036,
	SC_Registrar_List = 0x1037,
	SC_Registrar_Max = 0x1038,
	SC_Registrar_Nonce = 0x1039,
	SC_Request_Type = 0x103A,
	SC_Response_Type = 0x103B,
	SC_RF_Band = 0X103C,
	SC_R_Hash1 = 0X103D,
	SC_R_Hash2 = 0X103E,
	SC_R_SNonce1 = 0X103F,
	SC_R_SNonce2 = 0x1040,
	SC_Selected_Registrar = 0x1041,
	SC_Serial_Number = 0x1042,
	SC_Simple_Config_State = 0x1044,
	SC_SSID = 0x1045,
	SC_Total_Networks = 0x1046,
	SC_UUID_E = 0x1047,
	SC_UUID_R = 0x1048,
	SC_Vendor_Extension = 0x1049,
	SC_Version = 0x104A,
	SC_X_509_Certificate_Request = 0x104B,
	SC_X_509_Certificate = 0x104C,
	SC_EAP_Identity = 0x104D,
	SC_Message_Counter = 0x104E,
	SC_Public_Key_Hash = 0x104F,
	SC_Rekey_Key = 0x1050,
	SC_Key_Lifetime = 0x1051,
	SC_Permitted_Config_Methods = 0x1052,
	SC_SelectedRegistrarConfigMethods = 0x1053,
	SC_Primary_Device_Type = 0x1054,
	SC_Secondary_Device_Type_List = 0x1055,
	SC_Portable_Device = 0x1056,
	SC_AP_Setup_Locked = 0x1057,
	SC_Application_List = 0x1058,
	SC_EAP_Type = 0x1059,
	SC_Initialization_Vector = 0x1060,
	SC_Key_Provided_Auto = 0x1061,
	SC_8021x_Enabled = 0x1062,
	SC_App_Session_key = 0x1063,
	SC_WEP_Transmit_Key = 0x1064,
} wps_simple_config_attribute;
#endif

/** structure for channel switch result from TDLS FW */
typedef PACK_START struct _Channel_switch_result {
	/** current channel, 0 - base channel, 1 - off channel*/
	t_u8 current_channel;
	/** channel switch status*/
	t_u8 status;
	/** channel switch fauilure reason code*/
	t_u8 reason;
} PACK_END Channel_switch_result;

/** Event : TDLS Generic event */
typedef PACK_START struct _eventbuf_tdls_generic {
	/** Event Type */
	t_u16 event_type;
	/** Peer mac address */
	t_u8 peer_mac_addr[ETH_ALEN];
	union {
		/** channel switch result structure*/
		Channel_switch_result switch_result;
		/** channel switch stop reason */
		t_u8 cs_stop_reason;
		/** Reason code */
		t_u16 reason_code;
		/** IE Length */
		t_u16 ie_length;
	} u;
	/** IE pointer */
	t_u8 ie_ptr[];
} PACK_END eventbuf_tdls_generic;

/** Event : TDLS Debug event */
typedef PACK_START struct _eventbuf_tdls_debug {
	/** Event Type */
	t_u16 event_type;
	/** TimeStamp value */
	t_u64 tsf;
	/** First argument */
	t_u32 first_arg;
	/** Second argument */
	t_u32 second_arg;
	/** Third argument */
	t_u32 third_arg;
	/** Third argument */
	t_u16 string_len;
	/** Third argument */
	t_u8 string[];
} PACK_END eventbuf_tdls_debug;

/** Event : TDLS packet event */
typedef PACK_START struct _eventbuf_tdls_packet {
	/** Event Type */
	t_u16 event_type;
	/** Length */
	t_u16 length;
	/** packet data */
	t_u8 data[];
} PACK_END eventbuf_tdls_packet;

/** Eapol state */
typedef PACK_START struct {
	/** Eapol state*/
	t_u8 eapol_state;
	/** Station address*/
	t_u8 sta_mac_addr[ETH_ALEN];
} PACK_END eapol_state_t;

/** Debug Info */
typedef PACK_START union {
	/** Eapol key message */
	eapol_keymsg_debug_t eapol_pwkmsg;
	/** Station reject*/
	sta_reject_t sta_reject;
	/** WPA IE */
	wpaie_t wpaie;
	/** Eapol state */
	eapol_state_t eapol_state;
} PACK_END d_info;

/** Event body : Debug */
typedef PACK_START struct _eventbuf_debug {
	/** Debug type */
	t_u8 debug_type;
	/** Major debug id */
	t_u32 debug_id_major;
	/** Minor debug id */
	t_u32 debug_id_minor;
	/** Debug Info */
	d_info info;
} PACK_END eventbuf_debug;

int ishexstring(void *hex);
unsigned int a2hex(char *s);
/**
 *    @brief isdigit for String.
 *
 *    @param x            Char string
 *    @return             MLAN_EVENT_FAILURE for non-digit.
 *                        0 for digit
 */
static inline int ISDIGIT(char *x)
{
	unsigned int i;
	for (i = 0; i < strlen(x); i++)
		if (isdigit((unsigned char)x[i]) == 0)
			return MLAN_EVENT_FAILURE;
	return 0;
}

#define CUS_EVT_TOD_TOA "EVENT=TOD-TOA"

#define CUS_EVT_MLAN_CSI "EVENT=MLAN_CSI"
#define CSI_DUMP_FILE_MAX 1200000 // 1.2MB
/** CSI record data structure */
typedef PACK_START struct _csi_record_ds {
	/** Length in DWORDS, including header */
	t_u16 Len;
	/** CSI signature. 0xABCD fixed */
	t_u16 CSI_Sign;
	/** User defined HeaderID  */
	t_u32 CSI_HeaderID;
	/** Packet info field */
	t_u16 PKT_info;
	/** Frame control field for the received packet*/
	t_u16 FCF;
	/** Timestamp when packet received */
	t_u64 TSF;
	/** Received Packet Destination MAC Address */
	t_u8 Dst_MAC[6];
	/** Received Packet Source MAC Address */
	t_u8 Src_MAC[6];
	/** RSSI for antenna A */
	t_u8 Rx_RSSI_A;
	/** RSSI for antenna B */
	t_u8 Rx_RSSI_B;
	/** Noise floor for antenna A */
	t_u8 Rx_NF_A;
	/** Noise floor for antenna A */
	t_u8 Rx_NF_B;
	/** Rx signal strength above noise floor */
	t_u8 Rx_SINR;
	/** Channel */
	t_u8 channel;
	/** user defined Chip ID */
	t_u16 chip_id;
	/** Reserved */
	t_u32 rsvd;
	/** CSI data length in DWORDs */
	t_u32 CSI_Data_Length;
	/** Start of CSI data */
	t_u8 CSI_Data[0];
	/** At the end of CSI raw data, user defined TailID of 4 bytes*/
} PACK_END csi_record_ds;
/** Event body : MLAN_CSI */
typedef PACK_START struct _event_mlan_csi {
	/** Event string: EVENT=MLAN_CSI */
	char event_str[14];
	/** Event sequence # */
	t_u16 sequence;
	csi_record_ds csi_record;
} PACK_END event_mlan_csi;

#endif /* _MLAN_EVENT_H */
