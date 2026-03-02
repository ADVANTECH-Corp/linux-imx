/** @file wlan_wifidir.h
 *  @brief This file contains definitions for NXP WLAN driver host command,
 *         for wifidir specific commands.
 *
 *  Copyright 2012-2020, 2024 NXP
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

#ifndef _WLAN_WIFIDIR_H_
#define _WLAN_WIFIDIR_H_

#include "wlan_hostcmd.h"

/*
 *  ctype from older glib installations defines BIG_ENDIAN.  Check it
 *   and undef it if necessary to correctly process the wlan header
 *   files
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)
#undef BIG_ENDIAN
#endif

#ifdef BIG_ENDIAN

/** Convert TLV header to little endian format from CPU format */
#define endian_convert_tlv_header_out(x)                                       \
	;                                                                      \
	{                                                                      \
		(x)->tag = wlan_cpu_to_le16((x)->tag);                         \
		(x)->length = wlan_cpu_to_le16((x)->length);                   \
	}

/** Convert TLV header from little endian format to CPU format */
#define endian_convert_tlv_header_in(x)                                        \
	{                                                                      \
		(x)->tag = le16_to_cpu((x)->tag);                              \
		(x)->length = le16_to_cpu((x)->length);                        \
	}

#else /* BIG_ENDIAN */

/** Do nothing */
#define endian_convert_tlv_header_out(x)
/** Do nothing */
#define endian_convert_tlv_header_in(x)

#endif /* BIG_ENDIAN */

/* Event IDs for Netlink socket events.  They are borrowed from mlanevent.c.
 * Note that these are not all WIFIDIR events, so they should probably be
 * defined elsewhere.
 */
#define EV_ID_UAP_EV_WMM_STATUS_CHANGE 0x00000017
#define EV_ID_UAP_EV_ID_STA_DEAUTH 0x0000002c
#define EV_ID_UAP_EV_ID_STA_ASSOC 0x0000002d
#define EV_ID_UAP_EV_ID_BSS_START 0x0000002e
#define EV_ID_UAP_EV_ID_DEBUG 0x00000036
#define EV_ID_UAP_EV_BSS_IDLE 0x00000043
#define EV_ID_UAP_EV_BSS_ACTIVE 0x00000044
#define EV_ID_WIFIDIR_GENERIC 0x00000049
#define EV_ID_WIFIDIR_SERVICE_DISCOVERY 0x0000004a
#define EV_ID_UAP_EV_RSN_CONNECT 0x00000051
#define EV_ID_STA_EV_SCAN_COMPLETE 0x80000008
#define EV_ID_STA_EV_SCAN_COMPLETE_GSPI 0x80000009

/* WPS event Ids */
#define EV_ID_UAP_EV_PROBE_REQUEST 0x80000005
#define MAX_SERVICE_NAME_LEN 255
#define MAX_SERVICE_INFO_LEN 255

#define PRINT_CASE(i)                                                          \
	case i:                                                                \
		return #i
static inline char *nl_event_id_to_str(int id)
{
	switch (id) {
		PRINT_CASE(EV_ID_UAP_EV_WMM_STATUS_CHANGE);
		PRINT_CASE(EV_ID_UAP_EV_ID_STA_DEAUTH);
		PRINT_CASE(EV_ID_UAP_EV_ID_STA_ASSOC);
		PRINT_CASE(EV_ID_UAP_EV_ID_BSS_START);
		PRINT_CASE(EV_ID_UAP_EV_ID_DEBUG);
		PRINT_CASE(EV_ID_UAP_EV_BSS_IDLE);
		PRINT_CASE(EV_ID_UAP_EV_BSS_ACTIVE);
		PRINT_CASE(EV_ID_WIFIDIR_GENERIC);
		PRINT_CASE(EV_ID_WIFIDIR_SERVICE_DISCOVERY);
		PRINT_CASE(EV_ID_UAP_EV_RSN_CONNECT);
	}
	return "UNKNOWN";
}

/* enum wifidir_generic_event
 *
 * Events with the id EV_ID_WIFIDIR_GENERIC have one of the following types:
 *
 * EV_TYPE_WIFIDIR_NEGOTIATION_REQUEST:
 *
 * EV_TYPE_WIFIDIR_NEGOTIATION_RESPONSE:
 *
 * EV_TYPE_WIFIDIR_NEGOTIATION_RESULT:
 *
 * EV_TYPE_WIFIDIR_INVITATION_REQ:
 *
 * EV_TYPE_WIFIDIR_INVITATION_RESP:
 *
 * EV_TYPE_WIFIDIR_DISCOVERABILITY_REQUEST:
 *
 * EV_TYPE_WIFIDIR_DISCOVERABILITY_RESPONSE:
 *
 * EV_TYPE_WIFIDIR_PROVISION_DISCOVERY_REQ:
 *
 * EV_TYPE_WIFIDIR_PROVISION_DISCOVERY_RESP:
 *
 * EV_TYPE_WIFIDIR_PEER_DETECTED:
 *
 * EV_TYPE_WIFIDIR_CLIENT_ASSOCIATED_EVENT:
 *
 * EV_TYPE_WIFIDIR_FW_DEBUG_EVENT:
 */
enum wifidir_generic_event {
	EV_TYPE_WIFIDIR_NEGOTIATION_REQUEST = 0,
	EV_TYPE_WIFIDIR_NEGOTIATION_RESPONSE,
	EV_TYPE_WIFIDIR_NEGOTIATION_RESULT,
	EV_TYPE_WIFIDIR_INVITATION_REQ,
	EV_TYPE_WIFIDIR_INVITATION_RESP,
	EV_TYPE_WIFIDIR_DISCOVERABILITY_REQUEST,
	EV_TYPE_WIFIDIR_DISCOVERABILITY_RESPONSE,
	EV_TYPE_WIFIDIR_PROVISION_DISCOVERY_REQ,
	EV_TYPE_WIFIDIR_PROVISION_DISCOVERY_RESP,
	EV_TYPE_WIFIDIR_NEGOTIATION_RESPONSE_TX,
	/* Events 10-13 are not documented or used */
	EV_TYPE_WIFIDIR_PEER_DETECTED = 14,
	EV_TYPE_WIFIDIR_CLIENT_ASSOCIATED,
	EV_TYPE_WIFIDIR_FW_DEBUG,
	EV_TYPE_WIFIDIR_NEGOTIATION_REQ_TX = 18,
	EV_TYPE_WIFIDIR_INVITATION_REQ_TX,
	EV_TYPE_WIFIDIR_INVITATION_RESP_TX,
	EV_TYPE_WIFIDIR_PROVISION_REQ_TX,
	EV_TYPE_WIFIDIR_PROVISION_RESP_TX,
};

static inline char *wifidir_generic_event_to_str(enum wifidir_generic_event e)
{
	switch (e) {
	case EV_TYPE_WIFIDIR_NEGOTIATION_REQUEST:
		return "WIFIDIR_NEGOTIATION_REQUEST";
	case EV_TYPE_WIFIDIR_NEGOTIATION_RESPONSE:
		return "WIFIDIR_NEGOTIATION_RESPONSE";
	case EV_TYPE_WIFIDIR_NEGOTIATION_RESULT:
		return "WIFIDIR_NEGOTIATION_RESULT";
	case EV_TYPE_WIFIDIR_INVITATION_REQ:
		return "WIFIDIR_INVITATION_REQ";
	case EV_TYPE_WIFIDIR_INVITATION_RESP:
		return "WIFIDIR_INVITATION_RESP";
	case EV_TYPE_WIFIDIR_DISCOVERABILITY_REQUEST:
		return "WIFIDIR_DISCOVERABILITY_REQUEST";
	case EV_TYPE_WIFIDIR_DISCOVERABILITY_RESPONSE:
		return "WIFIDIR_DISCOVERABILITY_RESPONSE";
	case EV_TYPE_WIFIDIR_PROVISION_DISCOVERY_REQ:
		return "WIFIDIR_PROVISION_DISCOVERY_REQ";
	case EV_TYPE_WIFIDIR_PROVISION_DISCOVERY_RESP:
		return "WIFIDIR_PROVISION_DISCOVERY_RESP";
	case EV_TYPE_WIFIDIR_NEGOTIATION_RESPONSE_TX:
		return "EV_TYPE_WIFIDIR_NEGOTIATION_RESPONSE_TX";
	case EV_TYPE_WIFIDIR_PEER_DETECTED:
		return "WIFIDIR_PEER_DETECTED";
	case EV_TYPE_WIFIDIR_CLIENT_ASSOCIATED:
		return "WIFIDIR_CLIENT_ASSOCIATED";
	case EV_TYPE_WIFIDIR_FW_DEBUG:
		return "WIFIDIR_DEBUG";
	case EV_TYPE_WIFIDIR_NEGOTIATION_REQ_TX:
		return "EV_TYPE_WIFIDIR_NEGOTIATION_REQ_TX";
	case EV_TYPE_WIFIDIR_INVITATION_REQ_TX:
		return "EV_TYPE_WIFIDIR_INVITATION_REQ_TX";
	case EV_TYPE_WIFIDIR_INVITATION_RESP_TX:
		return "EV_TYPE_WIFIDIR_INVITATION_RESP_TX";
	case EV_TYPE_WIFIDIR_PROVISION_REQ_TX:
		return "EV_TYPE_WIFIDIR_PROVISION_REQ_TX";
	case EV_TYPE_WIFIDIR_PROVISION_RESP_TX:
		return "EV_TYPE_WIFIDIR_PROVISION_RESP_TX";

	default:
		return "UNKNOWN";
		break;
	}
}

/* enum wifidir_generic_subevent
 *
 * wifidir_generic_events may have one of the following subtypes.  These are
 * undocumented, but appear to function more as a result code than an event
 * subtype.  They are documented per-event above.
 */
enum wifidir_generic_subevent {
	EV_SUBTYPE_WIFIDIR_GO_NEG_FAILED = 0,
	EV_SUBTYPE_WIFIDIR_GO_ROLE,
	EV_SUBTYPE_WIFIDIR_CLIENT_ROLE,
};

/** OUI Type WIFIDIR */
#define WIFIDIR_OUI_TYPE 9
/** OUI Type WPS  */
#define WIFI_WPS_OUI_TYPE 4

/** category: Public Action frame */
#define WIFI_CATEGORY_PUBLIC_ACTION_FRAME 4
/** Dialog Token : Ignore */
#define WIFI_DIALOG_TOKEN_IGNORE 0

/** WIFIDIR IE header len */
#define WIFIDIR_IE_HEADER_LEN 3
/** IE header len */
#define IE_HEADER_LEN 2

/** command to stop as wifidir device. */
#define WIFIDIR_MODE_STOP 0
/** command to start as wifidir device. */
#define WIFIDIR_MODE_START 1
/** command to start as group owner from host. */
#define WIFIDIR_MODE_START_GROUP_OWNER 2
/** command to start as device from host. */
#define WIFIDIR_MODE_START_DEVICE 3
/** command to start find phase  */
#define WIFIDIR_MODE_START_FIND_PHASE 4
/** command to stop find phase  */
#define WIFIDIR_MODE_STOP_FIND_PHASE 5
/** command to abort current connection  */
#define WIFIDIR_MODE_CONFIGURED_ABORT_CUR_CON 0x0a
/** command to get WIFIDIR mode   */
#define WIFIDIR_MODE_QUERY 0xFF
/** command to get WIFIDIR GO mode   */
#define WIFIDIR_GO_QUERY 0xFFFF

/** 4 byte header to store buf len*/
#define BUF_HEADER_SIZE 4
/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE 1024
/** Host Command ID bit mask (bit 11:0) */
#define HostCmd_CMD_ID_MASK 0x0fff
/** WIFIDIRCMD response check */
#define WIFIDIRCMD_RESP_CHECK 0x8000

/** Host Command ID:  WIFIDIR_ACTION_FRAME */
#define HostCmd_CMD_802_11_ACTION_FRAME 0x00f4
/** Host Command ID : wifidir mode config */
#define HostCmd_CMD_WIFIDIR_MODE_CONFIG 0x00eb
/** Host Command ID:  WIFIDIR_SET_PARAMS */
#define HostCmd_CMD_WIFIDIR_PARAMS_CONFIG 0x00ea
/** Check Bit GO set by peer Bit 0 */
#define WIFIDIR_GROUP_OWNER_MASK 0x01
/** Check Bit GO set by peer Bit 0 */
#define WIFIDIR_GROUP_CAP_PERSIST_GROUP_BIT 0x02
/** Check Bit formation Bit 6 */
#define WIFIDIR_GROUP_FORMATION_OR_MASK 0x40
/** Check Bit formation Bit 6 */
#define WIFIDIR_GROUP_FORMATION_AND_MASK 0xBF
/** Group Capability: Intra-BSS Distribution Bit Mask [bit #3] */
#define WIFIDIR_GROUP_CAP_INTRA_BSS_DS 0x8

/** Device capability: Service Discovery */
#define WIFIDIR_DEV_CAPABILITY_SD_BIT 0x01

/** Mask for device role in sub_event_type */
#define DEVICE_ROLE_MASK 0x0003
/** Mask for packet process state in sub_event_type */
#define PKT_PROCESS_STATE_MASK 0x001c

/** C Success */
#define SUCCESS 1
/** C Failure */
#define FAILURE 0

/** Wifi reg class = 81 */
#define WIFI_REG_CLASS_81 81

/** MAC BROADCAST */
#define WIFIDIR_RET_MAC_BROADCAST 0x1FF
/** MAC MULTICAST */
#define WIFIDIR_RET_MAC_MULTICAST 0x1FE
/**
 * Hex or Decimal to Integer
 * @param   num string to convert into decimal or hex
 */
#define A2HEXDECIMAL(num)                                                      \
	(strncasecmp("0x", (num), 2) ? (unsigned int)strtoll((num), NULL, 0) : \
				       a2hex((num))) /**                       \
						      * Check of decimal or    \
						      * hex string             \
						      * @param   num string    \
						      */
#define IS_HEX_OR_DIGIT(num)                                                   \
	(strncasecmp("0x", (num), 2) ? ISDIGIT((num)) : ishexstring((num)))

/** Set OPP_PS variable */
#define SET_OPP_PS(x) ((x) << 7)

/** Convert WIFIDIR header to little endian format from CPU format */
#define endian_convert_tlv_wifidir_header_out(x)                               \
	;                                                                      \
	{                                                                      \
		(x)->length = wlan_cpu_to_le16((x)->length);                   \
	}

/** Convert WPS TLV header to network order */
#define endian_convert_tlv_wps_header_out(x)                                   \
	;                                                                      \
	{                                                                      \
		(x)->tag = mwu_htons((x)->tag);                                \
		(x)->length = mwu_htons((x)->length);                          \
	}
//#ifdef WFD_SERVICE_DISCOVERY
/** WIFIDIR Service Discovery Request */
#define WIFIDIR_SERVICE_DISCOVERY_REQUEST 0
/** WIFIDIR Service Discovery Response */
#define WIFIDIR_SERVICE_DISCOVERY_RESPONSE 1
/** WIFIDIR GAS Comeback Discovery Request */
#define WIFIDIR_GAS_COMEBACK_DISCOVERY_REQUEST 2
/** WIFIDIR GAS Comeback Discovery Response */
#define WIFIDIR_GAS_COMEBACK_DISCOVERY_RESPONSE 3
/** Host Command ID:  WFD_SERVICE_DISCOVERY */
#define HostCmd_CMD_WIFIDIR_SERVICE_DISCOVERY 0x00ec
/** File Transfer Service Protocol */
#define FILE_TRANSFER_SERVICE_PROTOCOL 4
/** Action field for service discovery request */
#define WIFIDIRECT_DISCOVERY_REQUEST_ACTION 10

/** Action field for service discovery response */
#define WIFIDIRECT_DISCOVERY_RESPONSE_ACTION 11

/** Action field for service discovery gas comeback request */
#define WIFIDIRECT_DISCOVERY_GAS_COMEBACK_REQUEST_ACTION 12

/** Action field for service discovery gas comeback response */
#define WIFIDIRECT_DISCOVERY_GAS_COMEBACK_RESPONSE_ACTION 13

/** GAS MORE FRAGMENT BIT */
#define GAS_MORE_FRAGMENT_BIT (1 << 7)

/** Maximum size of one fragment of GAS comeback response and service discovery
 * response*/
#define MAX_FRAGMENT_SIZE 200
//#endif /* WFD_SERVICE_DISCOVERY */

/** OUI SubType GO NEG REQ action DIRECT */
#define WIFIDIR_GO_NEG_REQ_ACTION_SUB_TYPE 0
/** OUI SubType Invitation request */
#define WIFIDIR_INVITATION_REQ_SUB_TYPE 3
/** OUI SubType Provision discovery request */
#define WIFIDIR_PROVISION_DISCOVERY_REQ_SUB_TYPE 7
/** WPS Device Type maximum length */
#define WPS_DEVICE_TYPE_MAX_LEN 8

/** OUI SubType Service discovery request */
#define WIFIDIR_SERVICE_DISCOVERY_SUB_TYPE 9
/** TLV : WIFIDIR status */
#define TLV_TYPE_WIFIDIR_STATUS 0
/** TLV : WIFIDIR param capability */
#define TLV_TYPE_WIFIDIR_CAPABILITY 2
/** TLV : WIFIDIR param device Id */
#define TLV_TYPE_WIFIDIR_DEVICE_ID 3
/** TLV : WIFIDIR param group owner intent */
#define TLV_TYPE_WIFIDIR_GROUPOWNER_INTENT 4
/** TLV : WIFIDIR param config timeout */
#define TLV_TYPE_WIFIDIR_CONFIG_TIMEOUT 5
/** TLV : WIFIDIR param channel */
#define TLV_TYPE_WIFIDIR_CHANNEL 6
/** TLV : WIFIDIR param group bssId */
#define TLV_TYPE_WIFIDIR_GROUP_BSS_ID 7
/** TLV : WIFIDIR param extended listen time */
#define TLV_TYPE_WIFIDIR_EXTENDED_LISTEN_TIME 8
/** TLV : WIFIDIR param intended address */
#define TLV_TYPE_WIFIDIR_INTENDED_ADDRESS 9
/** TLV : WIFIDIR param manageability */
#define TLV_TYPE_WIFIDIR_MANAGEABILITY 10
/** TLV : WIFIDIR param channel list */
#define TLV_TYPE_WIFIDIR_CHANNEL_LIST 11
/** TLV : WIFIDIR Notice of Absence */
#define TLV_TYPE_WIFIDIR_NOTICE_OF_ABSENCE 12
/** TLV : WIFIDIR param device Info */
#define TLV_TYPE_WIFIDIR_DEVICE_INFO 13
/** TLV : WIFIDIR param Group Info */
#define TLV_TYPE_WIFIDIR_GROUP_INFO 14
/** TLV : WIFIDIR param group Id */
#define TLV_TYPE_WIFIDIR_GROUP_ID 15
/** TLV : WIFIDIR param interface */
#define TLV_TYPE_WIFIDIR_INTERFACE 16
/** TLV : WIFIDIR param operating channel */
#define TLV_TYPE_WIFIDIR_OPCHANNEL 17
/** TLV : WIFIDIR param invitation flag */
#define TLV_TYPE_WIFIDIR_INVITATION_FLAG 18
/** TLV : WIFIDIR param OOB GO negotiation channel */
#define TLV_TYPE_WIFIDIR_OOB_GONEG_CHANNEL 19

/** TLV: WIFIDIR Provisioning parameters */
#define WIFIDIR_PROVISIONING_PARAMS_TLV_ID 0x18f

/** TLV: WIFIDIR Discovery Period */
#define MRVL_WIFIDIR_DISC_PERIOD_TLV_ID (PROPRIETARY_TLV_BASE_ID + 124)
/** TLV: WIFIDIR Scan Enable */
#define MRVL_WIFIDIR_SCAN_ENABLE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 125)
/** TLV: WIFIDIR Peer Device  */
#define MRVL_WIFIDIR_PEER_DEVICE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 126)
/** TLV: WIFIDIR Scan Request Peer Device  */
#define MRVL_WIFIDIR_SCAN_REQ_DEVICE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 127)
/** TLV: WIFIDIR Device State */
#define MRVL_WIFIDIR_DEVICE_STATE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 128)
/** TLV: WIFIDIR GO Passphrase */
#define MRVL_WIFIDIR_PASSPHRASE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 200)
/** TLV: WifiDirect Capability */
#define MRVL_WIFIDIRECT_CAPABILITY_TLV_ID                                      \
	(PROPRIETARY_TLV_BASE_ID + 0x82) // 0x0182
/** TLV: WifiDirect Invitation list */
#define MRVL_WIFIDIRECT_INVITATION_LIST_TLV_ID                                 \
	(PROPRIETARY_TLV_BASE_ID + 0x85) // 0x0185
/** TLV: WIFIDIR Listen Channel */
#define MRVL_WIFIDIR_LISTEN_CHANNEL_TLV_ID                                     \
	(PROPRIETARY_TLV_BASE_ID + 0x86) // 0x0186
/** TLV: WIFIDIR Operating Channel */
#define MRVL_WIFIDIR_OPERATING_CHANNEL_TLV_ID                                  \
	(PROPRIETARY_TLV_BASE_ID + 0x87) // 0x0187
/** TLV: WIFIDIR Persistent Group */
#define MRVL_WIFIDIR_PERSISTENT_GROUP_TLV_ID                                   \
	(PROPRIETARY_TLV_BASE_ID + 0x88) // 0x0188
/** TLV: WifiDirect WPS parameters */
#define MRVL_WIFIDIR_WPS_PARAMS_TLV_ID                                         \
	(PROPRIETARY_TLV_BASE_ID + 0x90) // 0x0190
/** TLV: WifiDirect Group Info */
#define MRVL_WIFIDIR_GROUP_INFO_TLV_ID (PROPRIETARY_TLV_BASE_ID + 0xCC)

#define MRVL_WIFIDIRECT_CONNECT_REMOVED_PERSISTENT_STA                         \
	(PROPRIETARY_TLV_BASE_ID + 0xcd)

/** Max Device capability */
#define MAX_DEV_CAPABILITY 255
/** Max group capability */
#define MAX_GRP_CAPABILITY 255
/** Max Intent */
#define MAX_INTENT 15
/** Max length of Primary device type OUI */
#define MAX_PRIMARY_OUI_LEN 4
/** Min value of Regulatory class */
#define MIN_REG_CLASS 1
/** Max value of Regulatory class */
#define MAX_REG_CLASS 255
/** Min Primary Device type category */
#define MIN_PRIDEV_TYPE_CAT 1
/** Max Primary Device type category */
#define MAX_PRIDEV_TYPE_CAT 11
/** Min value of NoA index */
#define MIN_NOA_INDEX 0
/** Max value of NoA index */
#define MAX_NOA_INDEX 255
/** Min value of CTwindow */
#define MIN_CTWINDOW 10
/** Max value of CTwindow */
#define MAX_CTWINDOW 63
/** Min value of Count/Type */
#define MIN_COUNT_TYPE 1
/** Max value of Count/Type */
#define MAX_COUNT_TYPE 255
/** Min Primary Device type subcategory */
#define MIN_PRIDEV_TYPE_SUBCATEGORY 1
/** Max Primary Device type subcategory */
#define MAX_PRIDEV_TYPE_SUBCATEGORY 9
/** Min value of WPS config method */
#define MIN_WPS_CONF_METHODS 0x01
/** Max value of WPS config method */
#define MAX_WPS_CONF_METHODS 0xffff
/** Max length of Advertisement Protocol IE  */
#define MAX_ADPROTOIE_LEN 4
/** Max length of Discovery Information ID  */
#define MAX_INFOID_LEN 2
/** Max length of OUI  */
#define MAX_OUI_LEN 3
/** Max count of interface list */
#define MAX_INTERFACE_ADDR_COUNT 41
/** Max count of secondary device types */
#define MAX_SECONDARY_DEVICE_COUNT 15
/** Max count of group secondary device types*/
#define MAX_GROUP_SECONDARY_DEVICE_COUNT 2
/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE 1024
/** Maximum number of NoA descriptors */
#define MAX_NOA_DESCRIPTORS 8
/** Maximum number of channel list entries */
#define MAX_CHAN_LIST 8
/** Maximum buffer size for channel entries */
#define MAX_BUFFER_SIZE 64
/** WPS Minimum version number */
#define WPS_MIN_VERSION 0x10
/** WPS Maximum version number */
#define WPS_MAX_VERSION 0x20
/** WPS Minimum request type */
#define WPS_MIN_REQUESTTYPE 0x00
/** WPS Maximum request type */
#define WPS_MAX_REQUESTTYPE 0x04
/** WPS Device Type maximum length */
#define WPS_DEVICE_TYPE_MAX_LEN 8
/** WPS Minimum association state */
#define WPS_MIN_ASSOCIATIONSTATE 0x0000
/** WPS Maximum association state */
#define WPS_MAX_ASSOCIATIONSTATE 0x0004
/** WPS Minimum configuration error */
#define WPS_MIN_CONFIGURATIONERROR 0x0000
/** WPS Maximum configuration error */
#define WPS_MAX_CONFIGURATIONERROR 0x0012
/** WPS Minimum Device password ID */
#define WPS_MIN_DEVICEPASSWORD 0x0000
/** WPS Maximum Device password ID */
#define WPS_MAX_DEVICEPASSWORD 0x000f
/** WPS Model maximum length */
#define WPS_MODEL_MAX_LEN 32
/** WPS Serial maximum length */
#define WPS_SERIAL_MAX_LEN 32
/** WPS Manufacturer maximum length */
#define WPS_MANUFACT_MAX_LEN 64
/** WPS Device Info OUI+Type+SubType Length */
#define WPS_DEVICE_TYPE_LEN 8
/** Max size of custom IE buffer */
#define MAX_SIZE_IE_BUFFER (256)
/** Country string last byte 0x04 */
#define WIFIDIR_COUNTRY_LAST_BYTE 0x04
/** Wifidirect Persistent group record */
#define WIFIDIRECT_PERSISTENT_GROUP_RECORD "hostcmd"
/** Wifidirect params config command */
#define WIFIDIRECT_PARAMS_CONFIG "hostcmd"
/** Wifidirect mode config command */
#define WIFIDIRECT_MODE_CONFIG "hostcmd"
/** Wifidirect action frame command */
#define WIFIDIRECT_ACTION_FRAME "hostcmd"
/** Wifidirect service discovery command */
#define WIFIDIRECT_SERVICE_DISCOVERY "hostcmd"

/** WIFIDIR IE Header */
typedef struct _wifidir_ie_header {
	/** Element ID */
	u8 element_id;
	/** IE Length */
	u8 ie_length;
	/** OUI */
	u8 oui[3];
	/** OUI type */
	u8 oui_type;
	/** IE List of TLV */
	u8 ie_list[0];
} __ATTRIB_PACK__ wifidir_ie_header;

/** IEEEtypes_DsParamSet_t */
typedef struct _IEEE_DsParamSet_t {
	/** DS parameter : Element ID */
	u8 element_id;
	/** DS parameter : Length */
	u8 ie_length;
	/** DS parameter : Current channel */
	u8 current_chan;
} __ATTRIB_PACK__ IEEE_DsParamSet;

/** Event : WIFIDIR event subtype fields */
#ifdef BIG_ENDIAN
typedef struct _wifidir_event_subtype {
	/** Reserved */
	u16 reserved : 11;
	/** Packet Process State */
	u16 pkt_process_state : 3;
	/** Device Role */
	u16 device_role : 2;
} __ATTRIB_PACK__ wifidir_event_subtype;
#else
typedef struct _wifidir_event_subtype {
	/** Device Role */
	u16 device_role : 2;
	/** Packet Process State */
	u16 pkt_process_state : 3;
	/** Reserved */
	u16 reserved : 11;
} __ATTRIB_PACK__ wifidir_event_subtype;
#endif

/** Event : WIFIDIR Generic event */
typedef struct _apeventbuf_wifidir_generic {
	/** Event Length */
	u16 event_length;
	/** Event Type */
	u16 event_type;
	/** Event SubType */
	wifidir_event_subtype event_sub_type;
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** IE List of TLV */
	u8 entire_ie_list[0];
} __ATTRIB_PACK__ apeventbuf_wifidir_generic;

/** TLV buffer : WIFIDIR IE device Id */
typedef struct _tlvbuf_wifidir_device_id {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR device MAC address */
	u8 dev_mac_address[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wifidir_device_id;

/** TLV buffer : WIFIDIR Status */
typedef struct _tlvbuf_wifidir_status {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR status code */
	u8 status_code;
} __ATTRIB_PACK__ tlvbuf_wifidir_status;

/** TLV buffer : WIFIDIR IE capability */
typedef struct _tlvbuf_wifidir_capability {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR device capability */
	u8 dev_capability;
	/** WIFIDIR group capability */
	u8 group_capability;
} __ATTRIB_PACK__ tlvbuf_wifidir_capability;

/** TLV buffer : WIFIDIR extended listen time */
typedef struct _tlvbuf_wifidir_ext_listen_time {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** Availability period */
	u16 availability_period;
	/** Availability interval */
	u16 availability_interval;
} __ATTRIB_PACK__ tlvbuf_wifidir_ext_listen_time;

/** TLV buffer : WIFIDIR Intended Interface Address */
typedef struct _tlvbuf_wifidir_intended_addr {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR Group interface address */
	u8 group_address[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wifidir_intended_addr;

/** TLV buffer : WIFIDIR IE Interface */
typedef struct _tlvbuf_wifidir_interface {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR interface Id */
	u8 interface_id[ETH_ALEN];
	/** WIFIDIR interface count */
	u8 interface_count;
	/** WIFIDIR interface addresslist */
	u8 interface_idlist[0];
} __ATTRIB_PACK__ tlvbuf_wifidir_interface;

/** TLV buffer : WIFIDIR configuration timeout */
typedef struct _tlvbuf_wifidir_config_timeout {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** Group configuration timeout */
	u8 group_config_timeout;
	/** Device configuration timeout */
	u8 device_config_timeout;
} __ATTRIB_PACK__ tlvbuf_wifidir_config_timeout;

/** TLV buffer : WIFIDIR IE Group owner intent */
typedef struct _tlvbuf_wifidir_group_owner_intent {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR device group owner intent */
	u8 dev_intent;
} __ATTRIB_PACK__ tlvbuf_wifidir_group_owner_intent;

/** TLV buffer : WIFIDIR IE channel */
typedef struct _tlvbuf_wifidir_channel {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR country string */
	u8 country_string[3];
	/** WIFIDIR regulatory class */
	u8 regulatory_class;
	/** WIFIDIR channel number */
	u8 channel_number;
} __ATTRIB_PACK__ tlvbuf_wifidir_channel;

/** TLV buffer : WIFIDIR IE channel */
typedef struct _tlvbuf_wifidir_oob_goneg_channel {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR country string */
	u8 country_string[3];
	/** WIFIDIR operating class */
	u8 operating_class;
	/** WIFIDIR channel number */
	u8 channel_number;
	/** WIFIDIR Role indication */
	u8 role_indication;
} __ATTRIB_PACK__ tlvbuf_wifidir_oob_goneg_channel;

/** TLV buffer : WIFIDIR IE manageability */
typedef struct _tlvbuf_wifidir_manageability {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR manageability */
	u8 manageability;
} __ATTRIB_PACK__ tlvbuf_wifidir_manageability;

/** TLV buffer : WIFIDIR IE Invitation Flag */
typedef struct _tlvbuf_wifidir_invitation_flag {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR Invitation Flag */
	u8 invitation_flag;
} __ATTRIB_PACK__ tlvbuf_wifidir_invitation_flag;

/** Channel Entry */
typedef struct _chan_entry {
	/** WIFIDIR regulatory class */
	u8 regulatory_class;
	/** WIFIDIR no of channels */
	u8 num_of_channels;
	/** WIFIDIR channel number */
	u8 chan_list[0];
} __ATTRIB_PACK__ chan_entry;

/** TLV buffer : WIFIDIR IE channel list */
typedef struct _tlvbuf_wifidir_channel_list {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR country string */
	u8 country_string[3];
	/** WIFIDIR channel entry list */
	chan_entry wifidir_chan_entry_list[0];
} __ATTRIB_PACK__ tlvbuf_wifidir_channel_list;

/** NoA Descriptor */
typedef struct _noa_descriptor {
	/** WIFIDIR count OR type */
	u8 count_type;
	/** WIFIDIR duration */
	u32 duration;
	/** WIFIDIR interval */
	u32 interval;
	/** WIFIDIR start time */
	u32 start_time;
} __ATTRIB_PACK__ noa_descriptor;

/** TLV buffer : WIFIDIR IE Notice of Absence */
typedef struct _tlvbuf_wifidir_notice_of_absence {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR NoA Index */
	u8 noa_index;
	/** WIFIDIR CTWindow and OppPS parameters */
	u8 ctwindow_opp_ps;
	/** WIFIDIR NoA Descriptor list */
	noa_descriptor wifidir_noa_descriptor_list[0];
} __ATTRIB_PACK__ tlvbuf_wifidir_notice_of_absence;

/** TLV buffer : WIFIDIR IE group Id */
typedef struct _tlvbuf_wifidir_group_id {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR group MAC address */
	u8 group_address[ETH_ALEN];
	/** WIFIDIR group SSID */
	u8 group_ssid[0];
} __ATTRIB_PACK__ tlvbuf_wifidir_group_id;

/** TLV buffer : WIFIDIR IE group BSS Id */
typedef struct _tlvbuf_wifidir_group_bss_id {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR group Bss Id */
	u8 group_bssid[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wifidir_group_bss_id;

typedef struct _wifidir_device_name_info {
	/** WPS Device Name Tag */
	u16 device_name_type;
	/** WPS Device Name Length */
	u16 device_name_len;
	/** Device name */
	u8 device_name[0];
} __ATTRIB_PACK__ wifidir_device_name_info;

/** TLV buffer : WIFIDIR IE device Info */
typedef struct _tlvbuf_wifidir_device_info {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR device address */
	u8 dev_address[ETH_ALEN];
	/** WPS config methods */
	u16 config_methods;
	/** Primary device type : category */
	u16 primary_category;
	/** Primary device type : OUI */
	u8 primary_oui[4];
	/** Primary device type : sub-category */
	u16 primary_subcategory;
	/** Secondary Device Count */
	u8 secondary_dev_count;
	/** Secondary Device Info */
	u8 secondary_dev_info[0];
	/** Device Name Info */
	wifidir_device_name_info device_name_info;
} __ATTRIB_PACK__ tlvbuf_wifidir_device_info;

typedef struct _wifidir_client_dev_info {
	/** Length of each device */
	u8 dev_length;
	/** WIFIDIR device address */
	u8 wifidir_dev_address[ETH_ALEN];
	/** WIFIDIR Interface  address */
	u8 wifidir_intf_address[ETH_ALEN];
	/** WIFIDIR Device capability*/
	u8 wifidir_dev_capability;
	/** WPS config methods */
	u16 config_methods;
	/** Primary device type : category */
	u16 primary_category;
	/** Primary device type : OUI */
	u8 primary_oui[4];
	/** Primary device type : sub-category */
	u16 primary_subcategory;
	/** Secondary Device Count */
	u8 wifidir_secondary_dev_count;
	/** Secondary Device Info */
	u8 wifidir_secondary_dev_info[0];
	/** WPS WIFIDIR Device Name Tag */
	u16 device_name_type;
	/** WPS WIFIDIR Device Name Length */
	u16 wifidir_device_name_len;
	/** WIFIDIR Device name */
	u8 wifidir_device_name[0];
} __ATTRIB_PACK__ wifidir_client_dev_info;

typedef struct _tlvbuf_wifidir_group_info {
	/** TLV Header tag */
	u8 tag;
	/** TLV Header length */
	u16 length;
	/** Secondary Device Info */
	u8 wifidir_client_dev_list[0];
} __ATTRIB_PACK__ tlvbuf_wifidir_group_info;

typedef struct _tlvbuf_wifidir_params_cfg_group_info {
	/** TLV Header tag */
	u16 tag;
	/** TLV Header length */
	u16 length;
	/** Secondary Device Info */
	tlvbuf_wifidir_group_info data;
} __ATTRIB_PACK__ tlvbuf_wifidir_params_cfg_group_info;

/** TLV buffer : WIFIDIR WPS IE */
typedef struct _tlvbuf_wifidir_wps_ie {
	/** TLV Header tag */
	u16 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR WPS IE data */
	u8 data[0];
} __ATTRIB_PACK__ tlvbuf_wps_ie;

/** TLV buffer : WIFIDIR Provisioning parameters*/
typedef struct _tlvbuf_wifidir_provisioning_params {
	/** TLV Header tag */
	u16 tag;
	/** TLV Header length */
	u16 length;
	/** action */
	u16 action;
	/** config methods */
	u16 config_methods;
	/** dev password */
	u16 dev_password;
} __ATTRIB_PACK__ tlvbuf_wifidir_provisioning_params;

typedef struct _tlvbuf_wifidir_wps_params {
	/** Tag */
	u16 tag;
	/** Length */
	u16 length;
	/** action */
	u16 action;
} __ATTRIB_PACK__ tlvbuf_wifidir_wps_params;

typedef struct _tlvbuf_wifidir_operating_channel {
	/** Tag */
	u16 tag;
	/** Length */
	u16 length;
	/** Operating Channel */
	u8 operating_channel;
} __ATTRIB_PACK__ tlvbuf_wifidir_op_channel;

typedef struct _mrvl_tlvbuf_wifidir_channel {
	/** Tag */
	u16 tag;
	/** Length */
	u16 length;
	/** Country string */
	u8 country_string[3];
	/** Regulatory Class */
	u8 regulatory_class;
	/** Channel Number*/
	u8 channel_number;
} __ATTRIB_PACK__ mrvl_tlvbuf_wifidir_channel;

/** WIFIDIRCMD buffer */
typedef struct _wifidircmdbuf {
	/** Command header */
	mrvl_cmd_head_buf cmd_head;
} __ATTRIB_PACK__ wifidircmdbuf;

/** HostCmd_CMD_WIFIDIR_ACTION_FRAME request */
typedef struct _wifidir_action_frame {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	u8 category;
	/** Action */
	u8 action;
	/** OUI */
	u8 oui[3];
	/** OUI type */
	u8 oui_type;
	/** OUI sub type */
	u8 oui_sub_type;
	/** Dialog taken */
	u8 dialog_token;
	/** IE List of TLVs */
	u8 ie_list[0];
} __ATTRIB_PACK__ wifidir_action_frame;

/** HostCmd_CMD_WIFIDIR_PARAMS_CONFIG struct */
typedef struct _wifidir_params_config {
	/** Action */
	u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	/** TLV data */
	u8 wifidir_tlv[0];
} __ATTRIB_PACK__ wifidir_params_config;

/** HostCmd_CMD_WIFIDIR_MODE_CONFIG structure */
typedef struct _wifidir_mode_config {
	/** Action */
	u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	/** wifidir mode data */
	u16 mode;
} __ATTRIB_PACK__ wifidir_mode_config;

/** Valid Input Commands */
typedef enum {
	SCANCHANNELS,
	CHANNEL,
	WIFIDIR_DEVICECAPABILITY,
	WIFIDIR_GROUPCAPABILITY,
	WIFIDIR_INTENT,
	WIFIDIR_REGULATORYCLASS,
	WIFIDIR_MANAGEABILITY,
	WIFIDIR_INVITATIONFLAG,
	WIFIDIR_COUNTRY,
	WIFIDIR_NO_OF_CHANNELS,
	WIFIDIR_NOA_INDEX,
	WIFIDIR_OPP_PS,
	WIFIDIR_CTWINDOW,
	WIFIDIR_COUNT_TYPE,
	WIFIDIR_DURATION,
	WIFIDIR_INTERVAL,
	WIFIDIR_START_TIME,
	WIFIDIR_PRIDEVTYPECATEGORY,
	WIFIDIR_PRIDEVTYPEOUI,
	WIFIDIR_PRIDEVTYPESUBCATEGORY,
	WIFIDIR_SECONDARYDEVCOUNT,
	WIFIDIR_GROUP_SECONDARYDEVCOUNT,
	WIFIDIR_GROUP_WIFIDIR_DEVICE_NAME,
	WIFIDIR_INTERFACECOUNT,
	WIFIDIR_ATTR_CONFIG_TIMEOUT,
	WIFIDIR_ATTR_EXTENDED_TIME,
	WIFIDIR_WPSCONFMETHODS,
	WIFIDIR_WPSVERSION,
	WIFIDIR_WPSSETUPSTATE,
	WIFIDIR_WPSREQRESPTYPE,
	WIFIDIR_WPSSPECCONFMETHODS,
	WIFIDIR_WPSUUID,
	WIFIDIR_WPSPRIMARYDEVICETYPE,
	WIFIDIR_WPSRFBAND,
	WIFIDIR_WPSASSOCIATIONSTATE,
	WIFIDIR_WPSCONFIGURATIONERROR,
	WIFIDIR_WPSDEVICENAME,
	WIFIDIR_WPSDEVICEPASSWORD,
	WIFIDIR_WPSMANUFACTURER,
	WIFIDIR_WPSMODELNAME,
	WIFIDIR_WPSMODELNUMBER,
	WIFIDIR_WPSSERIALNUMBER,
	WIFIDIR_MINDISCOVERYINT,
	WIFIDIR_MAXDISCOVERYINT,
	WIFIDIR_ENABLE_SCAN,
	WIFIDIR_DEVICE_STATE,
	WIFIDIR_SERVICEUPDATE_INDICATOR,
	WIFIDIR_DISC_SERVICEPROTO,
	WIFIDIR_DISC_SERVICETRANSACID,
	WIFIDIR_DISC_SERVICE_STATUS,
	WIFIDIR_DISC_DNSTYPE,
	WIFIDIR_DISC_BONJOUR_VERSION,
	WIFIDIR_DISC_UPNP_VERSION,
	WIFIDIR_PRESENCE_REQ_TYPE,
	WIFIDIR_CATEGORY,
	WIFIDIR_ACTION,
	WIFIDIR_DIALOGTOKEN,
	WIFIDIR_GAS_COMEBACK_DELAY,
	WIFIDIR_DISC_ADPROTOIE,
	WIFIDIR_DISC_INFOID,
	WIFIDIR_OUI,
	WIFIDIR_OUITYPE,
	WIFIDIR_OUISUBTYPE,
	WIFIDIR_DISC_SERVICE_NAME_LEN,
	WIFIDIR_DISC_SERVICE_NAME,
	WIFIDIR_DISC_SERVICE_INFO_REQ_LEN,
	WIFIDIR_DISC_SERVICE_INFO_REQ,
	//#endif
} valid_inputs;

/** Level of wifidir parameters in the wifidir.conf file */
typedef enum {
	WIFIDIR_PARAMS_CONFIG = 1,
	WIFIDIR_CAPABILITY_CONFIG,
	WIFIDIR_GROUP_OWNER_INTENT_CONFIG,
	WIFIDIR_CHANNEL_CONFIG,
	WIFIDIR_MANAGEABILITY_CONFIG,
	WIFIDIR_INVITATION_FLAG_CONFIG,
	WIFIDIR_CHANNEL_LIST_CONFIG,
	WIFIDIR_NOTICE_OF_ABSENCE,
	WIFIDIR_NOA_DESCRIPTOR,
	WIFIDIR_DEVICE_INFO_CONFIG,
	WIFIDIR_GROUP_INFO_CONFIG,
	WIFIDIR_GROUP_SEC_INFO_CONFIG,
	WIFIDIR_GROUP_CLIENT_INFO_CONFIG,
	WIFIDIR_DEVICE_SEC_INFO_CONFIG,
	WIFIDIR_GROUP_ID_CONFIG,
	WIFIDIR_GROUP_BSS_ID_CONFIG,
	WIFIDIR_DEVICE_ID_CONFIG,
	WIFIDIR_INTERFACE_CONFIG,
	WIFIDIR_TIMEOUT_CONFIG,
	WIFIDIR_EXTENDED_TIME_CONFIG,
	WIFIDIR_INTENDED_ADDR_CONFIG,
	WIFIDIR_OPCHANNEL_CONFIG,
	WIFIDIR_WPSIE,
	WIFIDIR_DISCOVERY_REQUEST_RESPONSE = 0x20,
	WIFIDIR_DISCOVERY_QUERY,
	WIFIDIR_DISCOVERY_SERVICE,
	WIFIDIR_DISCOVERY_VENDOR,
	WIFIDIR_DISCOVERY_QUERY_RESPONSE_PER_PROTOCOL,
	WIFIDIR_EXTRA,
} wifidir_param_level;

/* Needed for passphrase to PSK conversion */
void pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
		 int iterations, u8 *buf, size_t buflen);

typedef struct _wifidir_sd_resp_event {
	unsigned char peer_mac_addr[ETH_ALEN];
	unsigned char service_protocol;
	char service_name[MAX_SERVICE_NAME_LEN];
	char service_info[MAX_SERVICE_INFO_LEN];
} wifidir_sd_resp_event;

/** TLV buffer : persistent group */
typedef struct _tlvbuf_wifidir_persistent_group {
	/** Tag */
	u16 tag;
	/** Length */
	u16 length;
	/** Record Index */
	u8 rec_index;
	/** Device Role */
	u8 dev_role;
	/** wifidir group Bss Id */
	u8 group_bssid[ETH_ALEN];
	/** wifidir device MAC address */
	u8 dev_mac_address[ETH_ALEN];
	/** wifidir group SSID length */
	u8 group_ssid_len;
	/** wifidir group SSID */
	u8 group_ssid[0];
	/** wifidir PSK length */
	u8 psk_len;
	/** wifidir PSK */
	u8 psk[0];
	/** Num of PEER MAC Addresses */
	u8 num_peers;
	/** PEER MAC Addresses List */
	u8 peer_mac_addrs[0][ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wifidir_persistent_group;

/** TLV buffer : WIFIDIR IE capability configuration*/
/* This structure is meant to be used with MRVL_WIFIDIRECT_CAPABILITY_TLV_ID
 * tag */
typedef struct _tlvbuf_wifidir_capability_config {
	/** TLV Header tag */
	u16 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR device capability */
	u8 dev_capability;
	/** WIFIDIR group capability */
	u8 group_capability;
} __ATTRIB_PACK__ tlvbuf_wifidir_capability_config;

typedef struct _tlvbuf_wifidir_connect_removed_persistent_sta {
	/** TLV Header tag */
	u16 tag;
	/** TLV Header length */
	u16 length;
	/** WIFIDIR device capability */
	u8 allow;
} __ATTRIB_PACK__ tlvbuf_wifidir_connect_removed_persistent_sta;

/** TLV buffer : WifiDirect Invitation List */
typedef struct _tlvbuf_wifidirect_invitation_list {
	/** Tag */
	u16 tag;
	/** Length */
	u16 length;
	/** Invitation peer address*/
	u8 inv_peer_addr[ETH_ALEN];
} __ATTRIB_PACK__ tlvbuf_wifidirect_invitation_list;

//#ifdef WFD_SERVICE_DISCOVERY
/** WIFIDIRECTCMD buffer */
typedef struct _wifidirectcmdbuf {
	/** Buf Size */
	u32 buf_size;
	/** Command Code */
	u16 cmd_code;
	/** Size */
	u16 size;
	/** Sequence Number */
	u16 seq_num;
	/** Result */
	s16 result;
} __ATTRIB_PACK__ wifidirectcmdbuf;

typedef struct _wifidirect_anqp_comeback_response_header {
	/** Information identifier */
	u16 info_id;
	/** Response Length */
	u16 response_len;
	/** OUI */
	u8 oui[3];
	/** OUI sub type */
	u8 oui_sub_type;
	/** Service update indicator */
	u16 service_update_indicator;
} __ATTRIB_PACK__ wifidirect_anqp_comeback_response_header;

/** GAS fragment history*/
struct fragment_info {
	/**Peer Mac address of received GAS Request*/
	u8 peer_mac[ETH_ALEN];
	/** Fragment number for fragmentation tracking */
	u8 prev_fragment_number;
	/** Dialogue token of received GAS request */
	u8 dialogue_token;
	/** Service transaction id */
	u8 prev_service_transaction_id;
	/** GAS Response buffer */
	u8 *resp_buf;
	/** Index in the response buffer */
	u16 current_index;
	/** Maximum buffer size */
	u16 max_buffer_size;
};

/** Recv GAS response fragment history*/
struct fragment_info_recv {
	/** Dialogue token used in GAS comeback frames */
	u8 sent_dialogue_token_gas_initiation_req;
	/** ANQP comeback response */
	wifidirect_anqp_comeback_response_header anqp_header;
	/**Received Response Buffer */
	u8 *recv_resp_buf;
	/** Index in the response buffer */
	u16 current_index;
};

/* Maximum number of file categories supported */
#define MAX_FILE_CATEGORIES 3
/* Maximum number of file extensions/types supported for each category */
#define MAX_FILE_TYPES 4
/* Max size of file extension */
#define MAX_FILE_EXT_LEN 8
typedef char fext[MAX_FILE_EXT_LEN];

/** Internal WIFIDIRECT structure for Response Data */
typedef struct wifidirect_sd_response_data {
	union {
		struct wfds_query_response {
			/** Service name length */
			u8 service_name_len;
			/** Service name */
			char service_name[22]; /* @FIXME */
			/** service info req len */
			u16 service_info_len;
			/** service info request */
			u8 service_info[38]; /** File Category */
		} __ATTRIB_PACK__ query_resp_data;
	} u;
} wifidirect_sd_response_data;

/** Internal WIFIDIRECT structure for Query Data */
typedef struct wifidirect_sd_query_data {
	union {
		struct wfds_query_req {
			/** Service name length */
			u8 service_name_len;
			/** Service name */
			char service_name[22]; /* @FIXME */
			/** service info req len */
			u8 service_info_req_len;
			/** service info request */
			u8 service_info_req;
		} __ATTRIB_PACK__ query_data;
	} u;
} wifidirect_sd_query_data;

/** WIFIDIRECT SERVICE DISCOVERY response fragment*/
typedef struct _wifidirect_service_discovery_response_fragment {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	u8 category;
	/** Action */
	u8 action;
	/** Dialog taken */
	u8 dialog_token;
	/** Status code */
	u16 status_code;
	/** GAS comback reply */
	u16 gas_reply;
	/** Advertize protocol IE */
	u8 advertize_protocol_ie[4];
	/** Query response Length */
	u16 query_len;
} __ATTRIB_PACK__ wifidirect_service_discovery_response_fragment;

/** WIFIDIRECT SERVICE DISCOVERY COMEBACK response fragment*/

/** HostCmd_CMD_WIFIDIRECT_SERVICE_DISCOVERY request */
typedef struct _wifidirect_service_discovery_request {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	u8 category;
	/** Action */
	u8 action;
	/** Dialog taken */
	u8 dialog_token;
	/** Advertize protocol IE */
	u8 advertize_protocol_ie[4];
	/** Query request Length */
	u16 query_len;
	/** Information identifier */
	u8 info_id[2];
	/** Request Length */
	u16 request_len;
	/** OUI */
	u8 oui[3];
	/** OUI sub type */
	u8 oui_sub_type;
	/** Service update indicator */
	u16 service_update_indicator;
	/** Vendor Length */
	u16 vendor_len;
	/** Service protocol */
	u8 service_protocol;
	/** Service transaction Id */
	u8 service_transaction_id;
	/** Query Data */
	wifidirect_sd_query_data disc_query;
} __ATTRIB_PACK__ wifidirect_service_discovery_request;

/** HostCmd_CMD_WIFIDIRECT_SERVICE_DISCOVERY response */
typedef struct _wifidirect_service_discovery_response {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	u8 category;
	/** Action */
	u8 action;
	/** Dialog taken */
	u8 dialog_token;
	/** Status code */
	u16 status_code;
	/** GAS comback reply */
	u16 gas_reply;
	/** Advertize protocol IE */
	u8 advertize_protocol_ie[4];
	/** Query response Length */
	u16 query_len;
	/** Information identifier */
	u8 info_id[2];
	/** Response Length */
	u16 response_len;
	/** OUI */
	u8 oui[3];
	/** OUI sub type */
	u8 oui_sub_type;
	/** Service update indicator */
	u16 service_update_indicator;
	/** Vendor Length */
	u16 vendor_len;
	/** Service protocol */
	u8 service_protocol;
	/** Service transaction Id */
	u8 service_transaction_id;
	/** Discovery status code */
	u8 disc_status_code;
	/** Response Data */
	wifidirect_sd_response_data disc_resp;
} __ATTRIB_PACK__ wifidirect_service_discovery_response;

/** WIFIDIRECT_GAS_COMEBACK_SERVICE request */
typedef struct _wifidirect_gas_comeback_request {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** Category */
	u8 category;
	/** Action */
	u8 action;
	/** Dialog taken */
	u8 dialog_token;
} __ATTRIB_PACK__ wifidirect_gas_comeback_request;

/** HostCmd_CMD_WIFIDIRECT_GAS_COMEBACK_SERVICE request */
typedef struct _wifidirect_gas_comeback_request_cmd {
	/** Buf Size */
	u32 buf_size;
	/** Command Code */
	u16 cmd_code;
	/** Size */
	u16 size;
	/** Sequence Number */
	u16 seq_num;
	/** Result */
	s16 result;
	/** GAS comeback req */
	wifidirect_gas_comeback_request gcreq;
} __ATTRIB_PACK__ wifidirect_gas_comeback_request_cmd;

typedef struct _wifidirect_comeback_discovery_response_fragment_header {
	/** Category */
	u8 category;
	/** Action */
	u8 action;
	/** Dialog taken */
	u8 dialog_token;
	/** Status code */
	u16 status_code;
	/** Fragment ID */
	u8 gas_fragment_id;
	/** GAS comback reply */
	u16 gas_reply;
	/** Advertize protocol IE */
	u8 advertize_protocol_ie[4];
	/** Query response Length */
	u16 query_len;
} __ATTRIB_PACK__ wifidirect_comeback_discovery_response_fragment_header;

typedef struct _wifidirect_anqp_tlv_fragment {
	/** Vendor Length */
	u16 vendor_len;
	/** Service protocol */
	u8 service_protocol;
	/** Service transaction Id */
	u8 service_transaction_id;
	/** Discovery status code */
	u8 disc_status_code;
	/** Response Data */
	wifidirect_sd_response_data disc_resp;
} __ATTRIB_PACK__ wifidirect_anqp_tlv_fragment;

/** WIFIDIRECT_GAS_COMEBACK_SERVICE response */

typedef struct _wifidirect_gas_comeback_response {
	/** Peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	/** GAS comeback discovery response fragment header */
	wifidirect_comeback_discovery_response_fragment_header cdr_frag;
	/** ANQP comeback response header */
	wifidirect_anqp_comeback_response_header anqp_header;
	/** ANQP tlv fragment */
	wifidirect_anqp_tlv_fragment anqp_tlv;
} __ATTRIB_PACK__ wifidirect_gas_comeback_response;

/** HostCmd_CMD_WIFIDIRECT_GAS_COMEBACK_SERVICE response */

typedef struct _wifidirect_gas_comeback_response_cmd {
	/** Buf Size */
	u32 buf_size;
	/** Command Code */
	u16 cmd_code;
	/** Size */
	u16 size;
	/** Sequence Number */
	u16 seq_num;
	/** Result */
	s16 result;
	wifidirect_gas_comeback_response gcres;
} __ATTRIB_PACK__ wifidirect_gas_comeback_response_cmd;

/** HostCmd_CMD_WIFIDIRECT_SERVICE_DISCOVERY request */
typedef struct _wifidirect_service_discovery_request_cmd {
	/** Buf Size */
	u32 buf_size;
	/** Command Code */
	u16 cmd_code;
	/** Size */
	u16 size;
	/** Sequence Number */
	u16 seq_num;
	/** Result */
	s16 result;
	/** Service Discovery Req format */
	wifidirect_service_discovery_request sd_req;
} __ATTRIB_PACK__ wifidirect_service_discovery_request_cmd;

/** HostCmd_CMD_WIFIDIRECT_SERVICE_DISCOVERY response */
typedef struct _wifidirect_discovery_request_resp {
	/** Buf Size */
	u32 buf_size;
	/** Command Code */
	u16 cmd_code;
	/** Size */
	u16 size;
	/** Sequence Number */
	u16 seq_num;
	/** Result */
	s16 result;
	union {
		/** Service Discovery Response format */
		wifidirect_service_discovery_response sd_resp;
		/** Service Discovery fragmented Response format */
		wifidirect_service_discovery_response_fragment sd_resp_frag;
	} u;
} __ATTRIB_PACK__ wifidirect_service_discovery_response_cmd;
//#endif
/* Checks a particular input for validatation */
int is_input_valid(valid_inputs cmd, int argc, char *argv[]);

int wifidir_read_ie_buffer(struct mwu_iface_info *cur_if, short ie_index,
			   u16 *mrvl_header_len, u8 *buf);

/** Update persistent group record in the FW */
int wifidir_update_persistent_record(struct mwu_iface_info *cur_if);

/** Return index of persistent group record */
int wifidir_get_persistent_rec_ind_by_ssid(struct mwu_iface_info *cur_if,
					   int ssid_length, u8 *ssid,
					   s8 *pr_index);
#if 0
/** Delete specified presistent group record from the FW storage */
int wifidir_do_delete_persistent_record(struct mwu_iface_info *cur_if,
        wifidir_persistent_record *record);

/** Insert a presistent group record */
int wifidir_do_insert_persistent_record(struct mwu_iface_info *cur_if,
        wifidir_persistent_record *prec);
#endif
#endif /* _WLAN_WIFIDIR_H_ */
